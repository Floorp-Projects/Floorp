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


#include "xp.h"
#include "net.h"
#include "shist.h"
#include "pa_parse.h"
#include "layout.h"
#include "java.h"
#include "laylayer.h"
#include "libevent.h"
#include "libimg.h"             /* Image Library public API. */
#include "libi18n.h"		/* For the character code set conversions */
#include "intl_csi.h"

#include "secnav.h"		/* For SECNAV_GenKeyFromChoice and SECNAV_GetKeyChoiceList protos */

#ifdef PROFILE
#pragma profile on
#endif

#ifndef XP_TRACE
# define XP_TRACE(X) fprintf X
#endif

#ifdef TEST_16BIT
#define XP_WIN16
#endif /* TEST_16BIT */

#ifdef XP_WIN16
#define SIZE_LIMIT              32000
#endif /* XP_WIN16 */


#define	DEF_ISINDEX_PROMPT "This is a searchable index. Enter search keywords: "
#define	ISINDEX_TAG_TEXT	"name=isindex>"
#define	DEF_FILE_ELE_VALUE	"Choose File"
#define	DEF_RESET_BUTTON_NAME	"reset"
#define	DEF_RESET_BUTTON_TEXT	"Reset"
#define	DEF_SUBMIT_BUTTON_NAME	"submit"
#define	DEF_SUBMIT_BUTTON_TEXT	"Submit Query"
#define	DEF_BUTTON_TEXT		"  "
#define	DEF_TOGGLE_VALUE	"on"
#define	DEF_TEXT_ELE_SIZE	20
#define	DEF_TEXTAREA_ELE_ROWS	1
#define	DEF_TEXTAREA_ELE_COLS	20
#define	DEF_SELECT_ELE_SIZE	1

#define	MAX_BUTTON_WIDTH	1000
#define	MAX_BUTTON_HEIGHT	1000

static PA_Block lo_dup_block(PA_Block);		/* forward declaration */

/* VERY IMPORTANT
 *
 * the form_list linked list is in REVERSE
 * order.  Therefore if there is more than
 * one form on a page the different forms
 * are in the list in the REVERSE order
 * that they appear on the page.
 */
PRIVATE LO_FormElementStruct *
lo_return_next_form_element(MWContext *context,
                            LO_FormElementStruct *current_element,
							XP_Bool reverse_order)
{
	int32 doc_id;
	int i;
    lo_TopState *top_state;
    lo_DocState *state;
	lo_FormData *form_list;
	lo_FormData *prev_form_list=NULL;
	LO_Element **ele_list;
	LO_FormElementStruct *form_ele;

    /*
     * Get the unique document ID, and retreive this
     * documents layout state.
     */
    doc_id = XP_DOCID(context);
    top_state = lo_FetchTopState(doc_id);
    if ((top_state == NULL)||(top_state->doc_state == NULL))
    {
        return(NULL);
    }
    state = top_state->doc_state;

	/* 
	 * XXX BUGBUG This and the corresponding routines, only deal
	 * with forms in the main document and not forms in layers.
	 */
    /*
     * Extract the list of all forms in this doc.
     */
    form_list = top_state->doc_lists.form_list;
	
	while(form_list)
	  {
    	PA_LOCK(ele_list, LO_Element **, form_list->form_elements);

    	for (i=0; i<form_list->form_ele_cnt; i++)
    	  {
	
        	form_ele = (LO_FormElementStruct *)ele_list[i];

			if(NULL == current_element && FALSE == reverse_order)
			  {
				/* since the passed in element was NULL
				 * or we set the current element below
				 * since we wanted the next element
				 * return the first element we encounter.
				 */
    			PA_UNLOCK(form_list->form_elements);
				return(form_ele);

			  }
			else if(form_ele == current_element)
		  	  {
				if(reverse_order)
				  {
					if(i > 0)
					  {
    					PA_UNLOCK(form_list->form_elements);
						return((LO_FormElementStruct *)ele_list[i-1]);
					  }
					else if(NULL == form_list->next)
					  {
    					PA_UNLOCK(form_list->form_elements);
						return NULL;  /* first element in whole list*/
					  }
					else
					  {
                        /* if there is a next form_list
                         * then return the last element
                         * of the next form_list
                         */
    					PA_UNLOCK(form_list->form_elements);

						form_list = form_list->next;
                        if(form_list->form_ele_cnt == 0)
                            return NULL;

    					PA_LOCK(ele_list, 
								LO_Element **, 
								form_list->form_elements);
        				form_ele = (LO_FormElementStruct *)
									ele_list[form_list->form_ele_cnt-1];

    					PA_UNLOCK(form_list->form_elements);

						return(form_ele);
					  }
				  }
				else
				  {
					if(i+1<form_list->form_ele_cnt)
					  {
    					PA_UNLOCK(form_list->form_elements);
						return((LO_FormElementStruct *)ele_list[i+1]);
					  }
					else if(NULL == prev_form_list)
					  {
    					PA_UNLOCK(form_list->form_elements);
						return NULL;  /* last element in whole list*/
					  }
					else
					  {
						/* choose the first element of the
						 * previous form list 
						 */
                        PA_UNLOCK(form_list->form_elements);

                        if(prev_form_list->form_ele_cnt == 0)
                            return NULL;

                        PA_LOCK(ele_list,
                                LO_Element **,
                                prev_form_list->form_elements);

                        form_ele = (LO_FormElementStruct *)
                                    ele_list[0];

                        PA_UNLOCK(prev_form_list->form_elements);

                        return(form_ele);

					  }
				  }
		  	  }
		  }

    	PA_UNLOCK(form_list->form_elements);

        if(NULL == current_element && TRUE == reverse_order)
	      {
		    /* since the passed in element was NULL
		     * return the last element we encountered.
		     */
		    return(form_ele);
	      }

		prev_form_list = form_list;
		form_list = form_list->next;
	  }

	if(current_element == NULL)
		return NULL;

	/* the element was not found in the list
	 * something must be wrong, lets assert!
	 */
	XP_ASSERT(0);

	return NULL;
}


/* return the next form element in layouts list of form elements
 *
 * If NULL is passed in, this function returns the first element.
 *
 * If the last element is passed in, this function returns NULL
 *
 * If a garbage pointer is passed in, this function returns NULL
 */
PUBLIC LO_FormElementStruct *
LO_ReturnNextFormElement(MWContext *context,
                         LO_FormElementStruct *current_element)
{
	return(lo_return_next_form_element(context,   
										current_element,
										FALSE));

}

/* return the previous form element in layouts list of form elements
 *
 * If NULL is passed in, this function returns the last element.
 *
 * If the first element is passed in, this function returns NULL
 *
 * If a garbage pointer is passed in, this function returns NULL
 */
PUBLIC LO_FormElementStruct *
LO_ReturnPrevFormElement(MWContext *context,
                         LO_FormElementStruct *current_element)
{

	return(lo_return_next_form_element(context, 
										current_element,
										TRUE));
}

/* #ifndef NO_TAB_NAVIGATION */
/*
 for Form element type FORM_TYPE_RADIO and FORM_TYPE_CHECKBOX,
 if the element has focus, the Text elment following it needs to
 draw the Tab Focus(dotted box).
*/
Bool
LO_isFormElementNeedTextTabFocus( LO_FormElementStruct *pElement )
{
	if( pElement == NULL || pElement->type != LO_FORM_ELE )
		return(FALSE);

	/*
	 * Under bizarre cases, this may be an invalid form element
	 * with no form data.
	 */
	if (pElement->element_data == NULL)
		return(FALSE);
	
	if( pElement->element_data->type == FORM_TYPE_RADIO
		|| pElement->element_data->type == FORM_TYPE_CHECKBOX )
		return(TRUE);

	return(FALSE);

}

/* Function LO_isTabableFormElement() is copied from
 PUBLIC LO_FormElementStruct * LO_ReturnNextFormElementInTabGroup(MWContext *context, 	
*/
Bool 
LO_isTabableFormElement( LO_FormElementStruct * next_ele )  
{
	/*
	 * Under bizarre cases, this may be an invalid form element
	 * with no form data.
	 */
	if (next_ele->element_data == NULL)
		return(FALSE);

	if (next_ele->element_data->type == FORM_TYPE_TEXT
		|| next_ele->element_data->type == FORM_TYPE_SUBMIT
		|| next_ele->element_data->type == FORM_TYPE_RESET
		|| next_ele->element_data->type == FORM_TYPE_PASSWORD
		|| next_ele->element_data->type == FORM_TYPE_BUTTON
		|| next_ele->element_data->type == FORM_TYPE_RADIO
		|| next_ele->element_data->type == FORM_TYPE_CHECKBOX
		|| next_ele->element_data->type == FORM_TYPE_SELECT_ONE
		|| next_ele->element_data->type == FORM_TYPE_SELECT_MULT
		|| next_ele->element_data->type == FORM_TYPE_TEXTAREA
		|| next_ele->element_data->type == FORM_TYPE_FILE)
	{
		return(TRUE);
	}
	else 
		return( FALSE );
}
/* NO_TAB_NAVIGATION */

/* NO_TAB_NAVIGATION, 
	LO_ReturnNextFormElementInTabGroup() is used to tab through form elements.
	Since the winfe now has TAB_NAVIGATION, it is not used any more.
	If mac and Unix don't use it either, it can be removed.
*/

/* return the next tabable form element struct
 *
 * Current tabable elements are:
 *  
 *  FORM_TYPE_TEXT
 *	FORM_TYPE_SUBMIT
 *	FORM_TYPE_RESET
 *	FORM_TYPE_PASSWORD
 *	FORM_TYPE_BUTTON
 *	FORM_TYPE_FILE
 *  FORM_TYPE_RADIO
 *  FORM_TYPE_CHECKBOX
 *  FORM_TYPE_SELECT_ONE
 *  FORM_TYPE_SELECT_MULT
 *  FORM_TYPE_TEXTAREA
 *
 * If the last element is passed in the first element is returned.
 * If garbage is passed in the first element is returned.
 */
PUBLIC LO_FormElementStruct *
LO_ReturnNextFormElementInTabGroup(MWContext *context, 	
								   LO_FormElementStruct *current_element,
								   XP_Bool go_backwards)
{
	LO_FormElementStruct *next_ele;

	/*
	 * Under bizarre cases, this may be an invalid form element
	 * with no form data.
	 */
	if (current_element->element_data == NULL)
		return(NULL);

	while((next_ele = lo_return_next_form_element(context, current_element, go_backwards)) != NULL)
	  {

    	if (next_ele->element_data->type == FORM_TYPE_TEXT
			|| next_ele->element_data->type == FORM_TYPE_SUBMIT
			|| next_ele->element_data->type == FORM_TYPE_RESET
			|| next_ele->element_data->type == FORM_TYPE_PASSWORD
			|| next_ele->element_data->type == FORM_TYPE_BUTTON
			|| next_ele->element_data->type == FORM_TYPE_RADIO
			|| next_ele->element_data->type == FORM_TYPE_CHECKBOX
			|| next_ele->element_data->type == FORM_TYPE_SELECT_ONE
			|| next_ele->element_data->type == FORM_TYPE_SELECT_MULT
			|| next_ele->element_data->type == FORM_TYPE_TEXTAREA
			|| next_ele->element_data->type == FORM_TYPE_FILE)
		  {

			return(next_ele);
		  }
		else
		  {
			current_element = next_ele;
		  }
      }

	/* return the first element */    
	return(lo_return_next_form_element(context, NULL, go_backwards));
}

