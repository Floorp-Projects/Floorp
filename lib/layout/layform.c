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
#ifdef JAVA
#include "java.h"
#endif
#include "laylayer.h"
#include "libevent.h"
#include "libimg.h"             /* Image Library public API. */
#include "libi18n.h"		/* For the character code set conversions */
#include "intl_csi.h"

#include "secnav.h"		/* For SECNAV_GenKeyFromChoice and SECNAV_GetKeyChoiceList protos */

#ifdef XP_MAC
#include "prpriv.h"             /* for NewNamedMonitor */
#include "prinrval.h"			/* for PR_IntervalNow */
#endif

#ifdef PROFILE
#pragma profile on
#endif

#if defined(SingleSignon)
#include "prefapi.h"
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

#if defined(SingleSignon)
#include "xpgetstr.h"
extern int MK_SIGNON_PASSWORDS_GENERATE;
extern int MK_SIGNON_PASSWORDS_REMEMBER;
extern int MK_SIGNON_PASSWORDS_FETCH;
extern int MK_SIGNON_YOUR_SIGNONS;
#endif

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
		if (type == FORM_TYPE_HIDDEN)
	        form_data->value = lo_FetchParamValue(context, tag, PARAM_VALUE);
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

	if (form_data->type != FORM_TYPE_HIDDEN)
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

#if defined(SingleSignon)
				/* check for data from previous signon form */
				if (state->top_state) { /* null-check probably not needed */
				    SI_RestoreOldSignonData
					(context, form_element,
					state->top_state->base_url);
				}
#endif
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

				object_value = LJ_Applet_GetText(form_data->object->objTag.session_data);
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


#if defined(SingleSignon)

/* locks for signon cache */

static PRMonitor * signon_lock_monitor = NULL;
static PRThread  * signon_lock_owner = NULL;
static int signon_lock_count = 0;

PRIVATE void
si_lock_signon_list(void)
{
    if(!signon_lock_monitor)
	signon_lock_monitor =
            PR_NewNamedMonitor("signon-lock");

    PR_EnterMonitor(signon_lock_monitor);

    while(TRUE) {

	/* no current owner or owned by this thread */
	PRThread * t = PR_CurrentThread();
	if(signon_lock_owner == NULL || signon_lock_owner == t) {
	    signon_lock_owner = t;
	    signon_lock_count++;

	    PR_ExitMonitor(signon_lock_monitor);
	    return;
	}

	/* owned by someone else -- wait till we can get it */
	PR_Wait(signon_lock_monitor, PR_INTERVAL_NO_TIMEOUT);

    }
}

PRIVATE void
si_unlock_signon_list(void)
{
   PR_EnterMonitor(signon_lock_monitor);

#ifdef DEBUG
    /* make sure someone doesn't try to free a lock they don't own */
    PR_ASSERT(signon_lock_owner == PR_CurrentThread());
#endif

    signon_lock_count--;

    if(signon_lock_count == 0) {
	signon_lock_owner = NULL;
	PR_Notify(signon_lock_monitor);
    }
    PR_ExitMonitor(signon_lock_monitor);

}


/*
 * Data and procedures for rememberSignons preference
 */

static const char *pref_rememberSignons =
    "network.signon.rememberSignons";
PRIVATE Bool si_RememberSignons = FALSE;

PRIVATE int
si_SaveSignonDataLocked(char * filename);

PRIVATE void
si_SetSignonRememberingPref(Bool x)
{
	/* do nothing if new value of pref is same as current value */
	if (x == si_RememberSignons) {
	    return;
	}

	/* if pref is being turned off, save the current signons to a file */
	if (x == 0) {
	    si_lock_signon_list();
	    si_SaveSignonDataLocked(NULL);
	    si_unlock_signon_list();
	}

	/* change the pref */
	si_RememberSignons = x;

	/* if pref is being turned on, load the signon file into memory */
	if (x == 1) {
	    SI_RemoveAllSignonData();
	    SI_LoadSignonData(NULL);
	}
}

MODULE_PRIVATE int PR_CALLBACK
si_SignonRememberingPrefChanged(const char * newpref, void * data)
{
	Bool x;
	PREF_GetBoolPref(pref_rememberSignons, &x);
	si_SetSignonRememberingPref(x);
	return PREF_NOERROR;
}

void
si_RegisterSignonPrefCallbacks(void)
{
    Bool x;
    static Bool first_time = TRUE;

    if(first_time)
    {
	first_time = FALSE;
	PREF_GetBoolPref(pref_rememberSignons, &x);
	si_SetSignonRememberingPref(x);
	PREF_RegisterCallback(pref_rememberSignons, si_SignonRememberingPrefChanged, NULL);
    }
}

PRIVATE Bool
si_GetSignonRememberingPref(void)
{
	si_RegisterSignonPrefCallbacks();
	return si_RememberSignons;
}

/*
 * Data and procedures for signon cache
 */

typedef struct _SignonDataStruct {
    char * name;
    char * value;
    Bool isPassword;
} si_SignonDataStruct;

typedef struct _SignonUserStruct {
    XP_List * signonData_list;
} si_SignonUserStruct;

typedef struct _SignonURLStruct {
    char * URLName;
    si_SignonUserStruct* chosen_user; /* this is a state variable */
    Bool firsttime_chosing_user; /* this too is a state variable */
    XP_List * signonUser_list;
} si_SignonURLStruct;

PRIVATE XP_List * si_signon_list=0;
PRIVATE Bool si_signon_list_changed = FALSE;
PRIVATE int si_TagCount = 0;
PRIVATE si_SignonURLStruct * lastURL = NULL;


/* Remove misleading portions from URL name */
PRIVATE char*
si_StrippedURL (char* URLName) {
    char *result = 0;
    char *s, *t;

    /* check for null URLName */
    if (URLName == NULL || XP_STRLEN(URLName) == 0) {
	return NULL;
    }

    /* check for protocol at head of URL */
    s = URLName;
    s = (char*) XP_STRCHR(s+1, '/');
    if (s) {
        if (*(s+1) == '/') {
            /* looks good, let's parse it */
	    return NET_ParseURL(URLName, GET_HOST_PART);
	} else {
	    /* not a valid URL, leave it as is */
	    return URLName;
	}
    }

    /* initialize result */
    StrAllocCopy(result, URLName);

    /* remove everything in result after and including first question mark */
    s = result;
    s = (char*) XP_STRCHR(s, '?');
    if (s) {
        *s = '\0';
    }

    /* remove everything in result after last slash (e.g., index.html) */
    s = result;
    t = 0;
    while (s = (char*) XP_STRCHR(s+1, '/')) {
	t = s;
    }
    if (t) {
        *(t+1) = '\0';
    }

    /* remove socket number from result */
    s = result;
    while (s = (char*) XP_STRCHR(s+1, ':')) {
	/* s is at next colon */
        if (*(s+1) != '/') {
            /* and it's not :// so it must be start of socket number */
            if (t = (char*) XP_STRCHR(s+1, '/')) {
		/* t is at slash terminating the socket number */
		do {
		    /* copy remainder of t to s */
		    *s++ = *t;
		} while (*(t++));
	    }
	    break;
	}
    }

    /* alll done */
    return result;
}

