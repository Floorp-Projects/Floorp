/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* style struck used to hold style associations
 *
 * Designed and Implemented by Lou Montulli '97
 */

#include "xp.h"
#include "stystruc.h"

/* class to hold style information between style sheet parser
 * and layout engine
 */

#define INCREASE_ARRAY_BY 5
#define INITIAL_ARRAY_SIZE 10

typedef enum {
	CHAR_VALUE,
	SS_NUM_VALUE
} ss_pair_type;

typedef struct _ss_pair {
	char         *name;
	ss_pair_type  value_type;
	void         *value;
	int32		  priority;
} ss_pair;

typedef struct _SS_StyleStruct {
    StyleStructInterface  *vtable;
    int32                  refcount;

    /* private data */
    ss_pair ** pair_array;
    int32      pair_array_size;
    int32      pair_array_first_unused_index;

} SS_StyleStruct;

/* 
 * allocate SS_StyleStructs from a pool
 */
static XP_AllocStructInfo SS_StyleStructAlloc =
  { XP_INITIALIZE_ALLOCSTRUCTINFO(sizeof(SS_StyleStruct)) };

void
SS_freeSSNumber(SS_StyleStruct *self, SS_Number *obj)
{
	if(!obj)
		return;

	XP_FREEIF(obj->units);
	XP_FREE(obj);
}

SS_Number *
SS_newSSNumber(SS_StyleStruct *self, double value, char *units)
{
    SS_Number *new_num;

	if(!units)
	{
		XP_ASSERT(0);
		return NULL;	
	}

    /* alloc an ss_num */
    new_num = XP_CALLOC(1, sizeof(SS_Number));
    if(!new_num)
        return NULL;

    new_num->value = value;
    new_num->units = XP_STRDUP(units);

	if(!new_num->units)
	{
		XP_FREE(new_num);
		return NULL;
	}

	return(new_num);
}

SS_Number *
SS_copySSNumber(SS_StyleStruct *self, SS_Number *old_num)
{
	if(!old_num)
	{
		XP_ASSERT(0);
		return NULL;	
	}

	return(SS_newSSNumber(self, old_num->value, old_num->units));
}

void
ss_expand_array(SS_StyleStruct *self)
{
	if(self->pair_array)
	{
		self->pair_array_size += INCREASE_ARRAY_BY;
		self->pair_array = XP_REALLOC(self->pair_array, 
									  self->pair_array_size*sizeof(ss_pair*));
	}
	else
	{
		self->pair_array_size = INITIAL_ARRAY_SIZE;
		self->pair_array = XP_CALLOC(self->pair_array_size, sizeof(ss_pair*));
	}

	if(!self->pair_array)
	  {
		self->pair_array_first_unused_index = 0;
		self->pair_array_size = 0;
	  }
}

void 
ss_free_ss_pair(SS_StyleStruct *self, ss_pair *pair)
{
	if(pair)
	{
		XP_FREEIF(pair->name);
		switch(pair->value_type)
		{
			case CHAR_VALUE:
				XP_FREEIF(pair->value);
				break;

			case SS_NUM_VALUE:
				SS_freeSSNumber(self, (SS_Number*)pair->value);
				break;

			default:
				XP_ASSERT(0);
		}

		XP_FREE(pair);
	}
}

ss_pair *
ss_find_pair(SS_StyleStruct *self, char *name)
{
	int i;

	for(i = 0; i < self->pair_array_first_unused_index; i++)
	{
		if(!strcasecomp(self->pair_array[i]->name, name))
			return(self->pair_array[i]);
	}

	return NULL;
}

int32 
ss_find_pair_index(SS_StyleStruct *self, ss_pair *pair)
{
	int i;

	for(i = 0; i < self->pair_array_first_unused_index; i++)
	{
		if((self->pair_array[i] == pair))
			return(i);
	}

	return -1;
}

/* remove the pair from the pair array and
 * free's the pair if found.
 */
void
ss_delete_pair(SS_StyleStruct *self, ss_pair *pair)
{
	/* find the pair index */	
	int32 index = ss_find_pair_index(self, pair);

	if(index > -1)
	{
		ss_free_ss_pair(self, self->pair_array[index]);
	
		/* shift the array back over the deleted pair */
		if(index < self->pair_array_first_unused_index)
		{
			int32 move_size = (self->pair_array_first_unused_index-index)-1;
			move_size *= sizeof(ss_pair*);
			if(move_size)
				XP_MEMMOVE(&self->pair_array[index], &self->pair_array[index+1], move_size);
			self->pair_array_first_unused_index--;
		}
	}
	else
	{
		/* cant find pair */
		XP_ASSERT(0);
	}

}

