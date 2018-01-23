#include "afi.h"
#include <string.h>

void do_nop(afi_Entry *self, afi_State *state) {
	return;
}

void docol(afi_Entry *self, afi_State *state) {
	for(int i=0; i<self->numwords; i++) {
		afi_Entry *curr_word = self->words[i];
		switch(curr_word->type) {
		case AFI_T_LITR:
			PUSH(state->args,curr_word->literal);
			break;
		case AFI_T_PRIM:
			curr_word->codeword(curr_word,state);
			break;
		case AFI_T_COMP:
			docol(curr_word,state);
			break;
		case AFI_T_UBRN:
			i += curr_word->offset;
			break;
		case AFI_T_CBRN:
			if(POP(state->args) == 0) i+= curr_word->offset;
			break;
		default:
			break;
		}
	}
}

int afi_exec(afi_State *state, const char *buf, size_t buflen) {
	char toks[buflen];
	size_t num_toks = 0;
	int word_start = 0;
	int word_end = 0;
	for(int wr = 0, rd=0; rd<buflen+1; rd++) // Parse and tokenize.
	{
		if((buf[rd] == ' ' || buf[rd] == 0)) // If whitespace found
		{
			if(word_end == rd) // If first whitespace after word end
			{
				toks[wr++] = '\0'; // Mark end of token.
				num_toks += 1;
			}
			word_start = rd+1;
		}
		else // If non-whitespace.
		{
			if(rd - word_start >= AFI_NAME_LEN) // If word is too long.
			{
				return rd;
			}
			toks[wr++] = buf[rd];
			word_end = rd+1;
		}
	}
	if(toks[0] == '\0') return 0; // Trivial success if empty string.

	afi_Stack *branches = afi_newStack(num_toks);
	afi_Entry *line_word = afi_defCompound("", num_toks);
	char *cur_tok = toks;
	for(afi_int_t t = 0; t < num_toks; t++) // Symbol Lookup
	{
		afi_Entry *word = afi_find(state->dict, cur_tok);
		if(strncmp(cur_tok,"BRANCH",6) == 0)
		{
			word = afi_defBranch(strtol(cur_tok+6,NULL,10),false);
			state->dict = afi_addEntry(state->dict, word);
		}
		else if(strncmp(cur_tok,"ZBRANCH",7) == 0)
		{
			word = afi_defBranch(strtol(cur_tok+7,NULL,10),true);
			state->dict = afi_addEntry(state->dict, word);
		}
		else if(strcmp(cur_tok,"IF") == 0)
		{
			// Push location of IF onto stack.
			PUSH(branches, t);
			// Insert placeholder cond. branch in line_word.
			word = afi_defBranch(0,true);
			state->dict = afi_addEntry(state->dict, word);
		}
		else if(strcmp(cur_tok,"ELSE") == 0)
		{
			// Modify placeholder cond. branch to have proper offset.
			if(SIZE(branches) < 1)
			{
				free(line_word);
				free(branches);
				return -(t+1);
			}
			afi_int_t if_loc = POP(branches);
			line_word->words[if_loc]->offset = t - if_loc;
			// Push location of ELSE onto stack.
			PUSH(branches, t);
			// Insert placeholder uncond. branch in line_word.
			word = afi_defBranch(0,false);
			state->dict = afi_addEntry(state->dict, word);
		}
		else if(strcmp(cur_tok,"END") == 0)
		{
			// Modify placeholder cond. branch to have proper offset.
			if(SIZE(branches) < 1)
			{
				free(line_word);
				free(branches);
				return -(t+1);
			}
			afi_int_t else_loc = POP(branches);
			line_word->words[else_loc]->offset = t - else_loc;
			// Insert NOP to keep token number consistent.
			word = afi_find(state->dict,"NOP");
		}
		else if(word == NULL)
		{
			char *end;
			afi_int_t literal = strtol(cur_tok,&end,10);
			if(end == cur_tok) // If invalid token
			{
				free(line_word);
				free(branches);
				return -(t+1);
			}
			word = afi_defLiteral(literal);
			state->dict = afi_addEntry(state->dict, word);
		}
		line_word->words[t] = word;
		cur_tok += strlen(cur_tok) + 1;
	}
	if(SIZE(branches) > 0) // If invalid token
	{
		free(line_word);
		free(branches);
		return -(num_toks+1);
	}
	free(branches);

	docol(line_word, state); // Execute
	free(line_word);
	return 0;
}

afi_Entry *afi_find(afi_Node *dict, const char *name) {
	afi_Node *curr_node = dict;
	while(curr_node->entry != NULL)
	{
		afi_Entry *curr_entry = curr_node->entry;
		if(curr_entry->type == AFI_T_PRIM || curr_entry->type == AFI_T_COMP)
		{
			if(strncmp(curr_entry->name,name,AFI_NAME_LEN) == 0)
			{
				return curr_entry;
			}
		}
		curr_node = curr_node->prev;
	}
	return NULL;
}

afi_Entry *afi_defPrimitive(const char *name, afi_Word *codeword) {
	// malloc entry
	afi_Entry *entry = malloc(sizeof(afi_Entry));

	// Copy word name.
	strncpy(entry->name, name, AFI_NAME_LEN);

	entry->type = AFI_T_PRIM;

	// Set codeword pointer.
	entry->codeword = codeword;

	return entry;
}

afi_Entry *afi_defCompound(const char *name, size_t num_words) {
	// malloc entry header + array of words
	afi_Entry *entry = malloc(sizeof(afi_Entry) + (sizeof(afi_Entry*) * num_words));

	// Copy word name.
	strncpy(entry->name, name, AFI_NAME_LEN);

	entry->numwords = num_words;

	// Set codeword type.
	entry->type = AFI_T_COMP;

	return entry;
}

afi_Entry *afi_defBranch(size_t offset, bool cond) {
	// malloc entry header
	afi_Entry *entry = malloc(sizeof(afi_Entry));

	if(cond)
	{
		entry->type = AFI_T_CBRN;
	}
	else
	{
		entry->type = AFI_T_UBRN;
	}

	entry->offset = offset;

	return entry;
}

afi_Entry *afi_defLiteral(afi_int_t literal) {
	// malloc entry header
	afi_Entry *entry = malloc(sizeof(afi_Entry));

	entry->type = AFI_T_LITR;

	entry->literal = literal;

	return entry;
}

afi_Node *afi_addEntry(afi_Node *dict, afi_Entry *entry){
	afi_Node *head = malloc(sizeof(afi_Node));
	head->prev = dict;
	head->entry = entry;
	return head;
}

afi_Node *afi_newNode() {
	afi_Node *head = malloc(sizeof(afi_Node));
	head->prev = NULL;
	head->entry = NULL;
	return head;
}

afi_Stack *afi_newStack(size_t stack_size) {
	afi_Stack *stack = malloc(sizeof(afi_Stack) + sizeof(afi_int_t) * stack_size);
	stack->top = stack->data;
	stack->size = stack_size;
	return stack;
}

afi_State *afi_initState(size_t stack_size) {
	afi_State *state = malloc(sizeof(afi_State));
	state->args = afi_newStack(stack_size);
	state->dict = afi_newNode();
	afi_Entry *nop = afi_defPrimitive("NOP",do_nop);
	state->dict = afi_addEntry(state->dict, nop);
	return state;
}

void afi_freeState(afi_State *state) {
	free(state->args);
	afi_freeDict(state->dict);
	free(state);
}

void afi_freeDict(afi_Node *head) {
	while(head != NULL)
	{
		afi_Node *curr = head;
		head = head->prev;
		free(curr->entry);
		free(curr);
	}
}