/* remove terminating CRs or LFs */
PRIVATE void
si_StripLF(char* buffer) {
    while ((buffer[XP_STRLEN(buffer)-1] == '\n') ||
            (buffer[XP_STRLEN(buffer)-1] == '\r')) {
        buffer[XP_STRLEN(buffer)-1] = '\0';
    }
}

/* If user-entered password is "generate", then generate a random password */
PRIVATE void
si_Randomize(char * password) {
    PRIntervalTime random;
    int i;
    const char * hexDigits = "0123456789AbCdEf";
    if (XP_STRCMP(password, XP_GetString(MK_SIGNON_PASSWORDS_GENERATE)) == 0) {
	random = PR_IntervalNow();
	for (i=0; i<8; i++) {
	    password[i] = hexDigits[random%16];
	    random = random/16;
	}
    }
}

/*
 * Get the URL node for a given URL name
 *
 * This routine is called only when holding the signon lock!!!
 */
PRIVATE si_SignonURLStruct *
si_GetURL(char * URLName) {
    XP_List * list_ptr = si_signon_list;
    si_SignonURLStruct * URL;
    char *strippedURLName = 0;

    if (!URLName) {
	/* no URLName specified, return first URL (returns NULL if not URLs) */
	return (si_SignonURLStruct *) XP_ListNextObject(list_ptr);
    }

    strippedURLName = si_StrippedURL(URLName);
    while((URL = (si_SignonURLStruct *) XP_ListNextObject(list_ptr))!=0) {
	if(URL->URLName && !XP_STRCMP(strippedURLName, URL->URLName)) {
	    XP_FREE(strippedURLName);
	    return URL;
	}
    }
    XP_FREE(strippedURLName);
    return (NULL);
}

/* Remove a user node from a given URL node */
PUBLIC Bool
SI_RemoveUser(char *URLName, char *userName, Bool save) {
    si_SignonURLStruct * URL;
    si_SignonUserStruct * user;
    si_SignonDataStruct * data;
    XP_List * user_ptr;
    XP_List * data_ptr;

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()) {
	return FALSE;
    }

    si_lock_signon_list();

    /* get URL corresponding to URLName (or first URL if URLName is NULL) */
    URL = si_GetURL(URLName);
    if (!URL) {
	/* URL not found */
	si_unlock_signon_list();
	return FALSE;
    }

    /* free the data in each node of the specified user node for this URL */
    user_ptr = URL->signonUser_list;

    if (userName == NULL) {

	/* no user specified so remove the first one */
	user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr);

    } else {

	/* find the specified user */
	while((user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr))!=0) {
	    data_ptr = user->signonData_list;
	    while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
		if (XP_STRCMP(data->value, userName)==0) {
		    goto foundUser;
		}
	    }
	}
	si_unlock_signon_list();
	return FALSE; /* user not found so nothing to remove */
	foundUser: ;
    }

    /* free the items in the data list */
    data_ptr = user->signonData_list;
    while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
	XP_FREE(data->name);
	XP_FREE(data->value);
    }

    /* free the data list */
    XP_ListDestroy(user->signonData_list);

    /* free the user node */
    XP_ListRemoveObject(URL->signonUser_list, user);

    /* remove this URL if it contains no more users */
    if (XP_ListCount(URL->signonUser_list) == 0) {
	XP_FREE(URL->URLName);
	XP_ListRemoveObject(si_signon_list, URL);
    }

    /* write out the change to disk */
    if (save) {
	si_signon_list_changed = TRUE;
	si_SaveSignonDataLocked(NULL);
    }

    si_unlock_signon_list();
    return TRUE;
}

/*
 * Get the user node for a given URL
 *
 * This routine is called only when holding the signon lock!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE si_SignonUserStruct*
si_GetUser(MWContext *context, char* URLName, Bool pickFirstUser) {
    si_SignonURLStruct* URL;
    si_SignonUserStruct* user;
    si_SignonDataStruct* data;
    XP_List * data_ptr=0;
    XP_List * user_ptr=0;
    XP_List * old_user_ptr=0;

    /* get to node for this URL */
    URL = si_GetURL(URLName);
    if (URL != NULL) {

	/* node for this URL was found */
	user_ptr = URL->signonUser_list;
	if (XP_ListCount(user_ptr) == 1) {

	    /* only one set of data exists for this URL so select it */
	    user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr);
	    URL->firsttime_chosing_user = FALSE;
	    URL->chosen_user = user;

	} else if (pickFirstUser) {
	    user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr);

	} else if (URL->firsttime_chosing_user) {
/*
 * The following UI is temporary.  The real version will display a list of users
 * from which the desired user will be selected.  The list is ordered by
 * most-recently-used so the first user is the most likely one and will be
 * preselected when the list first appears.
 */
	    /* step through set of user nodes for this URL and display the
	     * first data node of each (presumably that is the user name).
	     * Note that the user nodes are already ordered by
	     * most-recently-used so the first one displayed is the most
	     * likely one to be chosen
	     */
	    URL->firsttime_chosing_user = FALSE;
	    old_user_ptr = user_ptr;
	    while((user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr))!=0) {
		char *message = 0;
		data_ptr = user->signonData_list;

		/* consider first item in data list to be the identifying item */
		data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr);

		/* display identifying item and see if user will select it */
		StrAllocCopy(message, data->name);
                StrAllocCat(message, " = ");
		StrAllocCat(message, data->value);
		if (FE_Confirm(context, message)) {

		    /* user selected this one */
		    URL->chosen_user = user;
		    XP_FREE(message);

		    /* user is most-recently used, put at head of list */
		    XP_ListRemoveObject(URL->signonUser_list, user);
		    XP_ListAddObject(URL->signonUser_list, user);

		    break;
		}
		XP_FREE(message);
	    }
/* End of temporary UI */
	} else {
	    user = URL->chosen_user;
	}
    } else {
	user = NULL;
    }
    return user;
}