void
ss_add_to_array(SS_StyleStruct *self, ss_pair *pair)
{
	if(!pair || !pair->name)
	{
		XP_ASSERT(0);
		return;
	}

	if(self->pair_array_size <= self->pair_array_first_unused_index)
		ss_expand_array(self);
	
	if(self->pair_array)
	{
#if DEBUG
		/* check for dups */
		ss_pair *dup = ss_find_pair(self, pair->name);

		if(dup)
			XP_ASSERT(0);
#endif

		self->pair_array[self->pair_array_first_unused_index++] = pair;
	}
	else
	{
		ss_free_ss_pair(self, pair);
	}
}

void
ss_free_pair_array(SS_StyleStruct *self)
{
    int i;

	if(self->pair_array)
	{
    	for(i = 0; i < self->pair_array_first_unused_index; i++)
    	{
        	ss_free_ss_pair(self, self->pair_array[i]);
    	}

		XP_FREE(self->pair_array);
	}
		
	self->pair_array_size = 0;
	self->pair_array_first_unused_index = 0;
}

void
SS_setString(SS_StyleStruct *self, char *name, char *value, int32 priority)
{
	ss_pair *new_pair;
	ss_pair *dup;

	/* check for dups */
	dup = ss_find_pair(self, name);

	if(dup)
	{
		if(dup->priority <= priority)
			ss_delete_pair(self, dup);
		else
			return;  /* ignore this set call */
	}

	/* alloc a pair */
	new_pair = XP_CALLOC(1, sizeof(ss_pair));

	if(new_pair)
	{
		new_pair->name = XP_STRDUP(name);
		new_pair->value_type = CHAR_VALUE;
		new_pair->value = XP_STRDUP(value);
		new_pair->priority = priority;

		if(!new_pair->name || !new_pair->value)
		{
			/* malloc error */
			XP_FREE(new_pair);
			XP_FREEIF(new_pair->name);
			XP_FREEIF(new_pair->value);
			return;
		}

		ss_add_to_array(self, new_pair);
	}
}

void
SS_setNumber(SS_StyleStruct *self, char *name, SS_Number *value, int32 priority)
{
	SS_Number *ss_num;
	ss_pair *new_pair;
	ss_pair *dup;

	/* check for dups */
	dup = ss_find_pair(self, name);

	if(dup)
	{
		if(dup->priority <= priority)
			ss_delete_pair(self, dup);
		else
			return;  /* ignore this set call */
	}

	/* alloc a pair */
	new_pair = XP_CALLOC(1, sizeof(ss_pair));

    if(new_pair)
    {
		/* alloc an ss_num */
		ss_num = SS_copySSNumber(self, value);

        new_pair->name = XP_STRDUP(name);
        new_pair->value_type = SS_NUM_VALUE;
		new_pair->value = ss_num;
		new_pair->priority = priority;

        if(!new_pair->name || !new_pair->value)
        {
            /* malloc error */
            XP_FREE(new_pair);
            XP_FREEIF(new_pair->name);
            SS_freeSSNumber(self, new_pair->value);
			
            return;
        }

        ss_add_to_array(self, new_pair);
    }
}

char *
SS_getString(SS_StyleStruct *self, char *name)
{
	ss_pair *pair = ss_find_pair(self, name);

	if(pair && pair->value)
	{
		if(pair->value_type == CHAR_VALUE)
		{
			return(XP_STRDUP((char*)pair->value));
		}
		else if(pair->value_type == SS_NUM_VALUE)
		{
			SS_Number *ss_num = (SS_Number*)pair->value;
			char *rv = PR_smprintf("%f", ss_num->value);
			StrAllocCat(rv, ss_num->units);

			return(rv);
		}
		else
		{
			XP_ASSERT(0);
		}
	}

	return(NULL);
}

SS_Number *
SS_stringToSSNumber(SS_StyleStruct *self, char *num_string)
{
	char *ptr, *num_ptr;
	double num_val;

	ptr = num_string;

	/* skip any whitespace */
	while(XP_IS_SPACE(*ptr)) ptr++;

	/* save a pointer to the first non white char */
	num_ptr = ptr;

	/* go past any sign in front of the number */
	if(*ptr == '-' || *ptr == '+') ptr++;

	/* go forward until a non number is encountered */
	while(XP_IS_DIGIT(*ptr)) ptr++;

	/* go past a decimal */
	if(*ptr == '.') ptr++;

	while(XP_IS_DIGIT(*ptr)) ptr++;

	/* skip any whitespace between the number and units */
	while(XP_IS_SPACE(*ptr)) ptr++;

	/* 
         * no need to clear out the string at the end since
	 * atof will do that for us, and writting to the string
	 * will make us crash
	 *
	 * ptr_value = *ptr;
	 * *ptr = '\0';
 	 * *ptr = ptr_value;	
	 */
	num_val = atof(num_ptr);

	return(SS_newSSNumber(self, num_val, ptr));
}