void
lo_BeginForm(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	lo_FormData *form;
	PA_Block buff;
	char *str;
	lo_DocLists *doc_lists;

	form = XP_NEW(lo_FormData);
	if (form == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	doc_lists = lo_GetCurrentDocLists(state);

	state->top_state->in_form = TRUE;
	doc_lists->current_form_num++;

	form->id = doc_lists->current_form_num;
	form->form_ele_cnt = 0;
	form->form_ele_size = 0;
	form->form_elements = NULL;
	form->next = NULL;
#ifdef MOCHA
	form->name = lo_FetchParamValue(context, tag, PARAM_NAME);
	form->mocha_object = NULL;
#endif

	buff = lo_FetchParamValue(context, tag, PARAM_ACTION);
	if (buff != NULL)
	{
		char *url;

		PA_LOCK(str, char *, buff);
		url = NET_MakeAbsoluteURL(state->top_state->base_url, str);
		PA_UNLOCK(buff);
		PA_FREE(buff);
		if (url == NULL)
		{
			buff = NULL;
		}
		else
		{
			buff = PA_ALLOC(XP_STRLEN(url) + 1);
			if (buff != NULL)
			{
				PA_LOCK(str, char *, buff);
				XP_STRCPY(str, url);
				PA_UNLOCK(buff);
			}
			else
			{
				state->top_state->out_of_memory = TRUE;
			}
			XP_FREE(url);
		}
	}
	else if (state->top_state->base_url != NULL)
	{
		buff = PA_ALLOC(XP_STRLEN(state->top_state->base_url) + 1);
		if (buff != NULL)
		{
			PA_LOCK(str, char *, buff);
			XP_STRCPY(str, state->top_state->base_url);
			PA_UNLOCK(buff);
		}
		else
		{
			state->top_state->out_of_memory = TRUE;
		}
	}
	form->action = buff;

	form->encoding = lo_FetchParamValue(context, tag, PARAM_ENCODING);

	buff = lo_FetchParamValue(context, tag, PARAM_TARGET);
	if (buff != NULL)
	{
		int32 len;
		char *target;

		PA_LOCK(target, char *, buff);
		len = lo_StripTextWhitespace(target, XP_STRLEN(target));
		if ((*target == '\0')||
		    (lo_IsValidTarget(target) == FALSE))
		{
			PA_UNLOCK(buff);
			PA_FREE(buff);
			buff = NULL;
		}
		else
		{
			PA_UNLOCK(buff);
		}
	}
	/*
	 * If there was no target use the default one.
	 * (default provided by BASE tag)
	 */
	if ((buff == NULL)&&(state->top_state->base_target != NULL))
	{
		buff = PA_ALLOC(XP_STRLEN(state->top_state->base_target) + 1);
		if (buff != NULL)
		{
			char *targ;

			PA_LOCK(targ, char *, buff);
			XP_STRCPY(targ, state->top_state->base_target);
			PA_UNLOCK(buff);
		}
		else
		{
			state->top_state->out_of_memory = TRUE;
		}
	}
	form->window_target = buff;

	form->method = FORM_METHOD_GET;
	buff = lo_FetchParamValue(context, tag, PARAM_METHOD);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		if (pa_TagEqual("post", str))
		{
			form->method = FORM_METHOD_POST;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	form->next = doc_lists->form_list;
	doc_lists->form_list = form;

#ifdef MOCHA
	if (!state->in_relayout)
	{
		lo_BeginReflectForm(context, state, tag, form);
	}
#endif
}


void
lo_EndForm(MWContext *context, lo_DocState *state)
{
	intn i, form_id;
	lo_FormData *form_list;
	LO_Element **ele_list;
	LO_FormElementStruct *single_text_ele;
	lo_DocLists *doc_lists;

	state->top_state->in_form = FALSE;

	doc_lists = lo_GetCurrentDocLists(state);
	form_id = doc_lists->current_form_num;
	form_list = doc_lists->form_list;
	while (form_list != NULL)
	{
		if (form_list->id == form_id)
		{
			break;
		}
		form_list = form_list->next;
	}
	if (form_list == NULL)
	{
		return;
	}

	single_text_ele = NULL;
	PA_LOCK(ele_list, LO_Element **, form_list->form_elements);
	for (i=0; i<form_list->form_ele_cnt; i++)
	{
		LO_FormElementStruct *form_ele;

		form_ele = (LO_FormElementStruct *)ele_list[i];
		if (form_ele->element_data == NULL)
		{
			continue;
		}

		if ((form_ele->element_data->type == FORM_TYPE_TEXT)||
			(form_ele->element_data->type == FORM_TYPE_PASSWORD))
		{
			if (single_text_ele != NULL)
			{
				single_text_ele = NULL;
				break;
			}
			else
			{
				single_text_ele = form_ele;
			}
		}
	}
	PA_UNLOCK(form_list->form_elements);

	if (single_text_ele != NULL)
	{
		FE_FormTextIsSubmit(context, single_text_ele);
	}

#ifdef MOCHA
	if (!state->in_relayout)
	{
		lo_EndReflectForm(context, form_list);
	}
#endif
}


int32
lo_ResolveInputType(char *type_str)
{
	if (type_str == NULL)
	{
		return(FORM_TYPE_TEXT);
	}

	if (pa_TagEqual(S_FORM_TYPE_TEXT, type_str))
	{
		return(FORM_TYPE_TEXT);
	}
	else if (pa_TagEqual(S_FORM_TYPE_RADIO, type_str))
	{
		return(FORM_TYPE_RADIO);
	}
	else if (pa_TagEqual(S_FORM_TYPE_CHECKBOX, type_str))
	{
		return(FORM_TYPE_CHECKBOX);
	}
	else if (pa_TagEqual(S_FORM_TYPE_HIDDEN, type_str))
	{
		return(FORM_TYPE_HIDDEN);
	}
	else if (pa_TagEqual(S_FORM_TYPE_SUBMIT, type_str))
	{
		return(FORM_TYPE_SUBMIT);
	}
	else if (pa_TagEqual(S_FORM_TYPE_RESET, type_str))
	{
		return(FORM_TYPE_RESET);
	}
	else if (pa_TagEqual(S_FORM_TYPE_PASSWORD, type_str))
	{
		return(FORM_TYPE_PASSWORD);
	}
	else if (pa_TagEqual(S_FORM_TYPE_BUTTON, type_str))
	{
		return(FORM_TYPE_BUTTON);
	}
	else if (pa_TagEqual(S_FORM_TYPE_FILE, type_str))
	{
		return(FORM_TYPE_FILE);
	}
	else if (pa_TagEqual(S_FORM_TYPE_IMAGE, type_str))
	{
		return(FORM_TYPE_IMAGE);
	}
	else if (pa_TagEqual(S_FORM_TYPE_OBJECT, type_str))
	{
		return(FORM_TYPE_OBJECT);
	}
	else if (pa_TagEqual(S_FORM_TYPE_JOT, type_str))
	{
		return(FORM_TYPE_JOT);
	}
	else if (pa_TagEqual(S_FORM_TYPE_READONLY, type_str))
	{
		return(FORM_TYPE_READONLY);
	}
	else
	{
		return(FORM_TYPE_TEXT);
	}
}


static void
lo_recolor_form_element_bg(lo_DocState *state, LO_FormElementStruct *form_ele,
	LO_Color *bg_color)
{
	LO_TextAttr *old_attr;
	LO_TextAttr *attr;
	LO_TextAttr tmp_attr;

	attr = NULL;
	old_attr = form_ele->text_attr;
	if (old_attr != NULL)
	{
		lo_CopyTextAttr(old_attr, &tmp_attr);
		
        /* don't recolor the background if it was
         * explicitly set to another color 
         */
        if(tmp_attr.no_background == TRUE)
        {
            tmp_attr.bg.red = bg_color->red;
    		tmp_attr.bg.green = bg_color->green;
	    	tmp_attr.bg.blue = bg_color->blue;
        }

        /* removed by Lou 4-3-97.  Allow text to
		 * overlap correctly in tables
	 	 */
		tmp_attr.no_background = FALSE;

		attr = lo_FetchTextAttr(state, &tmp_attr);
	}
	form_ele->text_attr = attr;
}


static LO_FormElementStruct *
new_form_element(MWContext *context, lo_DocState *state, int32 type)
{
	LO_FormElementStruct *form_element;
	lo_DocLists *doc_lists;
#ifdef XP_WIN
	XP_Bool attr_change;
	LO_TextAttr tmp_attr;
#endif

	doc_lists = lo_GetCurrentDocLists(state);
	
	form_element = (LO_FormElementStruct *)lo_NewElement(context, state, LO_FORM_ELE, NULL, 0);
	if (form_element == NULL)
	{
		return(NULL);
	}

	form_element->type = LO_FORM_ELE;
	form_element->ele_id = NEXT_ELEMENT;
	form_element->x = state->x;
	form_element->y = state->y;
	form_element->x_offset = 0;
	form_element->y_offset = 0;
	form_element->border_vert_space = 0;
	form_element->border_horiz_space = 0;
	form_element->width = 0;
	form_element->height = 0;

	form_element->next = NULL;
	form_element->prev = NULL;

	form_element->form_id = doc_lists->current_form_num;
	form_element->layer_id = lo_CurrentLayerId(state);
	form_element->element_data = NULL;
	form_element->element_index = 0;
#ifdef MOCHA
	form_element->mocha_object = NULL;
	form_element->event_handler_present = FALSE;
#endif

	if (state->font_stack == NULL)
	{
		form_element->text_attr = NULL;
	}
	else
	{
		form_element->text_attr = state->font_stack->text_attr;
		/*
		 * Possibly inherit the background color attribute
		 * of a parent table cell.
		 */
		if (state->is_a_subdoc == SUBDOC_CELL)
		{
			lo_DocState *up_state;

			/*
			 * Find the parent subdoc's state.
			 */
			up_state = state->top_state->doc_state;
			while ((up_state->sub_state != NULL)&&
				(up_state->sub_state != state))
			{
				up_state = up_state->sub_state;
			}
			if ((up_state->sub_state != NULL)&&
			    (up_state->current_ele != NULL)&&
			    (up_state->current_ele->type == LO_SUBDOC)&&
			    (up_state->current_ele->lo_subdoc.backdrop.bg_color != NULL))
			{
				lo_recolor_form_element_bg(state, form_element,
				    up_state->current_ele->lo_subdoc.backdrop.bg_color);
			}
		}
	}
#ifdef XP_WIN
	attr_change = FALSE;
	attr_change = FE_CheckFormTextAttributes(context,
			form_element->text_attr, &tmp_attr, type);
	if (attr_change != FALSE)
	{
		form_element->text_attr = lo_FetchTextAttr(state, &tmp_attr);
	}
#endif

	form_element->ele_attrmask = 0;

	form_element->sel_start = -1;
	form_element->sel_end = -1;

	return(form_element);
}


/*
 * If this document already has a list of form
 * data elements, return a pointer to the next form element data
 * structure.  Otherwise allocate a new data element structure, and
 * put the pointer to it in the document's list of
 * form data elements before returning it.
 */
static LO_FormElementData *
lo_next_element_data(lo_DocState *state, int32 type)
{
	lo_SavedFormListData *all_ele;
	int32 index;
	LO_FormElementData **data_list;
	LO_FormElementData *data_ele;

	all_ele = state->top_state->savedData.FormList;
	index = all_ele->data_index++;

	/*
	 * This only happens if we didn't have a pre-saved
	 * data list, so we grow this one and create a new element.
	 */
	if (all_ele->data_index > all_ele->data_count)
	{
		PA_Block old_data_list; /* really a (LO_FormElementData **) */

#ifdef XP_WIN16
		if ((all_ele->data_index * sizeof(LO_FormElementData *)) >
			SIZE_LIMIT)
		{
			all_ele->data_index--;
			return(NULL);
		}
#endif /* XP_WIN16 */
		all_ele->data_count = all_ele->data_index;
		old_data_list = NULL;
		if (all_ele->data_count == 1)
		{
			all_ele->data_list = PA_ALLOC(all_ele->data_count
				* sizeof(LO_FormElementData *));
		}
		else
		{
			old_data_list = all_ele->data_list;
			all_ele->data_list = PA_REALLOC(
				all_ele->data_list, (all_ele->data_count *
				sizeof(LO_FormElementData *)));
		}
		if (all_ele->data_list == NULL)
		{
			all_ele->data_list = old_data_list;
			all_ele->data_index--;
			all_ele->data_count--;
			state->top_state->out_of_memory = TRUE;
			return(NULL);
		}

		PA_LOCK(data_list, LO_FormElementData **,
			all_ele->data_list);
		data_list[index] = XP_NEW_ZAP(LO_FormElementData);
		if (data_list[index] != NULL)
		{
			data_list[index]->type = FORM_TYPE_NONE;
		}
		else
		{
			state->top_state->out_of_memory = TRUE;
		}
		PA_UNLOCK(all_ele->data_list);
	}

	PA_LOCK(data_list, LO_FormElementData **, all_ele->data_list);
	data_ele = data_list[index];

	/*
	 * If the type is changing, someone may be attacking us via server push
     * or JavaScript document.write and history.go(0), substituting a file
	 * upload input for a text input from the previous version of the doc
	 * that has its value field set to "/etc/passwd".
	 */
	if ((data_ele->type != FORM_TYPE_NONE) && (data_ele->type != type))
	{
		lo_FreeFormElementData(data_ele);
		XP_BZERO(data_ele, sizeof(*data_ele));
		data_ele->type = FORM_TYPE_NONE;
	}
	
	PA_UNLOCK(all_ele->data_list);
	/*
	 * If we are reusing an old structure, clean it of old memory
	 */
	lo_CleanFormElementData(data_ele);

	return(data_ele);
}


static LO_FormElementStruct *
lo_form_input_text(MWContext *context, lo_DocState *state,
	PA_Tag *tag, int32 type)
{
	LO_FormElementStruct *form_element;
	lo_FormElementTextData *form_data;
	PA_Block buff;
#if defined(XP_MAC)&&defined(MOCHA)
	PA_Block keydown, keypress, keyup;
#endif /* defined(XP_MAC)&&defined(MOCHA) */
	char *str;
	char *tptr;
	int16 charset;

	form_element = new_form_element(context, state, type);
	if (form_element == NULL)
	{
		return(NULL);
	}

	form_element->element_data = lo_next_element_data(state, type);
	if (form_element->element_data == NULL)
	{
		lo_FreeElement(context, (LO_Element *)form_element, FALSE);
		return(NULL);
	}
	form_element->element_index =
		state->top_state->savedData.FormList->data_index - 1;
	form_data = (lo_FormElementTextData *)form_element->element_data;

	/*
	 * IF our starting type is NONE, this is a new element,
	 * an we need to initialize the locations that will later
	 * hold persistent data.
	 */
	if (form_data->type == FORM_TYPE_NONE)
	{
		form_data->FE_Data = NULL;
		form_data->current_text = NULL;
	}

	form_data->encoding = INPUT_TYPE_ENCODING_NORMAL; /* default */

	form_data->type = type;
	form_data->name = lo_FetchParamValue(context, tag, PARAM_NAME);
	buff = lo_FetchParamValue(context, tag, PARAM_VALUE);
	PA_LOCK(tptr, char *, buff);
	/*
	 * strip newlines from single line text
	 * default values.
	 */
	if ((tptr != NULL)&&(type != FORM_TYPE_PASSWORD))
	{
		int32 len;

		len = XP_STRLEN(tptr);
		len = lo_StripTextNewlines(tptr, len);
	}
	charset = form_element->text_attr->charset;
	tptr = FE_TranslateISOText(context, charset, tptr);
	PA_UNLOCK(buff);
	form_data->default_text = buff;

#ifdef OLD_FILE_UPLOAD
	if ((form_data->default_text == NULL)&&(type == FORM_TYPE_FILE))
	{
		form_data->default_text = PA_ALLOC(
				(XP_STRLEN(DEF_FILE_ELE_VALUE) + 1)
				* sizeof(char));
		if (form_data->default_text != NULL)
		{
			PA_LOCK(tptr, char *, form_data->default_text);
			XP_STRCPY(tptr, DEF_FILE_ELE_VALUE);
			PA_UNLOCK(form_data->default_text);
		}
	}
#else
	/*
	 * File upload forms cannot default to a filename
	 */
	if ((form_data->default_text != NULL)&&(type == FORM_TYPE_FILE))
	{
		PA_FREE(form_data->default_text);
		form_data->default_text = NULL;
	}
#endif /* OLD_FILE_UPLOAD */

	form_data->read_only = FALSE;
	if (type != FORM_TYPE_FILE)
	  {
		buff = lo_FetchParamValue(context, tag, PARAM_READONLY);
		if (buff != NULL)
		  {
			PA_FREE(buff);
			form_data->read_only = TRUE;
		  }
	  }

	form_data->size = DEF_TEXT_ELE_SIZE;
	buff = lo_FetchParamValue(context, tag, PARAM_SIZE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		form_data->size = XP_ATOI(str);
		PA_UNLOCK(buff);
		PA_FREE(buff);
		if (form_data->size < 1)
		{
			form_data->size = 1;
		}
	}
#ifdef OLD_FILE_UPLOAD
	else if ((type == FORM_TYPE_FILE)&&(form_data->default_text != NULL))
	{
		int32 len;

		PA_LOCK(tptr, char *, form_data->default_text);
		len = XP_STRLEN(tptr);
		PA_UNLOCK(form_data->default_text);
		if (len > 0)
		{
			form_data->size = len;
		}
	}
#endif /* OLD_FILE_UPLOAD */

	buff = lo_FetchParamValue(context, tag, PARAM_ENCODING);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		if(!XP_STRCASECMP(str, "MACBINARY"))
			form_data->encoding = INPUT_TYPE_ENCODING_MACBIN;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	form_data->max_size = -1;
	buff = lo_FetchParamValue(context, tag, PARAM_MAXLENGTH);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		form_data->max_size = XP_ATOI(str);
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	
#if defined(XP_MAC)&&defined(MOCHA)
    keydown = lo_FetchParamValue(context, tag, PARAM_ONKEYDOWN);
    keypress = lo_FetchParamValue(context, tag, PARAM_ONKEYPRESS);
    keyup = lo_FetchParamValue(context, tag, PARAM_ONKEYUP);

    /* Text fields need this info which needs to be carried across table cell relayouts.
       Only the mac FE checks the event_handler_present bit. */
    if (keydown || keypress || keyup)
	form_element->event_handler_present = TRUE;
	
	if (keydown)
		PA_FREE(keydown);
	if (keypress)
		PA_FREE(keypress);
	if (keyup)
		PA_FREE(keyup);		
#endif /* defined(XP_MAC)&&defined(MOCHA) */

	return(form_element);
}


static LO_FormElementStruct *
lo_form_textarea(MWContext *context, lo_DocState *state,
	PA_Tag *tag, int32 type)
{
	LO_FormElementStruct *form_element;
	lo_FormElementTextareaData *form_data;
	PA_Block buff;
#if defined(XP_MAC)&&defined(MOCHA)
	PA_Block keydown, keypress, keyup;
#endif /* defined(XP_MAC)&&defined(MOCHA) */
	char *str;

	form_element = new_form_element(context, state, type);
	if (form_element == NULL)
	{
		return(NULL);
	}

	form_element->element_data = lo_next_element_data(state, type);
	if (form_element->element_data == NULL)
	{
		lo_FreeElement(context, (LO_Element *)form_element, FALSE);
		return(NULL);
	}
	form_element->element_index =
		state->top_state->savedData.FormList->data_index - 1;
	form_data = (lo_FormElementTextareaData *)form_element->element_data;

	/*
	 * IF our starting type is NONE, this is a new element,
	 * an we need to initialize the locations that will later
	 * hold persistent data.
	 */
	if (form_data->type == FORM_TYPE_NONE)
	{
		form_data->FE_Data = NULL;
		form_data->current_text = NULL;
	}

	form_data->type = type;
	form_data->name = lo_FetchParamValue(context, tag, PARAM_NAME);
	form_data->default_text = NULL;

	form_data->rows = DEF_TEXTAREA_ELE_ROWS;
	buff = lo_FetchParamValue(context, tag, PARAM_ROWS);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		form_data->rows = XP_ATOI(str);
		PA_UNLOCK(buff);
		PA_FREE(buff);
		if (form_data->rows < 1)
		{
			form_data->rows = 1;
		}
	}

	form_data->cols = DEF_TEXTAREA_ELE_COLS;
	buff = lo_FetchParamValue(context, tag, PARAM_COLS);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		form_data->cols = XP_ATOI(str);
		PA_UNLOCK(buff);
		PA_FREE(buff);
		if (form_data->cols < 1)
		{
			form_data->cols = 1;
		}
	}

	form_data->disabled = FALSE;
	buff = lo_FetchParamValue(context, tag, PARAM_DISABLED);
	if (buff != NULL)
	{
		PA_FREE(buff);
		form_data->disabled = TRUE;
	}

	form_data->read_only = FALSE;
	buff = lo_FetchParamValue(context, tag, PARAM_READONLY);
	if (buff != NULL)
	{
		PA_FREE(buff);
		form_data->read_only = TRUE;
	}

	form_data->auto_wrap = TEXTAREA_WRAP_OFF;
	buff = lo_FetchParamValue(context, tag, PARAM_WRAP);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		if (pa_TagEqual("off", str))
		{
			form_data->auto_wrap = TEXTAREA_WRAP_OFF;
		}
		else if (pa_TagEqual("hard", str))
		{
			form_data->auto_wrap = TEXTAREA_WRAP_HARD;
		}
		else if (pa_TagEqual("soft", str))
		{
			form_data->auto_wrap = TEXTAREA_WRAP_SOFT;
		}
		/*
		 * Make <TEXTAREA WRAP>
		 * default to WRAP=SOFT
		 */
		else
		{
			form_data->auto_wrap = TEXTAREA_WRAP_SOFT;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

#if defined(XP_MAC)&&defined(MOCHA)
    keydown = lo_FetchParamValue(context, tag, PARAM_ONKEYDOWN);
    keypress = lo_FetchParamValue(context, tag, PARAM_ONKEYPRESS);
    keyup = lo_FetchParamValue(context, tag, PARAM_ONKEYUP);

    /* Text fields need this info which needs to be carried across table cell relayouts.
       Only the mac FE checks the event_handler_present bit. */
    if (keydown || keypress || keyup)
	form_element->event_handler_present = TRUE;
	
	if (keydown)
		PA_FREE(keydown);
	if (keypress)
		PA_FREE(keypress);
	if (keyup)
		PA_FREE(keyup);		
#endif /* defined(XP_MAC)&&defined(MOCHA) */

	return(form_element);
}


static LO_FormElementStruct *
lo_form_input_minimal(MWContext *context, lo_DocState *state,
	PA_Tag *tag, int32 type)
{
	LO_FormElementStruct *form_element;
	lo_FormElementMinimalData *form_data;

	form_element = new_form_element(context, state, type);
	if (form_element == NULL)
	{
		return(NULL);
	}

	form_element->element_data = lo_next_element_data(state, type);
	if (form_element->element_data == NULL)
	{
		lo_FreeElement(context, (LO_Element *)form_element, FALSE);
		return(NULL);
	}
	form_element->element_index =
		state->top_state->savedData.FormList->data_index - 1;
	form_data = (lo_FormElementMinimalData *)form_element->element_data;

	/*
	 * IF our starting type is NONE, this is a new element,
	 * an we need to initialize the locations that will later
	 * hold persistent data.
	 */
	if (form_data->type == FORM_TYPE_NONE)
	{
		form_data->FE_Data = NULL;
	}

	form_data->type = type;

	form_data->name = lo_FetchParamValue(context, tag, PARAM_NAME);
#ifdef NICE_BUT_NOT_NCSA_COMPAT
	if (form_data->name == NULL)
	{
		if (form_data->type == FORM_TYPE_SUBMIT)
		{
			form_data->name = XP_ALLOC_BLOCK(XP_STRLEN(
				DEF_SUBMIT_BUTTON_NAME) + 1);
			if (form_data->name != NULL)
			{
				char *name;

				XP_LOCK_BLOCK(name, char *, form_data->name);
				XP_STRCPY(name, DEF_SUBMIT_BUTTON_NAME);
				XP_UNLOCK_BLOCK(form_data->name);
			}
		}
		else if (form_data->type == FORM_TYPE_RESET)
		{
			form_data->name = XP_ALLOC_BLOCK(XP_STRLEN(
				DEF_RESET_BUTTON_NAME) + 1);
			if (form_data->name != NULL)
			{
				char *name;

				XP_LOCK_BLOCK(name, char *, form_data->name);
				XP_STRCPY(name, DEF_RESET_BUTTON_NAME);
				XP_UNLOCK_BLOCK(form_data->name);
			}
		}
	}
#endif /* NICE_BUT_NOT_NCSA_COMPAT */

	form_data->value = lo_FetchParamValue(context, tag, PARAM_VALUE);
	if (form_data->value == NULL)
	{
		if (form_data->type == FORM_TYPE_SUBMIT)
		{
			form_data->value = PA_ALLOC(
				XP_STRLEN(DEF_SUBMIT_BUTTON_TEXT) + 1);
			if (form_data->value != NULL)
			{
				char *value;

				PA_LOCK(value, char *, form_data->value);
				XP_STRCPY(value, DEF_SUBMIT_BUTTON_TEXT);
				PA_UNLOCK(form_data->value);
			}
			else
			{
				state->top_state->out_of_memory = TRUE;
			}
		}
		else if (form_data->type == FORM_TYPE_RESET)
		{
			form_data->value = PA_ALLOC(
				XP_STRLEN(DEF_RESET_BUTTON_TEXT) + 1);
			if (form_data->value != NULL)
			{
				char *value;

				PA_LOCK(value, char *, form_data->value);
				XP_STRCPY(value, DEF_RESET_BUTTON_TEXT);
				PA_UNLOCK(form_data->value);
			}
			else
			{
				state->top_state->out_of_memory = TRUE;
			}
		}
		else
		{
			form_data->value = PA_ALLOC(
				XP_STRLEN(DEF_BUTTON_TEXT) + 1);
			if (form_data->value != NULL)
			{
				char *value;

				PA_LOCK(value, char *, form_data->value);
				XP_STRCPY(value, DEF_BUTTON_TEXT);
				PA_UNLOCK(form_data->value);
			}
			else
			{
				state->top_state->out_of_memory = TRUE;
			}
		}
	}
	else if ((type == FORM_TYPE_SUBMIT)||(type == FORM_TYPE_RESET))
	{
		int32 len;
		char *value;

		/*
		 * strip newlines from single line button names
		 */
		PA_LOCK(value, char *, form_data->value);
		len = XP_STRLEN(value);
		len = lo_StripTextNewlines(value, len);
		PA_UNLOCK(form_data->value);
	}

	/*
	 * Buttons can set a preferred minimum width and height
	 * in pixels that may be larger that what is needed
	 * to display button text.
	 */
	if ((type == FORM_TYPE_SUBMIT)||(type == FORM_TYPE_RESET)||
	    (type == FORM_TYPE_BUTTON))
	{
		PA_Block buff;
		char *str;
		int32 val;

		buff = lo_FetchParamValue(context, tag, PARAM_WIDTH);
		if (buff != NULL)
		{
			PA_LOCK(str, char *, buff);
			val = XP_ATOI(str);
			if (val < 0)
			{
				val = 0;
			}
			if (val > MAX_BUTTON_WIDTH)
			{
				val = MAX_BUTTON_WIDTH;
			}
			PA_UNLOCK(buff);
			PA_FREE(buff);
			form_element->width = val;
		}

		buff = lo_FetchParamValue(context, tag, PARAM_HEIGHT);
		if (buff != NULL)
		{
			PA_LOCK(str, char *, buff);
			val = XP_ATOI(str);
			if (val < 0)
			{
				val = 0;
			}
			if (val > MAX_BUTTON_HEIGHT)
			{
				val = MAX_BUTTON_HEIGHT;
			}
			PA_UNLOCK(buff);
			PA_FREE(buff);
			form_element->height = val;
		}
	}

#ifdef NEED_ISO_BACK_TRANSLATE
	if ((type != FORM_TYPE_HIDDEN)&&(type != FORM_TYPE_KEYGEN))
	{
		char *tptr;
		int16 charset;

		PA_LOCK(tptr, char *, form_data->value);
		charset = form_element->text_attr->charset;
		tptr = FE_TranslateISOText(context, charset, tptr);
		PA_UNLOCK(form_data->value);
	}
#endif

	return(form_element);
}


static LO_FormElementStruct *
lo_form_input_toggle(MWContext *context, lo_DocState *state,
	PA_Tag *tag, int32 type)
{
	LO_FormElementStruct *form_element;
	lo_FormElementToggleData *form_data;
	PA_Block buff;

	form_element = new_form_element(context, state, type);
	if (form_element == NULL)
	{
		return(NULL);
	}

	form_element->element_data = lo_next_element_data(state, type);
	if (form_element->element_data == NULL)
	{
		lo_FreeElement(context, (LO_Element *)form_element, FALSE);
		return(NULL);
	}
	form_element->element_index =
		state->top_state->savedData.FormList->data_index - 1;
	form_data = (lo_FormElementToggleData *)form_element->element_data;

	form_data->default_toggle = FALSE;
	buff = lo_FetchParamValue(context, tag, PARAM_CHECKED);
	if (buff != NULL)
	{
		form_data->default_toggle = TRUE;
		PA_FREE(buff);
	}

	/*
	 * IF our starting type is NONE, this is a new element,
	 * an we need to initialize the locations that will later
	 * hold persistent data.
	 */
	if (form_data->type == FORM_TYPE_NONE)
	{
		form_data->FE_Data = NULL;
		form_data->toggled = form_data->default_toggle;
	}

	form_data->type = type;
	form_data->name = lo_FetchParamValue(context, tag, PARAM_NAME);

	form_data->value = lo_FetchParamValue(context, tag, PARAM_VALUE);
	if (form_data->value == NULL)
	{
		form_data->value = PA_ALLOC(
			XP_STRLEN(DEF_TOGGLE_VALUE) + 1);
		if (form_data->value != NULL)
		{
			char *value;

			PA_LOCK(value, char *, form_data->value);
			XP_STRCPY(value, DEF_TOGGLE_VALUE);
			PA_UNLOCK(form_data->value);
		}
		else
		{
			state->top_state->out_of_memory = TRUE;
		}
	}

	return(form_element);
}


static LO_FormElementStruct *
lo_form_input_object(MWContext *context, lo_DocState *state,
	PA_Tag *tag, int32 type)
{
#ifdef JAVA
	LO_FormElementStruct *form_element;
	lo_FormElementObjectData *form_data;

	form_element = new_form_element(context, state, type);
	if (form_element == NULL)
	{
		return(NULL);
	}

	form_element->element_data = lo_next_element_data(state, type);
	if (form_element->element_data == NULL)
	{
		lo_FreeElement(context, (LO_Element *)form_element, FALSE);
		return(NULL);
	}
	form_element->element_index =
		state->top_state->savedData.FormList->data_index - 1;
	form_data = (lo_FormElementObjectData *)form_element->element_data;

	/*
	 * IF our starting type is NONE, this is a new element,
	 * an we need to initialize the locations that will later
	 * hold persistent data.
	 */
	if (form_data->type == FORM_TYPE_NONE)
	{
		form_data->FE_Data = NULL;
	}

	form_data->type = type;

	form_data->name = lo_FetchParamValue(context, tag, PARAM_NAME);
	if (form_data->name == NULL)
	{
		lo_FreeElement(context, (LO_Element *)form_element, FALSE);
		return(NULL);
	}

	/*
	 * Find the layout element for the object tag, and set form_data->object
	 * to point to it.
	 */
	{
	LO_JavaAppStruct *java_obj = lo_GetCurrentDocLists(state)->applet_list;
	while (java_obj)
	{
		if (java_obj->selector_type == LO_JAVA_SELECTOR_OBJECT_JAVABEAN)
		{
			char *name, *obj_name;

			PA_LOCK(name, char *, form_data->name);
			PA_LOCK(obj_name, char *, java_obj->attr_name);

			if (XP_STRCMP(name, obj_name) == 0)
			{
				form_data->object = java_obj;
				break;
			}

			PA_UNLOCK(form_data->name);
			PA_UNLOCK(java_obj->attr_name);
		}

		java_obj = java_obj->nextApplet;
	}

	if (java_obj == NULL)
	{
		lo_FreeElement(context, (LO_Element *)form_element, FALSE);
		return(NULL);
	}
	}

	return(form_element);
#else
    return NULL;
#endif

}


static LO_FormElementStruct *
lo_form_select(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_FormElementStruct *form_element;
	lo_FormElementSelectData *form_data;
	PA_Block buff;
	char *str;
	XP_Bool multiple;
	int32 size;
	int32 type;

	/*
	 * We have to determine type early so we can pass it to
	 * new_form_element(), so fetch and check the MULTIPLE and SIZE parameters
	 * right away.
	 */
	multiple = FALSE;
	buff = lo_FetchParamValue(context, tag, PARAM_MULTIPLE);
	if (buff != NULL)
	{
		multiple = TRUE;
		PA_FREE(buff);
	}

	size = 0;
	buff = lo_FetchParamValue(context, tag, PARAM_SIZE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		size = XP_ATOI(str);
		PA_UNLOCK(buff);
		PA_FREE(buff);
		if (size < 1)
		{
			size = 1;
		}
	}

	if ((multiple != FALSE)||(size > 1))
	{
		type = FORM_TYPE_SELECT_MULT;
	}
	else
	{
		type = FORM_TYPE_SELECT_ONE;
	}

	form_element = new_form_element(context, state, type);
	if (form_element == NULL)
	{
		return(NULL);
	}

	form_element->element_data = lo_next_element_data(state, type);
	if (form_element->element_data == NULL)
	{
		lo_FreeElement(context, (LO_Element *)form_element, FALSE);
		return(NULL);
	}
	form_element->element_index =
		state->top_state->savedData.FormList->data_index - 1;
	form_data = (lo_FormElementSelectData *)form_element->element_data;

	/*
	 * IF our starting type is NONE, this is a new element,
	 * an we need to initialize the locations that will later
	 * hold persistent data.
	 */
	if (form_data->type == FORM_TYPE_NONE)
	{
		form_data->FE_Data = NULL;
		form_data->multiple = FALSE;
		form_data->options_valid = FALSE;
		form_data->option_cnt = 0;
		form_data->options = NULL;
	}

	form_data->name = lo_FetchParamValue(context, tag, PARAM_NAME);

	form_data->multiple = multiple;
	form_data->size = size;
	form_data->type = type;

	/*
	 * Selects can set a preferred minimum width
	 * in pixels that may be larger that what is needed
	 * to display option text.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_WIDTH);
	if (buff != NULL)
	{
		int32 val;

		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 0)
		{
			val = 0;
		}
		if (val > MAX_BUTTON_WIDTH)
		{
			val = MAX_BUTTON_WIDTH;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		form_element->width = val;
	}

	/*
	 * Single selects can also set a preferred minimum height
	 * in pixels that may be larger that what is needed
	 * to display button text.  Multi selects must be the height
	 * of the number of items in the list.
	 */
	if (form_data->type == FORM_TYPE_SELECT_ONE)
	{
		int32 val;

		buff = lo_FetchParamValue(context, tag, PARAM_HEIGHT);
		if (buff != NULL)
		{
			PA_LOCK(str, char *, buff);
			val = XP_ATOI(str);
			if (val < 0)
			{
				val = 0;
			}
			if (val > MAX_BUTTON_HEIGHT)
			{
				val = MAX_BUTTON_HEIGHT;
			}
			PA_UNLOCK(buff);
			PA_FREE(buff);
			form_element->height = val;
		}
	}



	/*
	 * Make a copy of the tag so that we can correctly reflect this
	 *   element into javascript.  Reflection can't happen until this
	 *   element gets sent to lo_add_form_element() but by then the
	 *   original tag is gone.
	 */
	if (form_element != NULL && form_element->element_data != NULL)
	{
		form_data = (lo_FormElementSelectData *)
			form_element->element_data;

		if (form_data->saved_tag == NULL)
    			form_data->saved_tag = PA_CloneMDLTag(tag);
	}

#ifdef MOCHA
	/* needs to be moved */
	lo_ReflectFormElement(context, state, tag, form_element);
#endif

	return(form_element);
}


static LO_FormElementStruct *
lo_form_keygen(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_FormElementStruct *form_element;
	lo_FormElementKeygenData *form_data;

	form_element = new_form_element(context, state, FORM_TYPE_KEYGEN);
	if (form_element == NULL)
	{
		return(NULL);
	}

	form_element->element_data = lo_next_element_data(state, FORM_TYPE_KEYGEN);
	if (form_element->element_data == NULL)
	{
		lo_FreeElement(context, (LO_Element *)form_element, FALSE);
		return(NULL);
	}
	form_element->element_index =
		state->top_state->savedData.FormList->data_index - 1;
	form_data = (lo_FormElementKeygenData *)form_element->element_data;

	/*
	 * IF our starting type is NONE, this is a new element,
	 * and we need to initialize the locations that will later
	 * hold persistent data.
	 */
	if (form_data->type == FORM_TYPE_NONE)
	{
		form_data->value_str = NULL;
	}

	form_data->type = FORM_TYPE_KEYGEN;
	form_data->name = lo_FetchParamValue(context, tag, PARAM_NAME);
	form_data->challenge = lo_FetchParamValue(context, tag, PARAM_CHALLENGE);
	form_data->pqg = lo_FetchParamValue(context, tag, PARAM_PQG);
	form_data->key_type = lo_FetchParamValue(context, tag, PARAM_KEYTYPE);
	form_data->dialog_done = PR_FALSE;

	return(form_element);
}


static Bool
lo_add_element_to_form_list(MWContext *context, lo_DocState *state,
	LO_FormElementStruct *form_element)
{
	lo_FormData *form_list;
	LO_Element **ele_array;
	PA_Block old_form_elements; /* really a (LO_Element **) */
	lo_DocLists *doc_lists;

	doc_lists = lo_GetCurrentDocLists(state);
	form_list = doc_lists->form_list;
	if (form_list == NULL)
	{
		return(FALSE);
	}

	form_list->form_ele_cnt++;
#ifdef XP_WIN16
	if ((form_list->form_ele_cnt * sizeof(LO_Element *)) > SIZE_LIMIT)
	{
		form_list->form_ele_cnt--;
		return(FALSE);
	}
#endif /* XP_WIN16 */
	old_form_elements = NULL;
	if (form_list->form_ele_cnt > form_list->form_ele_size)
	{
		if (form_list->form_ele_size == 0)
		{
			form_list->form_elements = PA_ALLOC(
				form_list->form_ele_cnt * sizeof(LO_Element *));
		}
		else
		{
			old_form_elements = form_list->form_elements;
			form_list->form_elements = PA_REALLOC(
				form_list->form_elements,
				(form_list->form_ele_cnt *
				sizeof(LO_Element *)));
		}
#ifdef MOCHA
		if (form_list->form_elements != NULL)
		{
			int32 i;

			PA_LOCK(ele_array, LO_Element **,
				form_list->form_elements);
			for (i = form_list->form_ele_size;
			     i < form_list->form_ele_cnt;
			     i++)
			{
				ele_array[i] = NULL;
			}
			PA_UNLOCK(form_list->form_elements);
		}
#endif
		form_list->form_ele_size = form_list->form_ele_cnt;
	}
	if (form_list->form_elements == NULL)
	{
		form_list->form_elements = old_form_elements;
		form_list->form_ele_cnt--;
		state->top_state->out_of_memory = TRUE;
		return(FALSE);
	}
	PA_LOCK(ele_array, LO_Element **, form_list->form_elements);
#ifdef MOCHA
	{
		LO_FormElementStruct *old_form_ele;

		old_form_ele = (LO_FormElementStruct *)
			ele_array[form_list->form_ele_cnt - 1];
		if ((old_form_ele != NULL) &&
		    (form_element->mocha_object == NULL))
		{
			form_element->mocha_object = old_form_ele->mocha_object;
		}
	}
#endif
	ele_array[form_list->form_ele_cnt - 1] = (LO_Element *)form_element;
	PA_UNLOCK(form_list->form_elements);
	return(TRUE);
}


void
lo_LayoutInflowFormElement(MWContext *context,
						   lo_DocState *state,
						   LO_FormElementStruct *form_element,
						   int32 *baseline_inc,
						   Bool inRelayout)
{
	Bool line_break;

	form_element->ele_id = NEXT_ELEMENT;
	form_element->x = state->x;
	form_element->y = state->y;
	form_element->y_offset = 0;
	form_element->x_offset = 0;
	
	/*
	 * Bad HTML can make thing like linebreaks occur between 
	 * SELECT start and end.  Catch that here and clean up.
	 * Need to reset element ID too to maintain ever increasing order.
	 */
	if ((form_element->x != state->x)||(form_element->y != state->y))
	{
		form_element->ele_id = NEXT_ELEMENT;
		form_element->x = state->x;
		form_element->y = state->y;
	}
	form_element->baseline = 0;
	FE_GetFormElementInfo(context, form_element);

	if ((state->x + form_element->width) > state->right_margin)
	{
		line_break = TRUE;
	}
	else
	{
		line_break = FALSE;
	}

	/*
	 * if we are at the beginning of the line.  There is
	 * no point in breaking, we are just too wide.
	 * Also don't break in unwrapped preformatted text.
	 * Also can't break inside a NOBR section.
	 */
	if ((state->at_begin_line != FALSE)||
		(state->preformatted == PRE_TEXT_YES)||
		(state->breakable == FALSE))
	{
		line_break = FALSE;
	}

	/*
	 * Break on the form element if we have
	 * a break.  If this happens we need to reset the
	 * element id to maintain an ever increasing order.
	 */
	if (line_break != FALSE)
	{
		state->top_state->element_id = form_element->ele_id;

		if (!inRelayout)
		{
			lo_SoftLineBreak(context, state, TRUE);
		}
		else 
		{
			lo_rl_AddSoftBreakAndFlushLine(context, state);
		}

		form_element->ele_id = NEXT_ELEMENT;
		form_element->x = state->x;
		form_element->y = state->y;
	}

	/*
	 * The baseline of the form element just added to the line may be
	 * less than or greater than the baseline of the rest of the line.
	 * If the baseline is less, this is easy, we just increase
	 * y_offest to move the text down so the baselines
	 * line up.  For greater baselines, we can't move the element up to
	 * line up the baselines because we will overlay the previous line,
	 * so we have to move all the previous elements in this line down.
	 *
	 * If the baseline is zero, we are the first element on the line,
	 * and we get to set the baseline.
	 */
	*baseline_inc = 0;
	if (state->baseline == 0)
	{
		state->baseline = (intn) form_element->baseline;
		state->line_height = (intn) form_element->height;
	}
	else if (form_element->baseline < state->baseline)
	{
		form_element->y_offset = state->baseline -
			form_element->baseline;
	}
	else
	{
		*baseline_inc = form_element->baseline - state->baseline;
	}
}

void
lo_UpdateStateAfterFormElement(MWContext *context, lo_DocState *state,
							   LO_FormElementStruct *form_element,
							   int32 baseline_inc)
{
  state->baseline += (intn) baseline_inc;
  state->line_height += (intn) baseline_inc;
  
  if ((form_element->y_offset + form_element->height) >
	  state->line_height)
	{
	  state->line_height = form_element->y_offset +
		form_element->height;
	}
  
  state->x = state->x + form_element->x_offset + form_element->width;
  state->linefeed_state = 0;
  state->at_begin_line = FALSE;
  state->cur_ele_type = LO_FORM_ELE;
  state->trailing_space = FALSE;
}

static void
lo_add_form_element(MWContext *context, lo_DocState *state,
	LO_FormElementStruct *form_element)
{
	int32 baseline_inc;

	if (XP_FAIL_ASSERT(context))
		return;
	
	if (lo_add_element_to_form_list(context, state, form_element) == FALSE)
	{
		return;
	}

	lo_LayoutInflowFormElement(context, state, form_element, &baseline_inc, FALSE);

	lo_AppendToLineList(context, state,
		(LO_Element *)form_element, baseline_inc);

	lo_UpdateStateAfterFormElement(context, state, form_element, baseline_inc);

	form_element->layer =
	  lo_CreateEmbeddedObjectLayer(context, state,
				       (LO_Element*)form_element);
}


/*
 * Function to reallocate the lo_FormElementOptionData array pointed at by
 * lo_FormElementSelectData's options member to include space for the number
 * of options given by form_data->options_cnt.
 *
 * This function is exported to libmocha/lm_input.c.
 */
XP_Bool
LO_ResizeSelectOptions(lo_FormElementSelectData *form_data)
{
	PA_Block old_options; /* really a (lo_FormElementOptionData *) */

	if (form_data->option_cnt == 0)
	{
		if (form_data->options != NULL)
		{
			PA_FREE(form_data->options);
			form_data->options = NULL;
		}
		return TRUE;
	}
#ifdef XP_WIN16
	if ((form_data->option_cnt * sizeof(lo_FormElementOptionData)) >
		SIZE_LIMIT)
	{
		return FALSE;
	}
#endif /* XP_WIN16 */
	old_options = form_data->options;
	if (old_options == NULL)
	{
		form_data->options = PA_ALLOC(form_data->option_cnt
			* sizeof(lo_FormElementOptionData));
	}
	else
	{
		form_data->options = PA_REALLOC(
			form_data->options, (form_data->option_cnt *
			sizeof(lo_FormElementOptionData)));
	}
	if (form_data->options == NULL)
	{
		form_data->options = old_options;
		return FALSE;
	}

	return TRUE;
}


void
lo_BeginOptionTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_FormElementStruct *form_element;
	lo_FormElementSelectData *form_data;
	lo_FormElementOptionData *options;
	lo_FormElementOptionData *optr;
	PA_Block buff;

	form_element = (LO_FormElementStruct *)state->current_ele;
	if ((form_element == NULL)||(form_element->element_data == NULL))
	{
		return;
	}

	form_data = (lo_FormElementSelectData *)form_element->element_data;
	if (form_data->options_valid != FALSE)
	{
		return;
	}

	form_data->option_cnt++;
	if (!LO_ResizeSelectOptions(form_data))
	{
		state->top_state->out_of_memory = TRUE;
		form_data->option_cnt--;
		return;
	}

	PA_LOCK(options, lo_FormElementOptionData *,
		form_data->options);
	optr = &(options[form_data->option_cnt - 1]);
	optr->text_value = NULL;
	optr->selected = FALSE;

	optr->value = lo_FetchParamValue(context, tag, PARAM_VALUE);

	optr->def_selected = FALSE;
	buff = lo_FetchParamValue(context, tag, PARAM_SELECTED);
	if (buff != NULL)
	{
		optr->def_selected = TRUE;
		PA_FREE(buff);
	}

	PA_UNLOCK(form_data->options);
}