/*
 * Get the user for which a change-of-password is to be applied
 *
 * This routine is called only when holding the signon lock!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE si_SignonUserStruct*
si_GetUserForChangeForm(MWContext *context, char* URLName, int messageNumber)
{
    si_SignonURLStruct* URL;
    si_SignonUserStruct* user;
    si_SignonDataStruct * data;
    XP_List * user_ptr = 0;
    XP_List * data_ptr = 0;
    char *message = 0;

    /* get to node for this URL */
    URL = si_GetURL(URLName);
    if (URL != NULL) {

	/* node for this URL was found */
	user_ptr = URL->signonUser_list;
	while((user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr))!=0) {
	    char *strippedURLName = si_StrippedURL(URLName);
	    data_ptr = user->signonData_list;
	    data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr);
	    message = PR_smprintf(XP_GetString(messageNumber),
				  data->value,
				  strippedURLName);
	    XP_FREE(strippedURLName);
	    if (FE_Confirm(context, message)) {
		/*
		 * since this user node is now the most-recently-used one, move it
		 * to the head of the user list so that it can be favored for
		 * re-use the next time this form is encountered
		 */
		XP_ListRemoveObject(URL->signonUser_list, user);
		XP_ListAddObject(URL->signonUser_list, user);
		si_signon_list_changed = TRUE;
		si_SaveSignonDataLocked(NULL);
		XP_FREE(message);
		return user;
	    }
	}
    }
    XP_FREE(message);
    return NULL;
}

/*
 * Put data obtained from a submit form into the data structure for
 * the specified URL
 *
 * See comments below about state of signon lock when routine is called!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE void
si_PutData(char * URLName, LO_FormSubmitData * submit, Bool save) {
    char * name;
    char * value;
    Bool added_to_list = FALSE;
    si_SignonURLStruct * URL;
    si_SignonUserStruct * user;
    si_SignonDataStruct * data;
    int j;
    XP_List * list_ptr;
    XP_List * user_ptr;
    XP_List * data_ptr;
    si_SignonURLStruct * tmp_URL_ptr;
    size_t new_len;
    Bool mismatch;
    int i;

    /* discard this if the password is empty */
    for (i=0; i<submit->value_cnt; i++) {
	if ((((uint8*)submit->type_array)[i] == FORM_TYPE_PASSWORD) &&
		(XP_STRLEN( ((char **)(submit->value_array)) [i])) == 0) {
	    return;
	}
    }

    /*
     * lock the signon list
     *	 Note that, for efficiency, SI_LoadSignonData already sets the lock
     *	 before calling this routine whereas none of the other callers do.
     *	 So we need to determine whether or not we were called from
     *	 SI_LoadSignonData before setting or clearing the lock.  We can
     *   determine this by testing "save" since only SI_LoadSignonData passes
     *   in a value of FALSE for "save".
     */
    if (save) {
	si_lock_signon_list();
    }

    /* make sure the signon list exists */
    if(!si_signon_list) {
	si_signon_list = XP_ListNew();
	if(!si_signon_list) {
	    if (save) {
		si_unlock_signon_list();
	    }
	    return;
	}
    }

    /* find node in signon list having the same URL */
    if ((URL = si_GetURL(URLName)) == NULL) {

        /* doesn't exist so allocate new node to be put into signon list */
	URL = XP_NEW(si_SignonURLStruct);
	if (!URL) {
	    if (save) {
		si_unlock_signon_list();
	    }
	    return;
	}

	/* fill in fields of new node */
	URL->URLName = si_StrippedURL(URLName);
	if (!URL->URLName) {
	    XP_FREE(URL);
	    if (save) {
		si_unlock_signon_list();
	    }
	    return;
	}

	URL->signonUser_list = XP_ListNew();
	if(!URL->signonUser_list) {
	    XP_FREE(URL->URLName);
	    XP_FREE(URL);
	}

	/* put new node into signon list so that it is before any
	 * strings of smaller length
	 */
	new_len = XP_STRLEN(URL->URLName);
	list_ptr = si_signon_list;
	while((tmp_URL_ptr =
		(si_SignonURLStruct *) XP_ListNextObject(list_ptr))!=0) {
	    if(new_len > XP_STRLEN (tmp_URL_ptr->URLName)) {
		XP_ListInsertObject
		    (si_signon_list, tmp_URL_ptr, URL);
		added_to_list = TRUE;
		break;
	    }
	}
	/* no shorter strings found in list */
	if (!added_to_list) {
	    XP_ListAddObjectToEnd (si_signon_list, URL);
	}
    }

    /* initialize state variables in URL node */
    URL->chosen_user = NULL;
    URL->firsttime_chosing_user = TRUE;

    /*
     * see if a user node with data list matching new data already exists
     * (password fields will not be checked for in this matching)
     */
    user_ptr = URL->signonUser_list;
    while((user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr))!=0) {
	data_ptr = user->signonData_list;
	j = 0;
	while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
	    mismatch = FALSE;

	    /* skip non text/password fields */
	    while ((j < submit->value_cnt) &&
		    (((uint8*)submit->type_array)[j]!=FORM_TYPE_TEXT) &&
		    (((uint8*)submit->type_array)[j]!=FORM_TYPE_PASSWORD)) {
		j++;
	    }

	    /* check for match on name field and type field */
	    if ((j < submit->value_cnt) &&
		    (data->isPassword ==
			(((uint8*)submit->type_array)[j]==FORM_TYPE_PASSWORD)) &&
		    (!XP_STRCMP(data->name, ((char **)submit->name_array)[j]))) {

		/* success, now check for match on value field if not password */
		if (!submit->value_array[j]) {
		    /* special case a null value */
                    if (!XP_STRCMP(data->value, "")) {
			j++; /* success */
		    } else {
			mismatch = TRUE;
			break; /* value mismatch, try next user */
		    }
		} else if (!XP_STRCMP(data->value, ((char **)submit->value_array)[j])
			|| data->isPassword) {
		    j++; /* success */
		} else {
		    mismatch = TRUE;
		    break; /* value mismatch, try next user */
		}
	    } else {
		mismatch = TRUE;
		break; /* name or type mismatch, try next user */
	    }

	}
	if (!mismatch) {

	    /* all names and types matched and all non-password values matched */

	    /*
	     * note: it is ok for password values not to match; it means
	     * that the user has either changed his password behind our
	     * back or that he previously mis-entered the password
	     */

	    /* update the saved password values */
	    data_ptr = user->signonData_list;
	    j = 0;
	    while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {

		/* skip non text/password fields */
		while ((j < submit->value_cnt) &&
			(((uint8*)submit->type_array)[j]!=FORM_TYPE_TEXT) &&
			(((uint8*)submit->type_array)[j]!=FORM_TYPE_PASSWORD)) {
		    j++;
		}

		/* update saved password */
		if ((j < submit->value_cnt) && data->isPassword) {
		    if (XP_STRCMP(data->value, ((char **)submit->value_array)[j])) {
			si_signon_list_changed = TRUE;
			StrAllocCopy(data->value, ((char **)submit->value_array)[j]);
			si_Randomize(data->value);
		    }
		}
		j++;
	    }

	    /*
	     * since this user node is now the most-recently-used one, move it
	     * to the head of the user list so that it can be favored for
	     * re-use the next time this form is encountered
	     */
	    XP_ListRemoveObject(URL->signonUser_list, user);
	    XP_ListAddObject(URL->signonUser_list, user);

	    /* return */
	    if (save) {
		si_signon_list_changed = TRUE;
		si_SaveSignonDataLocked(NULL);
		si_unlock_signon_list();
	    }
	    return; /* nothing more to do since data already exists */
	}
    }

    /* user node with current data not found so create one */
    user = XP_NEW(si_SignonUserStruct);
    if (!user) {
	if (save) {
	    si_unlock_signon_list();
	}
	return;
    }
    user->signonData_list = XP_ListNew();
    if(!user->signonData_list) {
	XP_FREE(user);
	if (save) {
	    si_unlock_signon_list();
	}
	return;
    }


    /* create and fill in data nodes for new user node */
    for (j=0; j<submit->value_cnt; j++) {

	/* skip non text/password fields */
	if((((uint8*)submit->type_array)[j]!=FORM_TYPE_TEXT) &&
		(((uint8*)submit->type_array)[j]!=FORM_TYPE_PASSWORD)) {
	    continue;
	}

	/* create signon data node */
	data = XP_NEW(si_SignonDataStruct);
	if (!data) {
	    XP_ListDestroy(user->signonData_list);
	    XP_FREE(user);
	}
	data->isPassword =
	    (((uint8 *)submit->type_array)[j] == FORM_TYPE_PASSWORD);
        name = 0; /* so that StrAllocCopy doesn't free previous name */
	StrAllocCopy(name, ((char **)submit->name_array)[j]);
	data->name = name;
        value = 0; /* so that StrAllocCopy doesn't free previous name */
	if (submit->value_array[j]) {
	    StrAllocCopy(value, ((char **)submit->value_array)[j]);
	} else {
            StrAllocCopy(value, ""); /* insures that value is not null */
	}
	data->value = value;
	if (data->isPassword) {
	    si_Randomize(data->value);
	}

	/* append new data node to end of data list */
	XP_ListAddObjectToEnd (user->signonData_list, data);
    }

    /* append new user node to front of user list for matching URL */
	/*
	 * Note that by appending to the front, we assure that if there are
	 * several users, the most recently used one will be favored for
         * reuse the next time this form is encountered.  But don't do this
	 * when reading in signons.txt (i.e., when save is false), otherwise
	 * we will be reversing the order when reading in.
	 */
    if (save) {
	XP_ListAddObject(URL->signonUser_list, user);
	si_signon_list_changed = TRUE;
	si_SaveSignonDataLocked(NULL);
	si_unlock_signon_list();
    } else {
	XP_ListAddObjectToEnd(URL->signonUser_list, user);
    }
}