SS_Number *
SS_getNumber(SS_StyleStruct *self, char *name)
{
	ss_pair *pair = ss_find_pair(self, name);

    if(pair && pair->value)
    {
        if(pair->value_type == CHAR_VALUE)
        {
			return SS_stringToSSNumber(self, pair->value);
        }
        else if(pair->value_type == SS_NUM_VALUE)
        {
            return(SS_copySSNumber(self, (SS_Number*)pair->value));
        }
        else
        {
            XP_ASSERT(0);
        }
    }

    return(NULL);

}

uint32
SS_count(SS_StyleStruct *self)
{
   return self->pair_array_first_unused_index;
}

StyleStruct *
SS_duplicate(SS_StyleStruct *self)
{
	StyleStruct *new_ss = STYLESTRUCT_Factory_Create();
	int i;

	if(!new_ss)
		return NULL;

	/* add all the elements of the current struct to the new one */
	for(i = 0; i < self->pair_array_first_unused_index; i++)
	{
		ss_pair *pair = self->pair_array[i];	

		switch(pair->value_type)
		{
			case CHAR_VALUE:
				STYLESTRUCT_SetString(new_ss, pair->name, (char *)pair->value, pair->priority);
				break;

			case SS_NUM_VALUE:
				STYLESTRUCT_SetNumber(new_ss, pair->name, (SS_Number*)pair->value, pair->priority);
				break;
			default:
				XP_ASSERT(0);
		}
	}

	return new_ss;
}

void
SS_delete(SS_StyleStruct *self)
{
	self->refcount--;

	if(self->refcount > 0)
		return;

	ss_free_pair_array(self);

#ifdef DEBUG
	/* memset the struct so that any free memory read
	 * errors are immediately detectable 
	 */
	XP_MEMSET(self, 0, sizeof(SS_StyleStruct));
#endif /* DEBUG */
	
	/*
	 *return SS_StyleStruct to the pool
	 */
	XP_FreeStruct(&SS_StyleStructAlloc, self); 
	self = NULL;
}

/*****************************************************
 * class symantics
 */

/* static vtable */
const StyleStructInterface StyleStruct_interface = {

	(SS_Number* (*)(StyleStruct *self, double value, char *units))
		SS_newSSNumber,
    (void (*)(StyleStruct * self, SS_Number *obj))
		SS_freeSSNumber,
	(SS_Number* (*)(StyleStruct *self, SS_Number *old_num))
		SS_copySSNumber,
    (SS_Number* (*)(StyleStruct *self, char *strng))
		SS_stringToSSNumber,
	(void (*)(StyleStruct * self, char *name, char *value, int32 priority))
		SS_setString,
    (void (*)(StyleStruct * self, char *name, SS_Number *value, int32 priority))
		SS_setNumber,
    (char* (*)(StyleStruct * self, char *name))
		SS_getString,
    (SS_Number* (*)(StyleStruct * self, char *name))
		SS_getNumber,
    (uint32 (*)(StyleStruct * self))
		SS_count,
    (StyleStruct * (*)(StyleStruct * self))
		SS_duplicate,
    (void (*)(StyleStruct * self))
		SS_delete,
};


StyleStruct *
STYLESTRUCT_Factory_Create(void)
{
    /* initializer */
	/*
	 * allocate SS_StyleStruct from a pool
	 */
	SS_StyleStruct *self = (SS_StyleStruct*) XP_AllocStructZero(&SS_StyleStructAlloc); 
	
	if(!self)
		return NULL;
	
	self->vtable = (void*)&StyleStruct_interface;
	self->refcount = 1;

	return (StyleStruct*)self;
}

#ifdef SS_TEST

typedef struct {
	char name[100];
	char value[100];
	char units[100];
	ss_pair_type type;
} test_struct;

test_struct test_table[] = {

{"numone", "1", "pts", SS_NUM_VALUE},
{"numtwo", "2", "pts", SS_NUM_VALUE},
{"numthree", "3", "pts", SS_NUM_VALUE},
{"numfour", "4", "pts", SS_NUM_VALUE},
{"numfive", "5", "pts", SS_NUM_VALUE},
{"numsix", "6", "pts", SS_NUM_VALUE},
{"numseven", "7", "pts", SS_NUM_VALUE},
{"numeight", "8", "pts", SS_NUM_VALUE},
{"numnine", "9", "pts", SS_NUM_VALUE},
{"numten", "10", "pts", SS_NUM_VALUE},
{"numeleven", "11", "pts", SS_NUM_VALUE},
{"numtwelve", "12", "pts", SS_NUM_VALUE},
{"numthirteen plus spaces", "13", "pts", SS_NUM_VALUE},
{"numfive hundred thowsand", "500000", "pts", SS_NUM_VALUE},

{"strone", "strone", "", CHAR_VALUE},
{"strtwo", "strtwo", "", CHAR_VALUE},
{"strthree", "strthree", "", CHAR_VALUE},
{"strfour", "strfour", "", CHAR_VALUE},
{"strfive", "strfive", "", CHAR_VALUE},
{"strsix", "strsix", "", CHAR_VALUE},
{"strseven", "strseven", "", CHAR_VALUE},

{0, 0, SS_NUM_VALUE}

};