void
lo_EndOptionTag(MWContext *context, lo_DocState *state, PA_Block buff)
{
	LO_FormElementStruct *form_element;
	lo_FormElementSelectData *form_data;
	lo_FormElementOptionData *options;
	lo_FormElementOptionData *optr;
#ifdef NEED_ISO_BACK_TRANSLATE
	char *tptr;
	int16 charset;
#endif

	form_element = (LO_FormElementStruct *)state->current_ele;
	if ((form_element == NULL)||(form_element->element_data == NULL))
	{
		return;
	}

	form_data = (lo_FormElementSelectData *)form_element->element_data;
	if (form_data->options_valid != FALSE)
	{
		PA_FREE(buff);
		return;
	}

	PA_LOCK(options, lo_FormElementOptionData *, form_data->options);
	optr = &(options[form_data->option_cnt - 1]);

#ifdef NEED_ISO_BACK_TRANSLATE
	PA_LOCK(tptr, char *, buff);
	charset = form_element->text_attr->charset;
	tptr = FE_TranslateISOText(context, charset, tptr);
	PA_UNLOCK(buff);
#endif
	optr->text_value = buff;

	PA_UNLOCK(form_data->options);
}


void
lo_BeginSelectTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_FormElementStruct *form_element;

	form_element = lo_form_select(context, state, tag);

	/*
	 * Make a copy of the tag so that we can correctly reflect this
	 *   element into javascript.  Reflection can't happen until this
	 *   element gets sent to lo_add_form_element() but by then the
	 *   original tag is gone.
	 */
	if (form_element != NULL && form_element->element_data != NULL)
	{
		lo_FormElementSelectData *form_data;
		form_data = (lo_FormElementSelectData *)
			form_element->element_data;
		
		if (form_data->saved_tag == NULL)
			form_data->saved_tag = PA_CloneMDLTag(tag);
	}

	state->current_ele = (LO_Element *)form_element;
	state->cur_ele_type = LO_FORM_ELE;
}