/*
 * When a new form is started, set the tagCount to 0
 */
PUBLIC void
SI_StartOfForm() {
    si_TagCount = 0;
}

/*
 * Load signon data from disk file
 * The parameter passed in on entry is ignored
 */

#define BUFFER_SIZE 4096
#define MAX_ARRAY_SIZE 50
PUBLIC int
SI_LoadSignonData(char * filename) {
    char * URLName;
    LO_FormSubmitData submit;
    char* name_array[MAX_ARRAY_SIZE];
    char* value_array[MAX_ARRAY_SIZE];
    uint8 type_array[MAX_ARRAY_SIZE];
    char buffer[BUFFER_SIZE];
    XP_File fp;
    Bool badInput;
    int i;
    char* unmungedValue;

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()) {
	return 0;
    }

    /* open the signon file */
    if(!(fp = XP_FileOpen(filename, xpHTTPSingleSignon, XP_FILE_READ)))
	return(-1);

    /* initialize the submit structure */
    submit.name_array = (PA_Block)name_array;
    submit.value_array = (PA_Block)value_array;
    submit.type_array = (PA_Block)type_array;

    /* read the URL line */
    si_lock_signon_list();
    while(XP_FileReadLine(buffer, BUFFER_SIZE, fp)) {

	/* ignore comment lines at head of file */
        if (buffer[0] == '#') {
	    continue;
	}
	si_StripLF(buffer);
	URLName = NULL;
	StrAllocCopy(URLName, buffer);

	/* preparre to read the name/value pairs */
	submit.value_cnt = 0;
	badInput = FALSE;

	while(XP_FileReadLine(buffer, BUFFER_SIZE, fp)) {

	    /* line starting with . terminates the pairs for this URL entry */
            if (buffer[0] == '.') {
		break; /* end of URL entry */
	    }

	    /* line just read is the name part */

	    /* save the name part and determine if it is a password */
	    si_StripLF(buffer);
	    name_array[submit.value_cnt] = NULL;
            if (buffer[0] == '*') {
		type_array[submit.value_cnt] = FORM_TYPE_PASSWORD;
		StrAllocCopy(name_array[submit.value_cnt], buffer+1);
	    } else {
		type_array[submit.value_cnt] = FORM_TYPE_TEXT;
		StrAllocCopy(name_array[submit.value_cnt], buffer);
	    }

	    /* read in and save the value part */
	    if(!XP_FileReadLine(buffer, BUFFER_SIZE, fp)) {
		/* error in input file so give up */
		badInput = TRUE;
		break;
	    }
	    si_StripLF(buffer);
	    value_array[submit.value_cnt] = NULL;
            /* note that we need to skip over leading '=' of value */
	    if (type_array[submit.value_cnt] == FORM_TYPE_PASSWORD) {
	    	/* UnMungeString will return NULL in the free source because there is
	    	 * no security. When saving out the string, we wrote 8 stars. If that is
	    	 * what we get, create an empty password to be copied into the value array
	    	 */
	    	if ( strcmp(buffer+1, "********") == 0 ) {
	    		/* don't use an empty string because si_PutData doesn't like it */
	    		unmungedValue = XP_ALLOC(2);
	    		unmungedValue[0] = '?'; unmungedValue[1] = '\0';
	    	}
	    	else {
				unmungedValue = SECNAV_UnMungeString(buffer+1);
			}
			StrAllocCopy(value_array[submit.value_cnt++], unmungedValue);
			XP_FREE(unmungedValue);
	    } else {
		StrAllocCopy(value_array[submit.value_cnt++], buffer+1);
	    }

	    /* check for overruning of the arrays */
	    if (submit.value_cnt >= MAX_ARRAY_SIZE) {
		break;
	    }
	}

	/* store the info for this URL into memory-resident data structure */
	if (!URLName || XP_STRLEN(URLName) == 0) {
	    badInput = TRUE;
	}
	if (!badInput) {
	    si_PutData(URLName, &submit, FALSE);
	}

	/* free up all the allocations done for processing this URL */
	XP_FREE(URLName);
	for (i=0; i<submit.value_cnt; i++) {
	    XP_FREE(name_array[i]);
	    XP_FREE(value_array[i]);
	}
	if (badInput) {
	    return (1);
	}
    }
    si_unlock_signon_list();
    return(0);
}


