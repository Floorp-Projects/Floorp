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

/* styleStack used to hold a stack of tags and associated styles
 *
 * Designed and Implemented by Lou Montulli '97
 */


#include "xp.h"
#include "libmocha.h"
#include "stystruc.h"
#include "stystack.h"
#include "jsspriv.h"

/* simple stack implementation for style/tag stack 
 * why don't we have an XP Stack?
 */
#define INITIAL_STACK_SIZE 10
#define INCREASE_STACK_BY  5

typedef struct {

	StyleStruct *style;
	TagStruct   *tag;

} TagAndStyleAssoc;

typedef struct _SML_StyleAndTagStack {
    StyleAndTagStackInterface  *vtable;
    int32                       refcount;

    /* private data */
	XP_Bool 		   save_stack;
	TagAndStyleAssoc **tag_stack;
	int32              tag_stack_size;
	int32              tag_stack_first_unused_index;

	MWContext         *context;
	StyleObject		  *tags;
	StyleObject		  *classes;
	StyleObject		  *ids;

} SML_StyleAndTagStack;

/* specify whether to keep the stack or not.
 * when the stack is OFF (as it is at initalization)
 * adding to the stack is a noop
 */
void
SML_SetSaveOn(SML_StyleAndTagStack *self, XP_Bool on_flag)
{
	self->save_stack = on_flag;
}

XP_Bool
SML_IsSaveOn(SML_StyleAndTagStack *self)
{
	return self->save_stack;
}

/* 
 * allocate TagStrucs from a pool to speed up
 * the application
 */
static XP_AllocStructInfo TagStructAlloc =
  { XP_INITIALIZE_ALLOCSTRUCTINFO(sizeof(TagStruct)) };

TagStruct *
SML_NewTagStruct(SML_StyleAndTagStack *self, char *name, char *class_name, char *id)
{
	TagStruct *tag;

	if(!name)
		return NULL;

	/* 
	 * allocate TagStrucs from a pool to speed up
	 * the application
	 */
	tag = (TagStruct*) XP_AllocStructZero(&TagStructAlloc); 
	if(!tag)
		return(NULL);

	
	tag->name = name;
	if(class_name)
		tag->class_name = class_name;
	if(id)
		tag->id = id;

	return(tag);
}

void
SML_FreeTagStruct(SML_StyleAndTagStack *self, TagStruct *tag)
{
	if(!tag)
		return;

	XP_FREEIF(tag->name);
	XP_FREEIF(tag->class_name);
	XP_FREEIF(tag->id);
	/* 
	 * return the TagStruct to the pool
	 */
	XP_FreeStruct(&TagStructAlloc, tag); 
	tag = NULL; 
}


/* 
 * allocate TagAndStyleAssoc from a pool to speed up
 * the application
 */
static XP_AllocStructInfo TagAndStyleAssocAlloc =
  { XP_INITIALIZE_ALLOCSTRUCTINFO(sizeof(TagAndStyleAssoc)) };

TagAndStyleAssoc *
sml_new_assoc(SML_StyleAndTagStack *self, TagStruct *tag, StyleStruct *style)
{
	TagAndStyleAssoc *new_assoc;

	if(!tag || !style)
	{
		XP_ASSERT(0);
		return NULL;
	}

	/* 
	 * allocate TagAndStyleAssoc from a pool to speed up
	 * the application
	 */
	new_assoc = (TagAndStyleAssoc*) XP_AllocStructZero(&TagAndStyleAssocAlloc);

	if(!new_assoc)
		return NULL;

	new_assoc->tag = tag;
	new_assoc->style = style;

	return new_assoc;
}

void
sml_free_TagAndStyleAssoc(SML_StyleAndTagStack *self,TagAndStyleAssoc *assoc)
{
	if(!assoc)	
		return;

	SML_FreeTagStruct(self, assoc->tag);
	STYLESTRUCT_Delete(assoc->style);

#ifdef DEBUG
	assoc->tag = (TagStruct   *)-1;
	assoc->style = (StyleStruct *)-1;
#endif 

	/* 
	 * return the TagAndStyleAssoc to the pool
	 */
	XP_FreeStruct(&TagAndStyleAssocAlloc, assoc); 
	assoc = NULL; 
}

void
sml_free_stack(SML_StyleAndTagStack *self)
{
	int i;

	if(self->tag_stack)
	{
		for(i = 0; i < self->tag_stack_first_unused_index; i++)
		{
			sml_free_TagAndStyleAssoc(self, self->tag_stack[i]);
		}
	
		XP_FREE(self->tag_stack);
	}
                
	self->tag_stack_size = 0;
	self->tag_stack_first_unused_index = 0;
	self->tag_stack = NULL;

}


void
sml_expand_stack(SML_StyleAndTagStack *self)
{
    if(self->tag_stack)
    {
        self->tag_stack_size += INCREASE_STACK_BY;
        self->tag_stack = XP_REALLOC(self->tag_stack,
                                      self->tag_stack_size*sizeof(TagAndStyleAssoc*));
    }
    else
    {
        self->tag_stack_size = INITIAL_STACK_SIZE;
        self->tag_stack = XP_CALLOC(self->tag_stack_size, sizeof(TagAndStyleAssoc*));
    }

    if(!self->tag_stack)
      {
        self->tag_stack_first_unused_index = 0;
        self->tag_stack_size = 0;
      }
}