void
lo_EndSelectTag(MWContext *context, lo_DocState *state)
{
	LO_FormElementStruct *form_element;
	lo_FormElementSelectData *form_data;
	lo_FormElementOptionData *options;

	form_element = (LO_FormElementStruct *)state->current_ele;
	if (form_element != NULL)
	{
		form_data = (lo_FormElementSelectData *)
				form_element->element_data;
		/*
		 * If we have element data for a select one
		 * selection menu, sanify the data in case the user
		 * specified more or less than 1 item to start
		 * selected.
		 */
		if ((form_data != NULL)&&
			(form_data->type == FORM_TYPE_SELECT_ONE))
		{
			Bool sel_set;
			int32 i;

			if (form_data->option_cnt > 0)
			{
				PA_LOCK(options,
					lo_FormElementOptionData *,
					form_data->options);
				sel_set = FALSE;
				for (i=0; i < form_data->option_cnt; i++)
				{
					if (options[i].def_selected != FALSE)
					{
						if (sel_set != FALSE)
						{
							options[i].def_selected
								 = FALSE;
						}
						else
						{
							sel_set = TRUE;
						}
					}
				}
				if (sel_set == FALSE)
				{
					options[0].def_selected = TRUE;
				}
				PA_UNLOCK(form_data->options);
			}
		}

		if (form_data != NULL)
		{

			if ((form_data->size < 1)&&
				(form_data->type == FORM_TYPE_SELECT_MULT))
			{
				form_data->size = form_data->option_cnt;
				if (form_data->size < 1)
				{
					form_data->size = 1;
				}
			}
			else if (form_data->size < 1)
			{
				form_data->size = DEF_SELECT_ELE_SIZE;
			}

			form_data->options_valid = TRUE;
		}

		lo_add_form_element(context, state, form_element);

		/*
		 * Now that we have added the element to the form list we
		 *   can reflect it now.  
		 */
		if (form_data != NULL)
		{
			lo_ReflectFormElement(context, state, 
					      form_data->saved_tag, 
					      form_element);
			PA_FreeTag(form_data->saved_tag);
			form_data->saved_tag = NULL;
		}
	
	}
	state->current_ele = NULL;
}