/*
 * Save signon data to disk file
 * The parameter passed in on entry is ignored
 *
 * This routine is called only when holding the signon lock!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */

PRIVATE int
si_SaveSignonDataLocked(char * filename) {
    XP_List * list_ptr;
    XP_List * user_ptr;
    XP_List * data_ptr;
    si_SignonURLStruct * URL;
    si_SignonUserStruct * user;
    si_SignonDataStruct * data;
    XP_File fp;

    /* do nothing if signon list has not changed */
    list_ptr = si_signon_list;
    if(!si_signon_list_changed) {
	return(-1);
    }

    /* do nothing is signon list is empty */
    if(XP_ListIsEmpty(si_signon_list)) {
	return(-1);
    }

    /* do nothing if we are unable to open file that contains signon list */
    if(!(fp = XP_FileOpen(filename, xpHTTPSingleSignon, XP_FILE_WRITE))) {
	return(-1);
    }

    /* write out leading comments */
    XP_FileWrite("# Netscape HTTP Signons File" LINEBREAK
                  "# This is a generated file!  Do not edit."
                  LINEBREAK "#" LINEBREAK, -1, fp);

    /* format shall be:
     * url LINEBREAK {name LINEBREAK value LINEBREAK}*	. LINEBREAK
     * if type is signon, name is preceded by an asterisk (*)
     */

    /* write out each URL node */
    while((URL = (si_SignonURLStruct *)
	    XP_ListNextObject(list_ptr)) != NULL) {

	user_ptr = URL->signonUser_list;

	/* write out each user node of the URL node */
	while((user = (si_SignonUserStruct *)
		XP_ListNextObject(user_ptr)) != NULL) {
	    XP_FileWrite(URL->URLName, -1, fp);
	    XP_FileWrite(LINEBREAK, -1, fp);

	    data_ptr = user->signonData_list;

	    /* write out each data node of the user node */
	    while((data = (si_SignonDataStruct *)
		    XP_ListNextObject(data_ptr)) != NULL) {
		char* mungedValue;
		if (data->isPassword) {
                    XP_FileWrite("*", -1, fp);
		}
		XP_FileWrite(data->name, -1, fp);
		XP_FileWrite(LINEBREAK, -1, fp);
                XP_FileWrite("=", -1, fp); /* precede values with '=' */
		if (data->isPassword) {
			/* in free source, MungeString will return NULL because there is
			 * no security available. Instead, we write out 8 stars so we know
			 * this isn't a valid password.
			 */
		    mungedValue = SECNAV_MungeString(data->value);
		    if ( mungedValue ) {
			    XP_FileWrite(mungedValue, -1, fp);
			    XP_FREE(mungedValue);
			}
			else
				XP_FileWrite("********", -1, fp);
		} else {
		    XP_FileWrite(data->value, -1, fp);
		}
		XP_FileWrite(LINEBREAK, -1, fp);
	    }
            XP_FileWrite(".", -1, fp);
	    XP_FileWrite(LINEBREAK, -1, fp);
	}
    }

    si_signon_list_changed = FALSE;
    XP_FileClose(fp);
    return(0);
}


/*
 * Save signon data to disk file
 * The parameter passed in on entry is ignored
 */

PUBLIC int
SI_SaveSignonData(char * filename) {
    int retval;

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()) {
	return FALSE;
    }

    /* lock and call common save routine */
    si_lock_signon_list();
    retval = si_SaveSignonDataLocked(filename);
    si_unlock_signon_list();
    return retval;
}

/*
 * Remove all the signons and free everything
 */

PUBLIC void
SI_RemoveAllSignonData() {

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()) {
	return;
    }

    /* repeatedly remove first user node of first URL node */
    while (SI_RemoveUser(NULL, NULL, FALSE)) {
    }
}

/*
 * Check for a signon submission and remember the data if so
 */
PUBLIC void
SI_RememberSignonData(MWContext *context, LO_FormSubmitData * submit)
{
    int i;
    int passwordCount = 0;
    int pswd[3];

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()){
	return;
    }

    /* determine how many passwords are in the form and where they are */
    for (i=0; i<submit->value_cnt; i++) {
	if (((uint8 *)submit->type_array)[i] == FORM_TYPE_PASSWORD) {
	    if (passwordCount < 3 ) {
		pswd[passwordCount] = i;
	    }
	    passwordCount++;
	}
    }

    /* process the form according to how many passwords it has */
    if (passwordCount == 1) {
	/* one-password form is a log-in so remember it */
	si_PutData(context->hist.cur_doc_ptr->address, submit, TRUE);

    } else if (passwordCount == 2) {
	/* two-password form is a registration */

    } else if (passwordCount == 3) {
	/* three-password form is a change-of-password request */

	XP_List * data_ptr;
	si_SignonDataStruct* data;
	si_SignonUserStruct* user;

	/* make sure all passwords are non-null and 2nd and 3rd are identical */
	if (!submit->value_array[pswd[0]] || ! submit->value_array[pswd[1]] ||
		!submit->value_array[pswd[2]] ||
		XP_STRCMP(((char **)submit->value_array)[pswd[1]],
		       ((char **)submit->value_array)[pswd[2]])) {
	    return;
	}

	/* ask user if this is a password change */
	si_lock_signon_list();
	user = si_GetUserForChangeForm(
	    context,
	    context->hist.cur_doc_ptr->address,
	    MK_SIGNON_PASSWORDS_REMEMBER);

	/* return if user said no */
	if (!user) {
	    si_unlock_signon_list();
	    return;
	}

	/* get to password being saved */
	data_ptr = user->signonData_list;
	while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
	    if (data->isPassword) {
		break;
	    }
	}

	/*
         * if second password is "generate" then generate a random
	 * password for it and use same random value for third password
	 * as well (Note: this all works because we already know that
	 * second and third passwords are identical so third password
         * must also be "generate".  Furthermore si_Randomize() will
	 * create a random password of exactly eight characters -- the
         * same length as "generate".)
	 */
	si_Randomize(((char **)submit->value_array)[pswd[1]]);
	XP_STRCPY(((char **)submit->value_array)[pswd[2]],
	       ((char **)submit->value_array)[pswd[1]]);
	StrAllocCopy(data->value, ((char **)submit->value_array)[pswd[1]]);
	si_signon_list_changed = TRUE;
	si_SaveSignonDataLocked(NULL);
	si_unlock_signon_list();
    }
}