void
ss_add_to_stack(SML_StyleAndTagStack *self, TagAndStyleAssoc *pair)
{
    if(self->tag_stack_size <= self->tag_stack_first_unused_index)
        sml_expand_stack(self);

    if(self->tag_stack)
    {
        self->tag_stack[self->tag_stack_first_unused_index++] = pair;
    }
    else
    {
        sml_free_TagAndStyleAssoc(self, pair);
    }
}


PushTagStatus 
SML_PushTag(SML_StyleAndTagStack *self, char *name, char *class_name, char *id)
{
	TagStruct *new_tag;
	StyleStruct *ss;
	TagAndStyleAssoc *assoc;

	if(!self->save_stack)
		return PUSH_TAG_ERROR;

	new_tag = SML_NewTagStruct(self, name, class_name, id);
	if(!new_tag)
		return PUSH_TAG_ERROR;

 	ss = STYLESTRUCT_Factory_Create();

	if(!ss)
	{
		SML_FreeTagStruct(self, new_tag);
		return PUSH_TAG_ERROR;
	}

	assoc = sml_new_assoc(self, new_tag, ss);
	
	if(!assoc)
	{
		SML_FreeTagStruct(self, new_tag);
		STYLESTRUCT_Delete(ss);
		return PUSH_TAG_ERROR;
	}
	
	ss_add_to_stack(self, assoc);	

	/* add call to style sheet parser here to fill in style struct */
	jss_GetStyleForTopTag((StyleAndTagStack*)self);

	return(PUSH_TAG_SUCCESS);
}

/* pop a tag from within the stack
 * The stack is indexed from the top element.
 * the zero'th element is the top of the stack
 */
void 
SML_PopTagByIndex(SML_StyleAndTagStack *self, char *name, int32 index)
{

	TagAndStyleAssoc *assoc;
	/* index from the top of the stack */
	int32 real_index = self->tag_stack_first_unused_index - 1 - index;

	if(self->tag_stack_first_unused_index < real_index+1 || real_index < 0)
		return;

	assoc = self->tag_stack[real_index];
	sml_free_TagAndStyleAssoc(self, assoc);

	/* shift the stack down */
	if(real_index < self->tag_stack_first_unused_index-1)
		XP_MEMMOVE(&self->tag_stack[real_index],
			  &self->tag_stack[real_index+1],
			  index*sizeof(TagAndStyleAssoc*));

	/* zero last unused index for debugging purposes */
	self->tag_stack_first_unused_index--;
	self->tag_stack[self->tag_stack_first_unused_index] = 0;
}

void 
SML_PopTag(SML_StyleAndTagStack *self, char *name)
{
	SML_PopTagByIndex(self, name, 0);
}

/* the zero'th index is the top of the stack 
 */
TagAndStyleAssoc *
sml_get_index(SML_StyleAndTagStack *self, int32 index)
{
	int32 real_index = (self->tag_stack_first_unused_index-1) - index;

	if(real_index < 0)
		return NULL;

	return(self->tag_stack[real_index]);
}

/* the zero'th index is the bottom of the stack 
 */
TagAndStyleAssoc *
sml_get_reverse_index(SML_StyleAndTagStack *self, int32 index)
{
	if(index >= self->tag_stack_first_unused_index)
		return NULL;

	return(self->tag_stack[index]); 
}

/* the zero'th index is the top of the stack 
 */
TagStruct * 
SML_GetTagByIndex(SML_StyleAndTagStack *self, int32 index)
{
	TagAndStyleAssoc *assoc = sml_get_index(self, index);

	if(!assoc)
		return NULL;

	return(assoc->tag);
}

/* the zero'th index is the bottom of the stack 
 */
TagStruct * 
SML_GetTagByReverseIndex(SML_StyleAndTagStack *self, int32 index)
{
	TagAndStyleAssoc *assoc = sml_get_reverse_index(self, index);

	if(!assoc)
		return NULL;

	return(assoc->tag);
}

/* the zero'th index is the top of the stack 
 */
StyleStruct * 
SML_GetStyleByIndex(SML_StyleAndTagStack *self, int32 index)
{
	TagAndStyleAssoc *assoc = sml_get_index(self, index);

	if(!assoc)
		return NULL;

	return(assoc->style);
}

/* the zero'th index is the bottom of the stack 
 */
StyleStruct * 
SML_GetStyleByReverseIndex(SML_StyleAndTagStack *self, int32 index)
{
	TagAndStyleAssoc *assoc = sml_get_reverse_index(self, index);

	if(!assoc)
		return NULL;

	return(assoc->style);
}

void
SML_Delete(SML_StyleAndTagStack *self)
{
	self->refcount--;

	if(self->refcount > 0)
		return;

	sml_free_stack(self);

	XP_FREE(self);
}

void 
SML_Purge(SML_StyleAndTagStack *self)
{
	sml_free_stack(self);
}