void
lo_BeginTextareaTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_FormElementStruct *form_element;
	LO_TextAttr tmp_attr;
	LO_TextAttr *old_attr;
	LO_TextAttr *attr;

	old_attr = state->font_stack->text_attr;
	lo_CopyTextAttr(old_attr, &tmp_attr);
	tmp_attr.fontmask |= LO_FONT_FIXED;
	attr = lo_FetchTextAttr(state, &tmp_attr);

	lo_PushFont(state, tag->type, attr);

	form_element = lo_form_textarea(context, state, tag,
		FORM_TYPE_TEXTAREA);

	/*
	 * Make a copy of the tag so that we can correctly reflect this
	 *   element into javascript.  Reflection can't happen until this
	 *   element gets sent to lo_add_form_element() but by then the
	 *   original tag is gone.
	 */
	if (form_element != NULL && form_element->element_data != NULL)
	{
		lo_FormElementTextareaData *form_data;
		form_data = (lo_FormElementTextareaData *)
			form_element->element_data;
		
		if (form_data->saved_tag == NULL)
			form_data->saved_tag = PA_CloneMDLTag(tag);
	}

	attr = lo_PopFont(state, tag->type);

	state->current_ele = (LO_Element *)form_element;
        state->cur_ele_type = LO_FORM_ELE;
}


void
lo_EndTextareaTag(MWContext *context, lo_DocState *state, PA_Block buff)
{
	LO_FormElementStruct *form_element;
	char *tptr;

	form_element = (LO_FormElementStruct *)state->current_ele;
	if (form_element != NULL)
	{
		lo_FormElementTextareaData *form_data;

		form_data = (lo_FormElementTextareaData *)
			form_element->element_data;
		if (form_data != NULL)
		{
			int16 charset;

			PA_LOCK(tptr, char *, buff);
			charset = form_element->text_attr->charset;
			tptr = FE_TranslateISOText(context, charset, tptr);
			PA_UNLOCK(buff);
			form_data->default_text = buff;
		}
		lo_add_form_element(context, state, form_element);

		/*
		 * Now that we have added the element to the form list we
		 *   can reflect it now.
		 */
		if (form_data != NULL)
		{
			lo_ReflectFormElement(context, state, 
					      form_data->saved_tag, 
					      form_element);
			PA_FreeTag(form_data->saved_tag);
			form_data->saved_tag = NULL;
		}
	}
	state->current_ele = NULL;
}


void
lo_ProcessInputTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_FormElementStruct *form_element;
	PA_Block buff;
	char *str;
	int32 type;
	Bool disabled;

	type = FORM_TYPE_TEXT;
	buff = lo_FetchParamValue(context, tag, PARAM_TYPE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		type = lo_ResolveInputType(str);
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	disabled = FALSE;
	buff = lo_FetchParamValue(context, tag, PARAM_DISABLED);
	if (buff != NULL)
	{
		PA_FREE(buff);
		disabled = TRUE;
	}

	switch (type)
	{
		case FORM_TYPE_IMAGE:
			lo_FormatImage(context, state, tag);
			/*
			 * Now check to see if this INPUT image was
			 * prefetched earlier as a fake image.  If so
			 * we can free up that fake image now.
			 */
			if (tag->lo_data != NULL)
			{
                LO_ImageStruct *image = tag->lo_data;

                if (image->image_req)
                    IL_DestroyImage(image->image_req);
				lo_FreeElement(context,	(LO_Element *)image, TRUE);
				tag->lo_data = NULL;
			}
			return;
			break;
		case FORM_TYPE_TEXT:
		case FORM_TYPE_PASSWORD:
		case FORM_TYPE_FILE:
			{
				LO_TextAttr tmp_attr;
				LO_TextAttr *old_attr;
				LO_TextAttr *attr;

				old_attr = state->font_stack->text_attr;
				lo_CopyTextAttr(old_attr, &tmp_attr);
				tmp_attr.fontmask |= LO_FONT_FIXED;
				attr = lo_FetchTextAttr(state, &tmp_attr);

				lo_PushFont(state, tag->type, attr);

				form_element = lo_form_input_text(context,
					state, tag, type);

				attr = lo_PopFont(state, tag->type);
			}
			break;
		case FORM_TYPE_SUBMIT:
		case FORM_TYPE_RESET:
		case FORM_TYPE_BUTTON:
		case FORM_TYPE_HIDDEN:
		case FORM_TYPE_READONLY:
			form_element = lo_form_input_minimal(context, state,
				tag, type);
			break;
		case FORM_TYPE_RADIO:
		case FORM_TYPE_CHECKBOX:
			form_element = lo_form_input_toggle(context, state,
				tag, type);
			break;
		case FORM_TYPE_OBJECT:
			form_element = lo_form_input_object(context, state,
				tag, type);
			break;
		case FORM_TYPE_JOT:
		default:
			form_element = NULL;
			break;
	}

	if (form_element == NULL)
	{
		return;
	}

	if (type == FORM_TYPE_HIDDEN)
	{
		Bool status;

		status = lo_add_element_to_form_list(context, state,
			form_element);
	}
	else
	{
		if (type != FORM_TYPE_OBJECT) /* can't disable objects? */
		  {
			form_element->element_data->ele_minimal.disabled = disabled;
		  }

		lo_add_form_element(context, state, form_element);
	}

#ifdef MOCHA
	lo_ReflectFormElement(context, state, tag, form_element);
#endif
}


static PA_Block
lo_dup_block(PA_Block block)
{
	PA_Block new_block;
	char *str;
	char *str2;

	if (block != NULL)
	{
		PA_LOCK(str, char *, block);
		new_block = PA_ALLOC(XP_STRLEN(str) + 1);
		if (new_block != NULL)
		{
			PA_LOCK(str2, char *, new_block);
			XP_STRCPY(str2, str);
			PA_UNLOCK(new_block);
		}
		PA_UNLOCK(block);
	}
	else
	{
		new_block = NULL;
	}

	return(new_block);
}


/*
 * Returns true if the submit should be aborted (likely to be restarted
 * later), false otherwise.
 */