void
test_values(StyleStruct *h)
{
	int i;
	char *ptr;
	SS_Number *ss;
	char buf[200];

    for(i=0; *test_table[i].name; i++)
    {
        test_struct *ts = &test_table[i];

		printf("testing name: %s\n", ts->name);

        switch(ts->type)
        {
            case CHAR_VALUE:
                ptr = STYLESTRUCT_GetString(h, ts->name);

				if(!ptr)
				{
					printf("Error: value not found for name: %s\n", ts->name);
				}
				else if(strcmp(ptr, ts->value))
				{
					printf("Error: value does not match, old: %s new: %s\n", ts->value, ptr);
					XP_FREE(ptr);
				}
                break;

            case SS_NUM_VALUE:
				/* get as string */
                ptr = STYLESTRUCT_GetString(h, ts->name);

				if(!ptr)
				{
					printf("Error: value not found for name: %s\n", ts->name);
				}
				else
				{
                	strcpy(buf, ts->value);
                	strcat(buf, ts->units);
					if(strcmp(ptr, buf))
						printf("Error: value does not match, old: %s new: %s\n", buf, ptr);
					XP_FREE(ptr);
				}

				/* get as number */
                ss = STYLESTRUCT_GetNumber(h, ts->name);
				if(!ss)
				{
					printf("Error: value not found for name: %s\n", ts->name);
				}
				else
				{
					if(ss->value != atof(ts->value))
						printf("Error: value does not match, old: %s new: %d\n", 
								ts->value, ss->value);

					
					if(strcmp(ss->units, ts->units))
						printf("Error: value does not match, old: %s new: %s\n", 
								ts->units, ss->units);

					STYLESTRUCT_FreeSSNumber(h, ss);
				}
				
                break;

            default:
                XP_ASSERT(0);
                break;
        }
    }


	/* test for some names that dont exist */ 
    ptr = STYLESTRUCT_GetString(h, "DOESN'T EXIST");
	if(ptr)
	{
		printf("Error: returned value should not have been found");
		XP_FREE(ptr);
	}
    ss = STYLESTRUCT_GetNumber(h, "THIS NAME DOES NOT EXIST");
	if(ss)
	{
		printf("Error: returned value should not have been found");
		STYLESTRUCT_FreeSSNumber(h, ss);
	}

}

int
main(int argc, char *argv[])
{
	int i;
	StyleStruct *h;
	char buf[200];
	SS_Number *ss_num;
	StyleStruct *new_ss;

	h = STYLESTRUCT_Factory_Create();

	if(!h)
		exit(1);

	/* add everything as strings */
	for(i=0; *test_table[i].name; i++)
	{
		test_struct *ts = &test_table[i];

		switch(ts->type)
		{
			case CHAR_VALUE:
				STYLESTRUCT_SetString(h, ts->name, ts->value);
				break;

			case SS_NUM_VALUE:
				strcpy(buf, ts->value);
				strcat(buf, ts->units);
				STYLESTRUCT_SetString(h, ts->name, buf);
				break;

			default:
				XP_ASSERT(0);
				break;
		}
	}

	test_values(h);

    /* add strings and numbers */
    for(i=0; *test_table[i].name; i++)
    {
        test_struct *ts = &test_table[i];

        switch(ts->type)
        {
            case CHAR_VALUE:
                STYLESTRUCT_SetString(h, ts->name, ts->value);
                break;

            case SS_NUM_VALUE:
				ss_num = STYLESTRUCT_NewSSNumber(h, atol(ts->value), ts->units);
                STYLESTRUCT_SetNumber(h, ts->name, ss_num, 0);
				STYLESTRUCT_FreeSSNumber(h, ss_num);
                break;

            default:
                XP_ASSERT(0);
                break;
        }
    }

	test_values(h);

	/* dup the class */

	new_ss = STYLESTRUCT_Duplicate(h);
	STYLESTRUCT_Delete(h);

	test_values(new_ss);

	STYLESTRUCT_Delete(new_ss);

	printf("all tests passed\n\n");
}

#endif /* TEST_SS */