/*
 * Check for remembered data from a previous signon submission and
 * restore it if so
 */
PUBLIC void
SI_RestoreOldSignonData
    (MWContext *context, LO_FormElementStruct *form_element, char* URLName)
{
    si_SignonURLStruct* URL;
    si_SignonUserStruct* user;
    si_SignonDataStruct* data;
    XP_List * data_ptr=0;

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()){
	return;
    }

    /* get URL */
    si_lock_signon_list();
    URL = si_GetURL(URLName);

    /*
     * if we leave a form without submitting it, the URL's state variable
     * will not get reset.  So we test for that case now and reset the
     * state variables if necessary
     */
    if (lastURL != URL) {
	if (lastURL) {
	    lastURL -> chosen_user = NULL;
	    lastURL -> firsttime_chosing_user = TRUE;
	}
    }
    lastURL = URL;

    /* see if this is first item in form and is a password */
    si_TagCount++;
    if ((si_TagCount == 1) &&
	    (form_element->element_data->type == FORM_TYPE_PASSWORD)) {
	/*
         * it is so there's a good change its a password change form,
         * let's ask user to confirm this
	 */
	user = si_GetUserForChangeForm(context, URLName, MK_SIGNON_PASSWORDS_FETCH);
	if (user) {
            /* user has confirmed it's a change-of-password form */
	    data_ptr = user->signonData_list;
	    data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr);
	    while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
		if (data->isPassword) {
		    StrAllocCopy(
			(char *)form_element->element_data->ele_text.default_text,
			data->value);
		    si_unlock_signon_list();
		    return;
		}
	    }
	}
    }

    /* get the data from previous time this URL was visited */

    if (si_TagCount == 1) {
	user = si_GetUser(context, URLName, FALSE);
    } else {
	if (URL) {
	    user = URL->chosen_user;
	} else {
	    user = NULL;
	}
    }
    if (user) {

	/* restore the data from previous time this URL was visited */
	data_ptr = user->signonData_list;
	while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
	    if(XP_STRCMP(data->name,
		    (char *)form_element->element_data->ele_text.name)==0) {
		StrAllocCopy(
		    (char *)form_element->element_data->ele_text.default_text,
		    data->value);
		si_unlock_signon_list();
		return;
	    }
	}
    }
    si_unlock_signon_list();
}

/*
 * Remember signon data from a browser-generated password dialog
 */
PRIVATE void
si_RememberSignonDataFromBrowser(char* URLName, char* username, char* password)
{
    LO_FormSubmitData submit;
    char* USERNAME = "username";
    char* PASSWORD = "password";
    char* name_array[2];
    char* value_array[2];
    uint8 type_array[2];

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()){
	return;
    }

    /* initialize a temporary submit structure */
    submit.name_array = (PA_Block)name_array;
    submit.value_array = (PA_Block)value_array;
    submit.type_array = (PA_Block)type_array;
    submit.value_cnt = 2;

    name_array[0] = USERNAME;
    value_array[0] = NULL;
    StrAllocCopy(value_array[0], username);
    type_array[0] = FORM_TYPE_TEXT;

    name_array[1] = PASSWORD;
    value_array[1] = NULL;
    StrAllocCopy(value_array[1], password);
    type_array[1] = FORM_TYPE_PASSWORD;

    /* Save the signon data */
    si_PutData(URLName, &submit, TRUE);

    /* Free up the data memory just allocated */
    XP_FREE(value_array[0]);
    XP_FREE(value_array[1]);
}

/*
 * Check for remembered data from a previous browser-generated password dialog
 * restore it if so
 */
PRIVATE void
si_RestoreOldSignonDataFromBrowser
    (MWContext *context, char* URLName, Bool pickFirstUser,
    char** username, char** password)
{
    si_SignonUserStruct* user;
    si_SignonDataStruct* data;
    XP_List * data_ptr=0;

    /* get the data from previous time this URL was visited */
    si_lock_signon_list();
    user = si_GetUser(context, URLName, pickFirstUser);
    if (!user) {
	*username = 0;
	*password = 0;
	si_unlock_signon_list();
	return;
    }

    /* restore the data from previous time this URL was visited */
    data_ptr = user->signonData_list;
    while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
        if(XP_STRCMP(data->name, "username")==0) {
	    StrAllocCopy(*username, data->value);
        } else if(XP_STRCMP(data->name, "password")==0) {
	    StrAllocCopy(*password, data->value);
	}
    }
    si_unlock_signon_list();
}

/* Browser-generated prompt for user-name and password */
PUBLIC int
SI_PromptUsernameAndPassword
    (MWContext *context, char *prompt,
    char **username, char **password, char *URLName)
{
    int status;
    char *copyOfPrompt=0;

    /* just for safety -- really is a problem in SI_Prompt */
    StrAllocCopy(copyOfPrompt, prompt);

    /* do the FE thing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()){
	status = FE_PromptUsernameAndPassword
		     (context, copyOfPrompt, username, password);
	XP_FREEIF(copyOfPrompt);
	return status;
    }

    /* prefill with previous username/password if any */
    si_RestoreOldSignonDataFromBrowser
	(context, URLName, FALSE, username, password);

    /* get new username/password from user */
    status = FE_PromptUsernameAndPassword
		 (context, copyOfPrompt, username, password);

    /* remember these values for next time */
    if (status) {
	si_RememberSignonDataFromBrowser (URLName, *username, *password);
    }

    /* cleanup and return */
    XP_FREEIF(copyOfPrompt);
    return status;
}

/* Browser-generated prompt for password */
PUBLIC char *
SI_PromptPassword
    (MWContext *context, char *prompt, char *URLName,
    Bool pickFirstUser, Bool useLastPassword)
{
    char *password=0, *username=0, *copyOfPrompt=0, *result;

    /* just for safety -- really is a problem in SI_Prompt */
    StrAllocCopy(copyOfPrompt, prompt);

    /* do the FE thing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()){
	result = FE_PromptPassword(context, copyOfPrompt);
	XP_FREEIF(copyOfPrompt);
	return result;
    }

    /* get previous password used  with this username */
    if (useLastPassword) {
	si_RestoreOldSignonDataFromBrowser
	    (context, URLName, pickFirstUser, &username, &password);
    }
    if (password && XP_STRLEN(password)) {
	XP_FREEIF(copyOfPrompt);
	return password;
    }

    /* if no password found, get new password from user */
    password = FE_PromptPassword(context, copyOfPrompt);

    /* if username wasn't even found, substitute the URLName */
    if (!username) {
	StrAllocCopy(username, URLName);
    }

    /* remember these values for next time */
    if (password && XP_STRLEN(password)) {
	si_RememberSignonDataFromBrowser (URLName, username, password);
    }

    /* cleanup and return */
    XP_FREEIF(username);
    XP_FREEIF(copyOfPrompt);
    return password;
}