static PRBool
lo_get_form_element_data(MWContext *context,
		LO_FormElementStruct *submit_element, LO_Element *element,
		PA_Block *name_array, PA_Block *value_array, 
		uint8 *type_array, uint8 *encoding_array, int32 *array_cnt)
{
	LO_FormElementStruct *form_element;
	PA_Block name;
	PA_Block value;
	uint8 type;
	uint8 encoding;

	name = NULL;
	value = NULL;
	type = FORM_TYPE_NONE;
	encoding = INPUT_TYPE_ENCODING_NORMAL;

	if ((element == NULL)||(element->type != LO_FORM_ELE))
	{
		return PR_FALSE;
	}

	form_element = (LO_FormElementStruct *)element;
	if (form_element->element_data == NULL)
	{
		return PR_FALSE;
	}

#ifdef XP_WIN16
	if ((*array_cnt * sizeof(XP_Block)) >= SIZE_LIMIT)
	{
		return PR_FALSE;
	}
#endif /* XP_WIN16 */

	if ((form_element->element_data->type != FORM_TYPE_HIDDEN)&&
	    (form_element->element_data->type != FORM_TYPE_KEYGEN)&&
	    (form_element->element_data->type != FORM_TYPE_SUBMIT)&&
	    (form_element->element_data->type != FORM_TYPE_RESET)&&
	    (form_element->element_data->type != FORM_TYPE_READONLY)&&
	    (form_element->element_data->type != FORM_TYPE_BUTTON) &&
	    (form_element->element_data->type != FORM_TYPE_OBJECT))
	{
		FE_GetFormElementValue(context, form_element, FALSE);
	}

	switch (form_element->element_data->type)
	{
		case FORM_TYPE_TEXT:
		case FORM_TYPE_PASSWORD:
		case FORM_TYPE_FILE:
		    {
			lo_FormElementTextData *form_data;

			form_data = (lo_FormElementTextData *)
				form_element->element_data;

#if DISABLED_READONLY_SUPPORT
			if (form_data->disabled == TRUE)
			  /* if a form element is disabled, it's name/value pair isn't sent
				 to the server. */
			  break;
#endif

			if (form_data->name != NULL)
			{
				name = lo_dup_block(form_data->name);
				value = lo_dup_block(form_data->current_text);
				type = (uint8)form_data->type;
				encoding = (uint8)form_data->encoding;
			}
		    }
			break;
		case FORM_TYPE_TEXTAREA:
		    {
			lo_FormElementTextareaData *form_data;

			form_data = (lo_FormElementTextareaData *)
				form_element->element_data;

#if DISABLED_READONLY_SUPPORT
			if (form_data->disabled == TRUE)
			  /* if a form element is disabled, it's name/value pair isn't sent
				 to the server. */
			  break;
#endif

			if (form_data->name != NULL)
			{
				name = lo_dup_block(form_data->name);
				value = lo_FEToNetLinebreaks(
						form_data->current_text);
				type = (uint8)form_data->type;
			}
		    }
			break;
		/*
		 * Only submit the name/value for the submit button
		 * pressed.
		 */
		case FORM_TYPE_SUBMIT:
		case FORM_TYPE_RESET:
		case FORM_TYPE_BUTTON:
		    if (form_element == submit_element)
		    {
			lo_FormElementMinimalData *form_data;
			form_data = (lo_FormElementMinimalData *)
				form_element->element_data;

#if DISABLED_READONLY_SUPPORT
			if (form_data->disabled == TRUE)
			  /* if a form element is disabled, it's name/value pair isn't sent
				 to the server. */
			  break;
#endif

			if (form_data->name != NULL)
			{
				name = lo_dup_block(form_data->name);
				value = lo_dup_block(form_data->value);
				type = (uint8)form_data->type;
			}
		    }
			break;
		case FORM_TYPE_HIDDEN:
		case FORM_TYPE_READONLY:
		    {
			lo_FormElementMinimalData *form_data;
			form_data = (lo_FormElementMinimalData *)
				form_element->element_data;

#if DISABLED_READONLY_SUPPORT
			if (form_data->disabled == TRUE)
			  /* if a form element is disabled, it's name/value pair isn't sent
				 to the server. */
			  break;
#endif

			if (form_data->name != NULL)
			{
				name = lo_dup_block(form_data->name);
				value = lo_dup_block(form_data->value);
				type = (uint8)form_data->type;
			}
		    }
			break;
		case FORM_TYPE_RADIO:
		case FORM_TYPE_CHECKBOX:
		    {
			lo_FormElementToggleData *form_data;
			form_data = (lo_FormElementToggleData *)
				form_element->element_data;

#if DISABLED_READONLY_SUPPORT
			if (form_data->disabled == TRUE)
			  /* if a form element is disabled, it's name/value pair isn't sent
				 to the server. */
			  break;
#endif

			if ((form_data->toggled != FALSE)&&
				(form_data->name != NULL))
			{
				name = lo_dup_block(form_data->name);
				value = lo_dup_block(form_data->value);
				type = (uint8)form_data->type;
			}
		    }
			break;
		case FORM_TYPE_SELECT_ONE:
		case FORM_TYPE_SELECT_MULT:
		    {
			int i;
			lo_FormElementSelectData *form_data;
			lo_FormElementOptionData *options;
			form_data = (lo_FormElementSelectData *)
				form_element->element_data;

#if DISABLED_READONLY_SUPPORT
			if (form_data->disabled == TRUE)
			  /* if a form element is disabled, it's name/value pair isn't sent
				 to the server. */
			  break;
#endif

			if ((form_data->name != NULL)&&
				(form_data->option_cnt > 0))
			{
			    PA_LOCK(options, lo_FormElementOptionData *,
				form_data->options);
			    for (i=0; i < form_data->option_cnt; i++)
			    {
				if (options[i].selected == FALSE)
				{
					continue;
				}
				name = lo_dup_block(form_data->name);
				if (options[i].value == NULL)
				{
				    value = lo_dup_block(options[i].text_value);
				}
				else
				{
				    value = lo_dup_block(options[i].value);
				}
				type = (uint8)form_data->type;
				if (form_element->element_data->type ==
					FORM_TYPE_SELECT_ONE)
				{
					break;
				}
				name_array[*array_cnt] = name;
				value_array[*array_cnt] = value;
				encoding_array[*array_cnt] = encoding;
				type_array[*array_cnt] = type;
				*array_cnt = *array_cnt + 1;
#ifdef XP_WIN16
				if ((*array_cnt * sizeof(XP_Block)) >=
					SIZE_LIMIT)
				{
					PA_UNLOCK(form_data->options);
					return;
				}
#endif /* XP_WIN16 */
			    }
			    PA_UNLOCK(form_data->options);
			    if (form_element->element_data->type ==
				FORM_TYPE_SELECT_MULT)
			    {
				return PR_FALSE;
			    }
			}
		    }
			break;
		case FORM_TYPE_KEYGEN:
		    {
			lo_FormElementKeygenData *form_data;
			char *cstr;	/* challenge */
			char *nstr;	/* name */
			char *tstr;	/* temp */
			char *vstr;	/* value */
			char *kstr; /* key type */
			char *pstr; /* pqg */

			/*
			 * A KEYGEN element means that the previous
			 * name/value pair should be diverted to
			 * key-generation.
			 */

			form_data = (lo_FormElementKeygenData *)
				form_element->element_data;
			if (form_data->name == NULL)
			{
				break;
			}

			/*
			 * Check that our name matches the previous name;
			 * this is just a sanity check.
			 */
			if (*array_cnt < 1)
			{
				/* no previous element at all */
				break;
			}
			PA_LOCK(tstr, char *, form_data->name);
			PA_LOCK(nstr, char *, name_array[*array_cnt - 1]);
			if (XP_STRCMP(nstr, tstr) == 0)
			{
				value = value_array[*array_cnt - 1];
			}
			PA_UNLOCK(name_array[*array_cnt - 1]);
			PA_UNLOCK(form_data->name);

			if (value == NULL)
			{
				/* name did not match; punt */
				break;
			}

			if (form_data->dialog_done == PR_FALSE)
			{
				PRBool wait;

				/*
				 * If value_str is not null, then we have an
				 * old string there which we should free
				 * before replacing.
				 * XXX This does not seem like quite the
				 * right location for this, but neither
				 * does any place else I can think of.
				 */
				if (form_data->value_str != NULL)
				{
					XP_FREE(form_data->value_str);
					form_data->value_str = NULL;
				}

				/*
				 * The value of the previous element is
				 * the key-size string.
				 */
				PA_LOCK(vstr, char *, value);
				PA_LOCK(cstr, char *, form_data->challenge);
				PA_LOCK(kstr, char *, form_data->key_type);
				PA_LOCK(pstr, char *, form_data->pqg);
				wait = SECNAV_GenKeyFromChoice(context,
						(LO_Element *)submit_element,
						vstr, cstr, kstr, pstr,
						&form_data->value_str,
						&form_data->dialog_done);
				PA_UNLOCK(form_data->pqg);
				PA_UNLOCK(form_data->key_type);
				PA_UNLOCK(form_data->challenge);
				PA_UNLOCK(value);

				/*
				 * We may have put up a dialog and be waiting
				 * now for the user to interact with it; if so,
				 * just return and the dialog code will call
				 * back later to really do the submit.
				 */
				if (wait != PR_FALSE)
				{
					return PR_TRUE;
				}
			}

			/*
			 * "erase" the previous entry, hanging onto the
			 * name which is already a good duplicate to use
			 */
			name = name_array[*array_cnt - 1];
			PA_FREE(value);
			value = NULL;
			*array_cnt = *array_cnt - 1;

			if (form_data->value_str == NULL)
			{
				/* something went wrong */
				break;
			}

			/*
			 * The real value, finally...
			 */
			value = PA_ALLOC(XP_STRLEN(form_data->value_str) + 1);
			if (value != NULL)
			{
				PA_LOCK(vstr, char *, value);
				XP_STRCPY(vstr, form_data->value_str);
				PA_UNLOCK(value);
			}
			type = (uint8)form_data->type;
		    }
			break;
#ifdef JAVA
		case FORM_TYPE_OBJECT:
		    {
			lo_FormElementObjectData *form_data;
			form_data = (lo_FormElementObjectData *)
				form_element->element_data;
			if (form_data->name != NULL)
			{
				char *object_value;
				char *vstr;

				name = lo_dup_block(form_data->name);

				object_value = LJ_Applet_GetText(form_data->object->session_data);
				value = PA_ALLOC(XP_STRLEN(object_value) + 1);
				if (value != NULL)
				{
					PA_LOCK(vstr, char *, value);
					XP_STRCPY(vstr, object_value);
					PA_UNLOCK(value);
				}
				XP_FREE(object_value);

				type = (uint8)form_data->type;
			}
		    }
			break;
#endif /* JAVA */
		case FORM_TYPE_JOT:
		default:
			break;
	}

	if (name != NULL)
	{
		name_array[*array_cnt] = name;
		value_array[*array_cnt] = value;
		type_array[*array_cnt] = type;
		encoding_array[*array_cnt] = encoding;
		*array_cnt = *array_cnt + 1;
	}

	return PR_FALSE;
}

static LO_FormSubmitData *
lo_GatherSubmitData(MWContext *context, lo_DocState *state,
		lo_FormData *form_list, LO_FormElementStruct *submit_element)
{
	intn i;
	LO_FormSubmitData *submit_data;
	PA_Block *name_array, *value_array;
	uint8 *type_array;
	uint8 *encoding_array;
	int32 array_size;
	LO_Element **ele_list;
	PRBool is_a_image;
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);

	is_a_image = (submit_element->type == LO_IMAGE) ? PR_TRUE : PR_FALSE;

	submit_data = XP_NEW_ZAP(LO_FormSubmitData);
	if (submit_data == NULL)
	{
		return(NULL);
	}

	if (form_list->action != NULL)
	{
		submit_data->action = lo_dup_block(form_list->action);
		if (submit_data->action == NULL)
		{
			LO_FreeSubmitData(submit_data);
			return(NULL);
		}
	}
	if (form_list->encoding != NULL)
	{
		submit_data->encoding = lo_dup_block(form_list->encoding);
		if (submit_data->encoding == NULL)
		{
			LO_FreeSubmitData(submit_data);
			return(NULL);
		}
	}
	if (form_list->window_target != NULL)
	{
		submit_data->window_target = lo_dup_block(form_list->window_target);
		if (submit_data->window_target == NULL)
		{
			LO_FreeSubmitData(submit_data);
			return(NULL);
		}
	}
	submit_data->method = form_list->method;

	array_size = form_list->form_ele_cnt;
	if (is_a_image)
	{
		array_size += 2;
	}

	if (array_size <= 0)
	{
		return(submit_data);
	}

	/*
	 * Look through all the form elements for a select element of
	 * type multiple, since that can have more than 1 name/value
	 * pair per element.
	 */
	PA_LOCK(ele_list, LO_Element **, form_list->form_elements);
	for (i=0; i<form_list->form_ele_cnt; i++)
	{
		LO_FormElementStruct *form_element;
		int32 ele_type;

		form_element = (LO_FormElementStruct *)ele_list[i];
		if ((form_element != NULL)&&
			(form_element->element_data != NULL))
		{
			ele_type = form_element->element_data->type;
			if (ele_type != FORM_TYPE_SELECT_MULT)
			{
				continue;
			}
		}
		else
		{
			continue;
		}

		array_size += form_element->element_data->ele_select.option_cnt;
	}
	PA_UNLOCK(form_list->form_elements);

#ifdef XP_WIN16
	if ((array_size * sizeof(XP_Block)) > SIZE_LIMIT)
	{
		array_size = SIZE_LIMIT / sizeof(XP_Block);
	}
#endif /* XP_WIN16 */

	submit_data->name_array = PA_ALLOC(array_size * sizeof(PA_Block));
	submit_data->value_array = PA_ALLOC(array_size * sizeof(PA_Block));
	submit_data->type_array = PA_ALLOC(array_size * sizeof(uint8));
	submit_data->encoding_array = PA_ALLOC(array_size * sizeof(uint8));

	if ((submit_data->name_array == NULL)
		|| (submit_data->value_array == NULL)
			|| (submit_data->type_array == NULL)
				|| (submit_data->encoding_array == NULL))
	{
		return(submit_data);
	}

	PA_LOCK(name_array, PA_Block *, submit_data->name_array);
	PA_LOCK(value_array, PA_Block *, submit_data->value_array);
	PA_LOCK(type_array, uint8 *, submit_data->type_array);
	PA_LOCK(encoding_array, uint8 *, submit_data->encoding_array);
	PA_LOCK(ele_list, LO_Element **, form_list->form_elements);
	for (i=0; i<form_list->form_ele_cnt; i++)
	{
		PRBool wait;

		wait = lo_get_form_element_data(context,
				submit_element, ele_list[i],
				name_array, value_array, type_array, encoding_array,
				&submit_data->value_cnt);
		if (wait != PR_FALSE)
		{
			/*
			 * We now go to let the user interact with
			 * the dialogs.  We will be back later!
			 */
			LO_FreeSubmitData(submit_data);
			return(NULL);
		}
	}

	/* After all the form data is gathered, we need to convert from
	 * the "native" front-end character code set to the one used
	 * in the HTML form.  Convert each name/value pair.
	 */
	{
	CCCDataObject	object;
	unsigned char	*s1, *s2;
	int16	doc_csid;

	object = INTL_CreateCharCodeConverter();
	if (object == NULL)
	{
		PA_UNLOCK(form_list->form_elements);
		PA_UNLOCK(submit_data->type_array);
		PA_UNLOCK(submit_data->value_array);
		PA_UNLOCK(submit_data->name_array);
		PA_UNLOCK(submit_data->encoding_array);
		return(submit_data);
	}

	doc_csid = INTL_GetCSIDocCSID(c);
	if (doc_csid == CS_DEFAULT)
		doc_csid = INTL_DefaultDocCharSetID(context);
	doc_csid &= (~CS_AUTO);

	if (INTL_GetCharCodeConverter(INTL_GetCSIWinCSID(c), doc_csid, object)) {
		for (i = 0; i < submit_data->value_cnt; i++) {
			s1 = (unsigned char *)name_array[i];
			s2 = INTL_CallCharCodeConverter(object, s1,
				XP_STRLEN((char *)s1));
			if (s1 != s2) {		/* didn't convert in-place */
				PA_FREE((PA_Block)s1);
				name_array[i] = lo_dup_block((PA_Block)s2);
				PA_FREE((PA_Block)s2);
			}
			if (value_array[i] != NULL)
			{
				s1 = (unsigned char *)value_array[i];
				s2 = INTL_CallCharCodeConverter(object, s1,
					XP_STRLEN((char *)s1));
				if (s1 != s2) {	/* didn't convert in-place */
					PA_FREE((PA_Block)s1);
					value_array[i] =
						lo_dup_block((PA_Block)s2);
					PA_FREE((PA_Block)s2);
				}
			}
		}
	}

	INTL_DestroyCharCodeConverter(object);
	}

	PA_UNLOCK(form_list->form_elements);
	PA_UNLOCK(submit_data->type_array);
	PA_UNLOCK(submit_data->value_array);
	PA_UNLOCK(submit_data->name_array);

	if (is_a_image)
	{
		LO_ImageStruct *image;
		PA_Block buff;
		PA_Block name_buff;
		char val_str[20];
		char *str;
		char *name;
		intn name_len;

		name_buff = NULL; /* Make gcc and Jamie happy */
		image = (LO_ImageStruct *)submit_element;
		PA_LOCK(name_array, PA_Block *, submit_data->name_array);
		PA_LOCK(value_array, PA_Block *,submit_data->value_array);
		if (image->alt != NULL)
		{
			PA_LOCK(name, char *, image->alt);
		}
		else
		{
			name_buff = PA_ALLOC(1);
			if (name_buff == NULL)
			{
				PA_UNLOCK(submit_data->value_array);
				PA_UNLOCK(submit_data->name_array);
				return(submit_data);
			}
			PA_LOCK(name, char *, name_buff);
			name[0] = '\0';
		}
		name_len = XP_STRLEN(name);

		buff = PA_ALLOC(name_len + 3);
		if (buff == NULL)
		{
			PA_UNLOCK(submit_data->value_array);
			PA_UNLOCK(submit_data->name_array);
			return(submit_data);
		}
		PA_LOCK(str, char *, buff);
		if (name_len > 0)
		{
			XP_STRCPY(str, name);
			XP_STRCAT(str, ".x");
		}
		else
		{
			XP_STRCPY(str, "x");
		}
		PA_UNLOCK(buff);
		name_array[submit_data->value_cnt] = buff;

		XP_SPRINTF(val_str, "%d", image->image_attr->submit_x);
		buff = PA_ALLOC(XP_STRLEN(val_str) + 1);
		if (buff == NULL)
		{
			PA_UNLOCK(submit_data->value_array);
			PA_UNLOCK(submit_data->name_array);
			return(submit_data);
		}
		PA_LOCK(str, char *, buff);
		XP_STRCPY(str, val_str);
		PA_UNLOCK(buff);
		value_array[submit_data->value_cnt] = buff;

		submit_data->value_cnt++;

		buff = PA_ALLOC(name_len + 3);
		if (buff == NULL)
		{
			PA_UNLOCK(submit_data->value_array);
			PA_UNLOCK(submit_data->name_array);
			return(submit_data);
		}
		PA_LOCK(str, char *, buff);
		if (name_len > 0)
		{
			XP_STRCPY(str, name);
			XP_STRCAT(str, ".y");
		}
		else
		{
			XP_STRCPY(str, "y");
		}
		PA_UNLOCK(buff);
		name_array[submit_data->value_cnt] = buff;

		XP_SPRINTF(val_str, "%d", image->image_attr->submit_y);
		buff = PA_ALLOC(XP_STRLEN(val_str) + 1);
		if (buff == NULL)
		{
			PA_UNLOCK(submit_data->value_array);
			PA_UNLOCK(submit_data->name_array);
			return(submit_data);
		}
		PA_LOCK(str, char *, buff);
		XP_STRCPY(str, val_str);
		PA_UNLOCK(buff);
		value_array[submit_data->value_cnt] = buff;

		submit_data->value_cnt++;

		if (image->alt != NULL)
		{
			PA_UNLOCK(image->alt);
		}
		else
		{
			PA_UNLOCK(name_buff);
			PA_FREE(name_buff);
		}
	}

	return(submit_data);
}