void
SML_Init(SML_StyleAndTagStack *self, MWContext *context)
{
	self->context = context;
}

/* static vtable */
const StyleAndTagStackInterface StyleAndTagStack_interface = {

    (void (*)(StyleAndTagStack *self, MWContext *context))
		SML_Init,
   (TagStruct * (*)(StyleAndTagStack *self, char *name, char *class_name, char *id))
		SML_NewTagStruct,
    (void (*)(StyleAndTagStack *self, TagStruct *tag))
		SML_FreeTagStruct,
    (PushTagStatus (*)(StyleAndTagStack *self, char *name, char *class_name, char *id))
		SML_PushTag,
    (void  (*)(StyleAndTagStack *self, char *name))
		SML_PopTag,
    (void  (*)(StyleAndTagStack *self, char *name, int32 index))
		SML_PopTagByIndex,
    (TagStruct * (*)(StyleAndTagStack *self, int32 index))
		SML_GetTagByIndex,
    (TagStruct * (*)(StyleAndTagStack *self, int32 index))
		SML_GetTagByReverseIndex,
    (StyleStruct * (*)(StyleAndTagStack *self, int32 index))
		SML_GetStyleByIndex,
    (StyleStruct * (*)(StyleAndTagStack *self, int32 index))
		SML_GetStyleByReverseIndex,
    (StyleStruct * (*)(StyleAndTagStack *self))
		SML_Delete,
    (StyleStruct * (*)(StyleAndTagStack *self))
		SML_Purge,
    (void (*)(StyleAndTagStack *self, XP_Bool on_flag))
		SML_SetSaveOn,
    (XP_Bool (*)(StyleAndTagStack *self))
		SML_IsSaveOn
};

StyleAndTagStack *
SML_StyleStack_Factory_Create(void)
{
    /* initializer */
    SML_StyleAndTagStack * self = (SML_StyleAndTagStack*)XP_CALLOC(1, sizeof(SML_StyleAndTagStack));
   
    if(!self)
        return NULL;

    self->vtable = (void*)&StyleAndTagStack_interface;
    self->refcount = 1;

    return (StyleAndTagStack*)self;
}

void
SML_SetObjectRefs(StyleAndTagStack *styleStack,
				  StyleObject *tags,
				  StyleObject *classes,
				  StyleObject *ids)
{
	SML_StyleAndTagStack * self = (SML_StyleAndTagStack*)styleStack;

	self->tags = tags;
	self->classes = classes;
	self->ids = ids;
}

/*
 * Helper routine to assemble the JSSContext
 */
void
sml_GetJSSContext(StyleAndTagStack *styleStack, JSSContext *jc)
{
	SML_StyleAndTagStack * self = (SML_StyleAndTagStack*)styleStack;

	/* Lookup document.tags */
	jc->tags = self->tags;
	
	/* Lookup document.classes */
	jc->classes = self->classes;
	
	/* Lookup up document.ids */
	jc->ids = self->ids;
}

#ifdef TEST_STYLESTACK

typedef struct {
    char name[100];
    char class_name[100];
    char id[100];
} test_struct;

test_struct test_table[] = {

{"h1", "class1", "id1"},
{"h2", "class2", "id2"},
{"h3", "class3", "id3"},
{"h4", "class4", "id4"},

{"h5", "", "id1"},
{"h6", "", "id2"},
{"h7", "", "id3"},
{"h8", "", "id4"},

{"h9", "class9", ""},
{"h10", "class10", ""},
{"h11", "class11", ""},
{"h12", "class12", ""},

{"h13", "", ""},
{"h14", "", ""},
{"h15", "", ""},
{"h16", "", ""},

{"", "", ""}
};

void
test_values(StyleAndTagStack * h)
{
	int i;

    for(i=0; *test_table[i].name; i++)
		;  /* null body */

    for(i--; i >= 0; i--)
	{
		test_struct *ts = &test_table[i];

		TagStruct *tag = STYLESTACK_GetTagByIndex(h, 0);

		if(strcmp(tag->name, ts->name))
			printf("Error: names do not match: %s:%s\n", tag->name, ts->name);
		else if(*ts->class && strcmp(tag->class, ts->class))
			printf("Error: class names do not match: %s:%s\n", tag->class_name, ts->class_name);
		else if(*ts->id && strcmp(tag->id, ts->id))
			printf("Error: id names do not match: %s:%s\n", tag->id, ts->id);
		else
			printf("Success: retreival successful: %s\n", tag->name);

		STYLESTACK_PopTag(h, ts->name);
	}
}

int
main(int argc, char *argv[])
{
    int i;
	StyleAndTagStack * h;
	StyleStruct *style;


	h = SML_StyleStack_Factory_Create();

	if(!h)
		exit(1);

    for(i=0; *test_table[i].name; i++)
    {
		test_struct *ts = &test_table[i];

		style = STYLESTACK_PushTag(h, ts->name, ts->class_name, ts->id);

		if(!style)
			printf("stack (or malloc) error");
	}

	test_values(h);

	printf("\nAll tests complete\n");
		
	exit(0);
}

#endif /* TEST_STYLESTACK */