/* Browser-generated prompt for username */
PUBLIC char *
SI_Prompt (MWContext *context, char *prompt,
    char* defaultUsername, char *URLName)
{
    char *password=0, *username=0, *copyOfPrompt=0, *result;

    /*
     * make copy of prompt because it is currently in a temporary buffer in
     * the caller (from GetString) which will be destroyed if GetString() is
     * called again before prompt is used
     */
    StrAllocCopy(copyOfPrompt, prompt);

    /* do the FE thing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()){
	result = FE_Prompt(context, copyOfPrompt, defaultUsername);
	XP_FREEIF(copyOfPrompt);
	return result;
    }

    /* get previous username used */
    if (!defaultUsername || !XP_STRLEN(defaultUsername)) {
	si_RestoreOldSignonDataFromBrowser
	    (context, URLName, FALSE, &username, &password);
    } else {
       StrAllocCopy(username, defaultUsername);
    }

    /* prompt for new username */
    result = FE_Prompt(context, copyOfPrompt, username);

    /* remember this username for next time */
    if (result && XP_STRLEN(result)) {
        si_RememberSignonDataFromBrowser (URLName, result, "");
    }

    /* cleanup and return */
    XP_FREEIF(username);
    XP_FREEIF(password);
    XP_FREEIF(copyOfPrompt);
    return result;
}

#include "mkgeturl.h"
#include "htmldlgs.h"
extern int XP_EMPTY_STRINGS;
extern int SA_VIEW_BUTTON_LABEL;
extern int SA_REMOVE_BUTTON_LABEL;

#define BUFLEN 5000

#define FLUSH_BUFFER			\
    if (buffer) {			\
	StrAllocCat(buffer2, buffer);	\
	g = 0;				\
    }

/* return TRUE if "number" is in sequence of comma-separated numbers */
Bool si_InSequence(char* sequence, int number) {
    char* ptr;
    char* endptr;
    char* undo = NULL;
    Bool retval = FALSE;
    int i;

    /* not necessary -- routine will work even with null sequence */
    if (!*sequence) {
	return FALSE;
    }

    for (ptr = sequence ; ptr ; ptr = endptr) {

	/* get to next comma */
        endptr = XP_STRCHR(ptr, ',');

	/* if comma found, set it to null */
	if (endptr) {

	    /* restore last comma-to-null back to comma */
	    if (undo) {
                *undo = ',';
	    }
	    undo = endptr;
            *endptr++ = '\0';
	}

        /* if there is a number before the comma, compare it with "number" */
	if (*ptr) {
	    i = atoi(ptr);
	    if (i == number) {

                /* "number" was in the sequence so return TRUE */
		retval = TRUE;
		break;
	    }
	}
    }

    if (undo) {
        *undo = ',';
    }
    return retval;
}

MODULE_PRIVATE void
si_DisplayUserInfoAsHTML
    (MWContext *context, si_SignonUserStruct* user, char* URLName)
{
    char *buffer = (char*)XP_ALLOC(BUFLEN);
    char *buffer2 = 0;
    int g = 0;
    XP_List *data_ptr = user->signonData_list;
    si_SignonDataStruct* data;
    int dataNum;

    static XPDialogInfo dialogInfo = {
	XP_DIALOG_OK_BUTTON,
	NULL,
	250,
	200
    };

    XPDialogStrings* strings;
    StrAllocCopy(buffer2, "");

    g += PR_snprintf(buffer+g, BUFLEN-g,
"<FORM><TABLE>\n"
"<TH><CENTER>%s<BR></CENTER><CENTER><SELECT NAME=\"selname\" MULTIPLE SIZE=3>\n",
	URLName);
    FLUSH_BUFFER
    dataNum = 0;

    /* write out details of each data item for user */
    while ( (data=(si_SignonDataStruct *) XP_ListNextObject(data_ptr)) ) {
	g += PR_snprintf(buffer+g, BUFLEN-g,
"<OPTION VALUE=%d>%s=%s</OPTION>",
            dataNum, data->name, data->isPassword ? "***" : data->value);
	FLUSH_BUFFER
	dataNum++;
    }
    g += PR_snprintf(buffer+g, BUFLEN-g,
"</SELECT></CENTER></TH></TABLE></FORM>\n"
	);
    FLUSH_BUFFER

    /* free buffer since it is no longer needed */
    if (buffer) {
	XP_FREE(buffer);
    }

    /* do html dialog */
    strings = XP_GetDialogStrings(XP_EMPTY_STRINGS);
    if (!strings) {
	if (buffer2) {
	    XP_FREE(buffer2);
	}
	return;
    }
    if (buffer2) {
	XP_CopyDialogString(strings, 0, buffer2);
	XP_FREE(buffer2);
	buffer2 = NULL;
    }
    XP_MakeHTMLDialog(context, &dialogInfo, MK_SIGNON_YOUR_SIGNONS,
		strings, context, PR_FALSE);

    return;
}