LO_FormSubmitData *
LO_SubmitForm(MWContext *context, LO_FormElementStruct *form_element)
{
	intn form_id;
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_FormSubmitData *submit_data;
	lo_FormData *form_list;
	lo_DocLists *doc_lists;

	/*
	 * Get the unique document ID, and retrieve this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return(NULL);
	}
	state = top_state->doc_state;

	if (form_element->type == LO_IMAGE)
	{
		LO_ImageStruct *image;

		image = (LO_ImageStruct *)form_element;
		form_id = image->image_attr->form_id;
	}
	else
	{
		form_id = form_element->form_id;
	}

	doc_lists = lo_GetDocListsById(state, form_element->layer_id);
	form_list = doc_lists->form_list;
	while (form_list != NULL)
	{
		if (form_list->id == form_id)
		{
			break;
		}
		form_list = form_list->next;
	}
	if (form_list == NULL)
	{
		return(NULL);
	}

	submit_data = lo_GatherSubmitData(context, state, form_list,
					form_element);
	if (submit_data == NULL)
	{
		return(NULL);
	}

	return(submit_data);
}


LO_FormSubmitData *
LO_SubmitImageForm(MWContext *context, LO_ImageStruct *image,
				int32 x, int32 y)
{
	image->image_attr->submit_x = (intn)x;
	image->image_attr->submit_y = (intn)y;

	return LO_SubmitForm(context, (LO_FormElementStruct *)image);
}


static void
lo_save_form_element_data(MWContext *context, LO_Element *element,
	Bool discard_element)
{
	LO_FormElementStruct *form_element;

	if ((element == NULL)||(element->type != LO_FORM_ELE))
	{
		return;
	}

	form_element = (LO_FormElementStruct *)element;
	if (form_element->element_data == NULL)
	{
		return;
	}

	if ((form_element->element_data->type != FORM_TYPE_HIDDEN)&&
	    (form_element->element_data->type != FORM_TYPE_KEYGEN))
	{
		FE_GetFormElementValue(context, form_element, discard_element);
	}
}

void
lo_SaveFormElementStateInFormList(MWContext *context, lo_FormData *form_list,
								  Bool discard_element)
{
	intn i;
	LO_Element **ele_list;

	while (form_list != NULL)
	{
		PA_LOCK(ele_list, LO_Element **,
			form_list->form_elements);
		for (i=0; i<form_list->form_ele_cnt; i++)
		{
			lo_save_form_element_data(context, ele_list[i],
				discard_element);
		}
		PA_UNLOCK(form_list->form_elements);
		form_list = form_list->next;
	}
}


void
lo_SaveFormElementState(MWContext *context, lo_DocState *state,
	Bool discard_element)
{
    int i;
	lo_FormData *form_list;

    for (i = 0; i <= state->top_state->max_layer_num; i++) {
        lo_LayerDocState *layer_state = state->top_state->layers[i];
        if (!layer_state)
            continue;
        form_list = layer_state->doc_lists->form_list;
        lo_SaveFormElementStateInFormList(context, 
                                          form_list, 
                                          discard_element);
	}
	state->top_state->savedData.FormList->valid = TRUE;
}


#ifdef XP_MAC
PRIVATE
#endif
void
lo_RedoHistoryForms(MWContext *context)
{
	XP_List *list_ptr;
	History_entry *entry;

	list_ptr = SHIST_GetList(context);
	while ((entry = (History_entry *)XP_ListNextObject(list_ptr))!=0)
	{
		if (entry->savedData.FormList != NULL)
		{
			int32 i;
			lo_SavedFormListData *fptr;
			LO_FormElementData **data_list;

			fptr = (lo_SavedFormListData *) entry->savedData.FormList;
			PA_LOCK(data_list, LO_FormElementData **, fptr->data_list);
			if (data_list != NULL)
			{
			    for (i=0; i < (fptr->data_count); i++)
			    {
				if (data_list[i] != NULL)
				{
					FE_FreeFormElement(context,
						(LO_FormElementData *)data_list[i]);
				}
			    }
			}
			PA_UNLOCK(fptr->data_list);
		}
	}
}


#ifdef XP_MAC
PRIVATE
#endif
void
lo_redo_form_elements_in_form_list(MWContext *context, lo_FormData *form_list)
{
	intn i;
	LO_Element **ele_list;

	while (form_list != NULL)
	{
		LO_FormElementStruct *form_ele;

		PA_LOCK(ele_list, LO_Element **,
			form_list->form_elements);
		for (i=0; i<form_list->form_ele_cnt; i++)
		{
			form_ele = (LO_FormElementStruct *)(ele_list[i]);
			if (form_ele != NULL)
			{
				if (form_ele->element_data != NULL)
				{
#ifdef OLD_WAY
					FE_FreeFormElement(context,
						form_ele->element_data);
#else
					FE_GetFormElementValue(context,
						form_ele, TRUE);
#endif
				}
				FE_GetFormElementInfo(context, form_ele);
			}
		}
		PA_UNLOCK(form_list->form_elements);
		form_list = form_list->next;
	}
}

void
LO_RedoFormElements(MWContext *context)
{
	int32 doc_id, i;
	lo_TopState *top_state;
	lo_DocState *state;
	lo_FormData *form_list;

#ifdef OLD_WAY
	lo_RedoHistoryForms(context);
#endif

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
	state = top_state->doc_state;

    for (i = 0; i <= state->top_state->max_layer_num; i++) {
        lo_LayerDocState *layer_state = state->top_state->layers[i];
        if (!layer_state)
            continue;
		form_list = layer_state->doc_lists->form_list;
		lo_redo_form_elements_in_form_list(context, form_list);
	}
}

#ifdef XP_MAC
PRIVATE
#endif
void
form_reset_closure(MWContext * context, LO_Element * ele, int32 event, 
		   void *obj, ETEventStatus status)
{
	intn i;
	LO_Element **ele_list;
	lo_FormData *form_list = (lo_FormData *) obj;

        if (status != EVENT_OK)
	    return;

	PA_LOCK(ele_list, LO_Element **, form_list->form_elements);
	for (i=0; i<form_list->form_ele_cnt; i++)
	{
		LO_FormElementStruct *form_ele;

		form_ele = (LO_FormElementStruct *)ele_list[i];
		if (form_ele->element_data == NULL)
		{
			continue;
		}

		if ((form_ele->element_data->type != FORM_TYPE_HIDDEN)&&
		    (form_ele->element_data->type != FORM_TYPE_KEYGEN)&&
		    (form_ele->element_data->type != FORM_TYPE_SUBMIT)&&
		    (form_ele->element_data->type != FORM_TYPE_RESET)&&
		    (form_ele->element_data->type != FORM_TYPE_READONLY)&&
		    (form_ele->element_data->type != FORM_TYPE_BUTTON))
		{
			FE_ResetFormElement(context, form_ele);
		}
	}
	PA_UNLOCK(form_list->form_elements);
}

void
LO_ResetForm(MWContext *context, LO_FormElementStruct *form_element)
{
	intn form_id;
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	lo_FormData *form_list;
	JSEvent *event;
	lo_DocLists *doc_lists;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
	state = top_state->doc_state;

	form_id = form_element->form_id;
	
	doc_lists = lo_GetDocListsById(state, form_element->layer_id);
	form_list = doc_lists->form_list;
	while (form_list != NULL)
	{
		if (form_list->id == form_id)
		{
			break;
		}
		form_list = form_list->next;
	}
	if (form_list == NULL)
	{
		return;
	}

	if (form_list->form_ele_cnt <= 0)
	{
		return;
	}

	/* all further processing will be done by our closure */
	event = XP_NEW_ZAP(JSEvent);
	event->type = EVENT_RESET;
	event->layer_id = form_element->layer_id;

	ET_SendEvent(context, (LO_Element *) form_element, event, 
		     form_reset_closure, form_list);

}



LO_FormElementStruct *
LO_FormRadioSet(MWContext *context, LO_FormElementStruct *form_element)
{
	intn i, form_id;
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	lo_FormData *form_list;
	LO_Element **ele_list;
	lo_FormElementToggleData *form_data1;
	char *name1;
	char *name2;
	LO_FormElementStruct *prev_ele;
	lo_DocLists *doc_lists;

	/*
	 * Get the unique document ID, and retrieve this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return NULL;
	}
	state = top_state->doc_state;

	if (form_element->element_data == NULL)
	{
		return NULL;
	}

	if (form_element->element_data->type != FORM_TYPE_RADIO)
	{
		return NULL;
	}

	form_data1 = (lo_FormElementToggleData *)form_element->element_data;
	if (form_data1->name == NULL)
	{
		return NULL;
	}
	form_data1->toggled = TRUE;
	prev_ele = form_element;

	form_id = form_element->form_id;

	doc_lists = lo_GetDocListsById(state, form_element->layer_id);
	form_list = doc_lists->form_list;
	while (form_list != NULL)
	{
		if (form_list->id == form_id)
		{
			break;
		}
		form_list = form_list->next;
	}
	if (form_list == NULL)
	{
		return prev_ele;
	}

	PA_LOCK(name1, char *, form_data1->name);
	PA_LOCK(ele_list, LO_Element **, form_list->form_elements);
	for (i=0; i<form_list->form_ele_cnt; i++)
	{
		LO_FormElementStruct *form_ele;
		lo_FormElementToggleData *form_data2;

		form_ele = (LO_FormElementStruct *)ele_list[i];
		if (form_ele->element_data == NULL)
		{
			continue;
		}

		if (form_ele->element_data->type != FORM_TYPE_RADIO)
		{
			continue;
		}

		if (form_ele == form_element)
		{
			continue;
		}

		form_data2 = (lo_FormElementToggleData *)form_ele->element_data;

		if (form_data2->name == NULL)
		{
			continue;
		}

		PA_LOCK(name2, char *, form_data2->name);
		if (XP_STRCMP(name1, name2) != 0)
		{
			PA_UNLOCK(form_data2->name);
			continue;
		}
		PA_UNLOCK(form_data2->name);

		if (form_data2->toggled != FALSE)
		{
			prev_ele = form_ele;
		}
		form_data2->toggled = FALSE;
		FE_SetFormElementToggle(context, form_ele, FALSE);
	}
	PA_UNLOCK(form_list->form_elements);
	PA_UNLOCK(form_data1->name);

	return prev_ele;
}


void
lo_ProcessIsIndexTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	PA_Tag tmp_tag;
	PA_Block buff;
	char *str;

	lo_SetSoftLineBreakState(context, state, FALSE, 1);
	/* passing tag to form gets ACTION */
	lo_BeginForm(context, state, tag);
	/* passing NULL tag to hrule gives default */
	lo_HorizontalRule(context, state, NULL);
	lo_SoftLineBreak(context, state, FALSE);

	buff = lo_FetchParamValue(context, tag, PARAM_PROMPT);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
	}
	else
	{
		buff = PA_ALLOC(XP_STRLEN(DEF_ISINDEX_PROMPT) + 1);
		if (buff == NULL)
		{
			state->top_state->out_of_memory = TRUE;
			return;
		}
		PA_LOCK(str, char *, buff);
		XP_STRCPY(str, DEF_ISINDEX_PROMPT);
	}

	state->cur_ele_type = LO_NONE;
	lo_FreshText(state);
	state->cur_ele_type = LO_TEXT;
	if (state->preformatted != PRE_TEXT_NO)
	{
		lo_PreformatedText(context, state, str);
	}
	else
	{
		lo_FormatText(context, state, str);
	}
	PA_UNLOCK(buff);
	PA_FREE(buff);
	lo_FlushLineBuffer(context, state);

	buff = PA_ALLOC(XP_STRLEN(ISINDEX_TAG_TEXT) + 1);
	if (buff == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}
	PA_LOCK(str, char *, buff);
	XP_STRCPY(str, ISINDEX_TAG_TEXT);
	PA_UNLOCK(buff);

	tmp_tag.type = P_INPUT;
	tmp_tag.is_end = FALSE;
	tmp_tag.newline_count = tag->newline_count;
	tmp_tag.data = buff;
	tmp_tag.data_len = XP_STRLEN(ISINDEX_TAG_TEXT);
	tmp_tag.true_len = 0;
	tmp_tag.lo_data = NULL;
	tmp_tag.next = NULL;
	lo_ProcessInputTag(context, state, &tmp_tag);
	PA_FREE(buff);

	lo_SetSoftLineBreakState(context, state, FALSE, 1);
	/* passing NULL tag to hrule gives default */
	lo_HorizontalRule(context, state, NULL);
	lo_SoftLineBreak(context, state, FALSE);
	lo_EndForm(context, state);
}


void
lo_ProcessKeygenTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_FormElementStruct *fe;
	lo_FormElementKeygenData *keygen_data;
	lo_FormElementSelectData *select_data;
	PA_Tag ttag;
	PA_Block buff;
	char **keylist;
	Bool status;
	intn len;
	char *str;
	char *kstr;
	char *pstr;

	/*
	 * To produce what we want, we first create a SELECT element
	 * that allows the user to select the key size, then we
	 * add a dummy/hidden element which is a marker that the
	 * *previous* element is the key information.  At the time
	 * of the submit, this will cause the option data to get
	 * figured out first, and then the KEYGEN element is our
	 * cue to take that an pass it to the security library for
	 * processing.
	 */

	/*
	 * A fake/empty tag for use during the creation of the selection
	 * choices.
	 */
	ttag.is_end = FALSE;
	ttag.newline_count = tag->newline_count;
	ttag.data = NULL;
	ttag.data_len = 0;
	ttag.true_len = 0;
	ttag.lo_data = NULL;
	ttag.next = NULL;

	/*
	 * Create a SELECT element whose options are the available key sizes.
	 * This will get displayed so the user can choose the key size, but
	 * it will get turned into a key generation on submit.
	 */
	ttag.type = P_SELECT;
	lo_BeginSelectTag(context, state, &ttag);

	/*
	 * Hang onto the select element so we can copy the KEYGEN
	 * element name into it (see below).
	 */
	fe = (LO_FormElementStruct *)state->current_ele;
	select_data = (lo_FormElementSelectData *)fe->element_data;

	/*
	 * XXX Get any necessary attributes out of "tag" for limiting or
	 * advising the SEC function list of choices.  (Or just pass the
	 * "tag" to SECNAV_GetKeyChoiceList?)  We could also use the
	 * attributes to describe whether the choices should be listed
	 * as select/options or as radio buttons or if there is only one
	 * choice then we don't show anything at all, etc.
	 */
	kstr = (char *)lo_FetchParamValue(context, tag, PARAM_KEYTYPE);
	pstr = (char *)lo_FetchParamValue(context, tag, PARAM_PQG);
	keylist = SECNAV_GetKeyChoiceList(kstr, pstr);

	ttag.type = P_OPTION;
	while (*keylist != NULL)
	{
		str = *keylist;
		len = XP_STRLEN(str);
		buff = PA_ALLOC(len + 1);
		if (buff != NULL)
		{
			char *tptr;

			PA_LOCK(tptr, char *, buff);
			XP_BCOPY(str, tptr, len);
			tptr[len] = '\0';
			PA_UNLOCK(buff);
		}
		lo_BeginOptionTag(context, state, &ttag);
		lo_EndOptionTag(context, state, buff);

		keylist++;
	}

	lo_EndSelectTag(context, state);

	/*
	 * Now create and add an INPUT element of type KEYGEN.
	 * This is actually a hidden element which is a marker that the
	 * previous element is the key information selection field.
	 */
	fe = lo_form_keygen(context, state, tag);
	if (fe != NULL)
	{
		status = lo_add_element_to_form_list(context, state, fe);
		if (status == FALSE)
			return;
	}

	keygen_data = (lo_FormElementKeygenData *)fe->element_data;
	select_data->name = lo_dup_block(keygen_data->name);
}


void
LO_SaveFormData(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

	/*
	 * Get the unique document ID, and retrieve this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
	state = top_state->doc_state;

	lo_SaveFormElementState(context, state, FALSE);
}

#define PA_ALLOC_FAILED(copy,old) (((copy) == NULL)&&((old) != NULL))

/*
 * Form element select data options is an array of lo_FormElementOptionData
 * structures. Makes a deep copy of the array
 */
static PA_Block
lo_copy_select_options(int32 option_cnt, PA_Block options)
{
	int i;
	PA_Block new_options;
	lo_FormElementOptionData *option_data, *new_option_data;

	if ((option_cnt <= 0)||(options == NULL))
	{
		return(NULL);
	}

	/*
	 * Allocate an array of lo_FormElementOptionData
	 */
	new_options = PA_ALLOC(option_cnt * sizeof(lo_FormElementOptionData));

	if (new_options == NULL)
	{
		return(NULL);
	}

	PA_LOCK(option_data, lo_FormElementOptionData *, options);
	PA_LOCK(new_option_data, lo_FormElementOptionData *, new_options);

	XP_MEMCPY(new_option_data, option_data,
		option_cnt * sizeof(lo_FormElementOptionData));

	/*
	 * Some of the members also need to be copied
	 */
	for (i=0; i < option_cnt; i++)
	{
		if (option_data[i].text_value != NULL)
		{
			new_option_data[i].text_value = lo_dup_block(
				option_data[i].text_value);
			new_option_data[i].value = lo_dup_block(option_data[i].value);

			if ((PA_ALLOC_FAILED(new_option_data[i].text_value,
					option_data[i].text_value))||
				(PA_ALLOC_FAILED(new_option_data[i].value,
					option_data[i].value)))
			{
				/*
				 * Free any data we've already allocated
				 */
				while (i >= 0)
				{
					if (new_option_data[i].text_value != NULL)
					{
						PA_FREE(new_option_data[i].text_value);
					}

					if (new_option_data[i].value != NULL)
					{
						PA_FREE(new_option_data[i].value);
					}

					i--;
				}

				PA_UNLOCK(options);
				PA_UNLOCK(new_options);
				PA_FREE(new_options);
				return(NULL);
			}
		}
	}

	PA_UNLOCK(options);
	PA_UNLOCK(new_options);
	return(new_options);
}

/*
 * Clone a form element data structure
 */
static LO_FormElementData *
lo_clone_form_element_data(LO_FormElementData *element_data)
{
	LO_FormElementData *new_element_data = XP_NEW(LO_FormElementData);

	if (new_element_data != NULL)
	{
		XP_MEMCPY(new_element_data, element_data, sizeof(LO_FormElementData));

		/*
		 * The old FE data isn't meaningful
		 */
		new_element_data->ele_text.FE_Data = NULL;

		/*
		 * Some of the form element types have members that must be copied
		 */
		switch (new_element_data->type)
		{
			case FORM_TYPE_RADIO:
			case FORM_TYPE_CHECKBOX:
				{
					lo_FormElementToggleData *form_data, *new_form_data;

					form_data = (lo_FormElementToggleData *)element_data;
					new_form_data = (lo_FormElementToggleData *)new_element_data;

					new_form_data->name = lo_dup_block(form_data->name);
					new_form_data->value = lo_dup_block(form_data->value);

					if ((PA_ALLOC_FAILED(new_form_data->name,
							form_data->name))||
						(PA_ALLOC_FAILED(new_form_data->value,
							form_data->value)))
					{
						lo_FreeFormElementData(new_element_data);
						XP_DELETE(new_element_data);
						return(NULL);
					}
				}
				break;

			case FORM_TYPE_SUBMIT:
			case FORM_TYPE_RESET:
			case FORM_TYPE_BUTTON:
			case FORM_TYPE_HIDDEN:
			case FORM_TYPE_KEYGEN:
			case FORM_TYPE_READONLY:
				{
					lo_FormElementMinimalData *form_data, *new_form_data;

					form_data = (lo_FormElementMinimalData *)element_data;
					new_form_data = (lo_FormElementMinimalData *)new_element_data;

					new_form_data->name = lo_dup_block(form_data->name);
					new_form_data->value = lo_dup_block(form_data->value);

					if ((PA_ALLOC_FAILED(new_form_data->name,
							form_data->name))||
						(PA_ALLOC_FAILED(new_form_data->value,
							form_data->value)))
					{
						lo_FreeFormElementData(new_element_data);
						XP_DELETE(new_element_data);
						return(NULL);
					}
				}
				break;

			case FORM_TYPE_TEXT:
			case FORM_TYPE_PASSWORD:
			case FORM_TYPE_FILE:
				{
					lo_FormElementTextData *form_data, *new_form_data;

					form_data = (lo_FormElementTextData *)element_data;
					new_form_data = (lo_FormElementTextData *)new_element_data;

					new_form_data->name = lo_dup_block(form_data->name);
					new_form_data->default_text = lo_dup_block(form_data->default_text);
					new_form_data->current_text = lo_dup_block(form_data->current_text);
					new_form_data->encoding = form_data->encoding;

					if ((PA_ALLOC_FAILED(new_form_data->name,
							form_data->name))||
						(PA_ALLOC_FAILED(new_form_data->default_text,
							form_data->default_text))||
						(PA_ALLOC_FAILED(new_form_data->current_text,
							form_data->current_text)))
					{
						lo_FreeFormElementData(new_element_data);
						XP_DELETE(new_element_data);
						return(NULL);
					}
				}
				break;

			case FORM_TYPE_TEXTAREA:
				{
					lo_FormElementTextareaData *form_data, *new_form_data;

					form_data = (lo_FormElementTextareaData *)element_data;
					new_form_data = (lo_FormElementTextareaData *)new_element_data;

					new_form_data->name = lo_dup_block(form_data->name);
					new_form_data->default_text = lo_dup_block(form_data->default_text);
					new_form_data->current_text = lo_dup_block(form_data->current_text);

					if ((PA_ALLOC_FAILED(new_form_data->name,
							form_data->name))||
						(PA_ALLOC_FAILED(new_form_data->default_text,
							form_data->default_text))||
						(PA_ALLOC_FAILED(new_form_data->current_text,
							form_data->current_text)))
					{
						lo_FreeFormElementData(new_element_data);
						XP_DELETE(new_element_data);
						return(NULL);
					}
				}
				break;

			case FORM_TYPE_SELECT_ONE:
			case FORM_TYPE_SELECT_MULT:
				{
					lo_FormElementSelectData *form_data, *new_form_data;

					form_data = (lo_FormElementSelectData *)element_data;
					new_form_data = (lo_FormElementSelectData *)new_element_data;

					new_form_data->name = lo_dup_block(form_data->name);
					new_form_data->options = lo_copy_select_options(
						form_data->option_cnt, form_data->options);

					if ((PA_ALLOC_FAILED(new_form_data->name,
							form_data->name))||
						(PA_ALLOC_FAILED(new_form_data->options,
							form_data->options)))
					{
						lo_FreeFormElementData(new_element_data);
						XP_DELETE(new_element_data);
						return(NULL);
					}
				}
				break;

			case FORM_TYPE_OBJECT:
				{
					lo_FormElementObjectData *form_data, *new_form_data;

					form_data = (lo_FormElementObjectData *)element_data;
					new_form_data = (lo_FormElementObjectData *)new_element_data;

					new_form_data->name = lo_dup_block(form_data->name);
					new_form_data->object = form_data->object;

					if (PA_ALLOC_FAILED(new_form_data->name, form_data->name))
					{
						lo_FreeFormElementData(new_element_data);
						XP_DELETE(new_element_data);
						return(NULL);
					}
				}
				break;
		}
	}

	return(new_element_data);
}

/*
 * Clone the form list data and hang it off the specified URL_Struct
 */
void
LO_CloneFormData(SHIST_SavedData* savedData, MWContext *context,
			URL_Struct *url_struct)
{
	int i;
	LO_FormElementData **src_data_list, **dst_data_list;
	lo_SavedFormListData *fptr, *copyptr;

	if (savedData == NULL || context == NULL || url_struct == NULL)
	{
		return;
	}

	fptr = savedData->FormList;

	if (fptr == NULL)
	{
		return;
	}

	if (fptr->data_count <= 0)
	{
		return;
	}

	/*
	 * The destination URL_Struct had better not already have form data
	 */
	if (url_struct->savedData.FormList != NULL)
	{
		return;
	}

	/*
	 * Allocate a form list data structure
	 */
	url_struct->savedData.FormList = lo_NewDocumentFormListData();

	if (url_struct->savedData.FormList == NULL)
	{
		return;
	}

	copyptr = (lo_SavedFormListData *)url_struct->savedData.FormList;

	/*
	 * Allocate an array to copy the form element data into
	 */
	copyptr->data_list = PA_ALLOC(fptr->data_count
		* sizeof(LO_FormElementData *));

	if (copyptr->data_list == NULL)
	{
		XP_DELETE(url_struct->savedData.FormList);
		url_struct->savedData.FormList = NULL;
		return;
	}

	/*
	 * Copy the form element data
	 */
	copyptr->valid = TRUE;
	PA_LOCK(src_data_list, LO_FormElementData **, fptr->data_list);
	PA_LOCK(dst_data_list, LO_FormElementData **, copyptr->data_list);
	if ((src_data_list != NULL)&&(dst_data_list != NULL))
	{
	    for (i=0; i < (fptr->data_count); i++)
	    {
		if (src_data_list[i] != NULL)
		{
			dst_data_list[i] = lo_clone_form_element_data(src_data_list[i]);

			if (dst_data_list[i] == NULL)
			{
				PA_UNLOCK(fptr->data_list);
				PA_UNLOCK(copyptr->data_list);
				lo_FreeDocumentFormListData(context, copyptr);
				XP_DELETE(copyptr);
				url_struct->savedData.FormList = NULL;
				return;
			}

			copyptr->data_count++;
		}
	    }
	}
	PA_UNLOCK(fptr->data_list);
	PA_UNLOCK(copyptr->data_list);
}

#ifdef TEST_16BIT
#undef XP_WIN16
#endif /* TEST_16BIT */

#ifdef PROFILE
#pragma profile off
#endif