PR_STATIC_CALLBACK(PRBool)
si_SignonInfoDialogDone
    (XPDialogState* state, char** argv, int argc, unsigned int button)
{
    XP_List *URL_ptr;
    XP_List *user_ptr;
    XP_List *data_ptr;
    si_SignonURLStruct *URL;
    si_SignonURLStruct *URLToDelete = 0;
    si_SignonUserStruct* user;
    si_SignonUserStruct* userToDelete = 0;
    si_SignonDataStruct* data;

    char *buttonName, *userNumberAsString;
    int userNumber;
    char* gone;

    /* test for cancel */
    if (button == XP_DIALOG_CANCEL_BUTTON) {
	/* CANCEL button was pressed */
	return PR_FALSE;
    }

    /* get button name */
    buttonName = XP_FindValueInArgs("button", argv, argc);
    if (buttonName &&
	    !XP_STRCASECMP(buttonName, XP_GetString(SA_VIEW_BUTTON_LABEL))) {

	/* view button was pressed */

        /* get "selname" value in argv list */
        if (!(userNumberAsString = XP_FindValueInArgs("selname", argv, argc))) {
	    /* no selname in argv list */
	    return(PR_TRUE);
	}

        /* convert "selname" value from string to an integer */
	userNumber = atoi(userNumberAsString);

	/* step to the user corresponding to that integer */
	si_lock_signon_list();
	URL_ptr = si_signon_list;
	while ((URL = (si_SignonURLStruct *) XP_ListNextObject(URL_ptr))) {
	    user_ptr = URL->signonUser_list;
	    while ((user = (si_SignonUserStruct *)
		    XP_ListNextObject(user_ptr))) {
		if (--userNumber < 0) {
		    si_DisplayUserInfoAsHTML
			((MWContext *)(state->arg), user, URL->URLName);
		    return(PR_TRUE);
		}
	    }
	}
	si_unlock_signon_list();
    }

    /* OK was pressed, do the deletions */

    /* first get the comma-separated sequence of signons to be deleted */
    gone = XP_FindValueInArgs("goneC", argv, argc);
    XP_ASSERT(gone);
    if (!gone) {
	return PR_FALSE;
    }

    /* step through all users and delete those that are in the sequence
     * Note: we can't delete user while "user_ptr" is pointing to it because
     * that would destroy "user_ptr". So we do a lazy deletion
     */
    userNumber = 0;
    URL_ptr = si_signon_list;
    while ((URL = (si_SignonURLStruct *) XP_ListNextObject(URL_ptr))) {
	user_ptr = URL->signonUser_list;
	while ((user = (si_SignonUserStruct *)
		XP_ListNextObject(user_ptr))) {
	    if (si_InSequence(gone, userNumber)) {
		if (userToDelete) {
                    /* get to first data item -- that's the user name */
		    data_ptr = userToDelete->signonData_list;
		    data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr);
		    /* do the deletion */
		    SI_RemoveUser(URLToDelete->URLName, data->value, TRUE);
		}
		userToDelete = user;
		URLToDelete = URL;
	    }
	    userNumber++;
	}
    }
    if (userToDelete) {
        /* get to first data item -- that's the user name */
		data_ptr = userToDelete->signonData_list;
		data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr);
		/* do the deletion */
		SI_RemoveUser(URLToDelete->URLName, data->value, TRUE);

		si_signon_list_changed = TRUE;
		si_SaveSignonDataLocked(NULL);
    }

    return PR_FALSE;
}

MODULE_PRIVATE void
SI_DisplaySignonInfoAsHTML(ActiveEntry * cur_entry)
{
    char *buffer = (char*)XP_ALLOC(BUFLEN);
    char *buffer2 = 0;
    int g = 0, userNumber;
    XP_List *URL_ptr;
    XP_List *user_ptr;
    XP_List *data_ptr;
    si_SignonURLStruct *URL;
    si_SignonUserStruct * user;
    si_SignonDataStruct* data;

    MWContext *context = cur_entry->window_id;

    static XPDialogInfo dialogInfo = {
	XP_DIALOG_OK_BUTTON | XP_DIALOG_CANCEL_BUTTON,
	si_SignonInfoDialogDone,
	300,
	420
    };

    XPDialogStrings* strings;
    StrAllocCopy(buffer2, "");

    /* Write out the javascript */
    g += PR_snprintf(buffer+g, BUFLEN-g,
"<script>\n"
"function DeleteSignonSelected() {\n"
"  selname = document.theform.selname;\n"
"  goneC = document.theform.goneC;\n"
"  var p;\n"
"  var i;\n"
"  for (i=selname.options.length-1 ; i>=0 ; i--) {\n"
"    if (selname.options[i].selected) {\n"
"      selname.options[i].selected = 0;\n"
"      goneC.value = goneC.value + \",\" + selname.options[i].value;\n"
"      for (j=i ; j<selname.options.length ; j++) {\n"
"        selname.options[j] = selname.options[j+1];\n"
"      }\n"
"    }\n"
"  }\n"
"}\n"
"</script>\n"
	);
    FLUSH_BUFFER

    /* force loading of the signons file */
    si_RegisterSignonPrefCallbacks();
    si_lock_signon_list();
    URL_ptr = si_signon_list;

    /* Write out each URL */
    g += PR_snprintf(buffer+g, BUFLEN-g,
"<FORM><TABLE>\n"
"<TH><CENTER>%s<BR></CENTER><CENTER><SELECT NAME=\"selname\" MULTIPLE SIZE=15>\n",
	XP_GetString(MK_SIGNON_YOUR_SIGNONS));
    FLUSH_BUFFER
    userNumber = 0;
    while ( (URL=(si_SignonURLStruct *) XP_ListNextObject(URL_ptr)) ) {
	user_ptr = URL->signonUser_list;
	while ( (user=(si_SignonUserStruct *) XP_ListNextObject(user_ptr)) ) {
	    /* first data item for user is the username */
	    data_ptr = user->signonData_list;
	    data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr);
	    g += PR_snprintf(buffer+g, BUFLEN-g,
"<OPTION VALUE=%d>%s: %s</OPTION>",
		userNumber, URL->URLName, data->value);
	    FLUSH_BUFFER
	    userNumber++;
	}
    }
    si_unlock_signon_list();
    g += PR_snprintf(buffer+g, BUFLEN-g,
"</SELECT></CENTER>\n"
	);
    FLUSH_BUFFER

    g += PR_snprintf(buffer+g, BUFLEN-g,
"<CENTER>\n"
"<INPUT TYPE=\"BUTTON\" VALUE=%s ONCLICK=\"DeleteSignonSelected();\">\n"
"<INPUT TYPE=\"BUTTON\" NAME=\"view\" VALUE=%s ONCLICK=\"parent.clicker(this,window.parent)\">\n"
"<INPUT TYPE=\"HIDDEN\" NAME=\"goneC\" VALUE=\"\">\n"
"</CENTER></TH>\n"
"</TABLE></FORM>\n",
	XP_GetString(SA_REMOVE_BUTTON_LABEL),
	XP_GetString(SA_VIEW_BUTTON_LABEL) );
    FLUSH_BUFFER

    /* free buffer since it is no longer needed */
    if (buffer) {
	XP_FREE(buffer);
    }

    /* do html dialog */
    strings = XP_GetDialogStrings(XP_EMPTY_STRINGS);
    if (!strings) {
	if (buffer2) {
	    XP_FREE(buffer2);
	}
	return;
    }
    if (buffer2) {
	XP_CopyDialogString(strings, 0, buffer2);
	XP_FREE(buffer2);
	buffer2 = NULL;
    }
    XP_MakeHTMLDialog(context, &dialogInfo, MK_SIGNON_YOUR_SIGNONS,
		strings, context, PR_FALSE);

    return;
}

#endif
