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
/* 
   xpform.c --- an API for accessing form element information.
   Created: Chris Toshok <toshok@netscape.com>, 29-Oct-1997.
 */


#include "xpform.h"
#include "xpassert.h"

LO_FormElementData*
XP_GetFormElementData(LO_FormElementStruct *form)
{
  return form->element_data;
}

LO_TextAttr*
XP_GetFormTextAttr(LO_FormElementStruct *form)
{
  return form->text_attr;
}

uint16
XP_GetFormEleAttrmask(LO_FormElementStruct *form)
{
  return form->ele_attrmask;
}

int32   
XP_FormGetType(LO_FormElementData *form)
{
  return form->type;
}

void    
XP_FormSetFEData(LO_FormElementData *form, 
				 void* fe_data)
{
  form->ele_minimal.FE_Data = fe_data;
}


void*   
XP_FormGetFEData(LO_FormElementData *form)
{
  return form->ele_minimal.FE_Data;
}

/* Methods that work for Hidden, Submit, Reset, Button, and Readonly
   form elements. */
PA_Block
XP_FormGetValue(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_HIDDEN
			|| form->type == FORM_TYPE_SUBMIT
			|| form->type == FORM_TYPE_RESET
			|| form->type == FORM_TYPE_BUTTON
			|| form->type == FORM_TYPE_READONLY);

  return form->ele_minimal.value;
}

Bool
XP_FormGetDisabled(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_SELECT_ONE
			|| form->type == FORM_TYPE_SELECT_MULT
			|| form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_TEXTAREA
			|| form->type == FORM_TYPE_RADIO
			|| form->type == FORM_TYPE_CHECKBOX
			|| form->type == FORM_TYPE_SUBMIT
			|| form->type == FORM_TYPE_RESET
			|| form->type == FORM_TYPE_BUTTON
			|| form->type == FORM_TYPE_READONLY);

  if (form->type == FORM_TYPE_SELECT_ONE
			|| form->type == FORM_TYPE_SELECT_MULT
			|| form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_TEXTAREA
			|| form->type == FORM_TYPE_RADIO
			|| form->type == FORM_TYPE_CHECKBOX
			|| form->type == FORM_TYPE_SUBMIT
			|| form->type == FORM_TYPE_RESET
			|| form->type == FORM_TYPE_BUTTON
			|| form->type == FORM_TYPE_READONLY)
    return form->ele_minimal.disabled;
  else
    return FALSE;
}

/* Methods that work on text or text area form elements */
PA_Block
XP_FormGetDefaultText(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_TEXTAREA
			|| form->type == FORM_TYPE_PASSWORD
			|| form->type == FORM_TYPE_READONLY
			|| form->type == FORM_TYPE_FILE);
  
  switch(form->type)
	{
	case FORM_TYPE_TEXT:
	case FORM_TYPE_PASSWORD:
	case FORM_TYPE_FILE:
	case FORM_TYPE_READONLY:
	  return form->ele_text.default_text;
	case FORM_TYPE_TEXTAREA:
	  return form->ele_textarea.default_text;
	default:
	  return NULL;
	}
}

PA_Block
XP_FormGetCurrentText(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_TEXTAREA
			|| form->type == FORM_TYPE_PASSWORD
			|| form->type == FORM_TYPE_READONLY
			|| form->type == FORM_TYPE_FILE);
  
  switch(form->type)
	{
	case FORM_TYPE_TEXT:
	case FORM_TYPE_PASSWORD:
	case FORM_TYPE_FILE:
	case FORM_TYPE_READONLY:
	  return form->ele_text.current_text;
	case FORM_TYPE_TEXTAREA:
	  return form->ele_textarea.current_text;
	default:
	  return NULL;
	}
}

void    
XP_FormSetCurrentText(LO_FormElementData *form,
					  PA_Block current_text)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_TEXTAREA
			|| form->type == FORM_TYPE_PASSWORD
			|| form->type == FORM_TYPE_READONLY
			|| form->type == FORM_TYPE_FILE);
  
  switch(form->type)
	{
	case FORM_TYPE_TEXT:
	case FORM_TYPE_PASSWORD:
	case FORM_TYPE_FILE:
	case FORM_TYPE_READONLY:
	  form->ele_text.current_text = current_text;
	  break;
	case FORM_TYPE_TEXTAREA:
	  form->ele_textarea.current_text = current_text;
	  break;
	}
}

Bool
XP_FormGetReadonly(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_TEXTAREA
			|| form->type == FORM_TYPE_PASSWORD
			|| form->type == FORM_TYPE_READONLY);
  
  switch(form->type)
	{
	case FORM_TYPE_TEXT:
	case FORM_TYPE_PASSWORD:
	case FORM_TYPE_READONLY:
	  return form->ele_text.read_only;
	case FORM_TYPE_TEXTAREA:
	  return form->ele_textarea.read_only;
	default:
	  return FALSE;
	}
}

/* text specific methods. */
int32   
XP_FormTextGetSize(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_PASSWORD
			|| form->type == FORM_TYPE_READONLY
			|| form->type == FORM_TYPE_FILE);
  
  return form->ele_text.size;
}

int32   
XP_FormTextGetMaxSize(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_PASSWORD
			|| form->type == FORM_TYPE_READONLY
			|| form->type == FORM_TYPE_FILE);
  
  return form->ele_text.max_size;
}

/* text area specific methods. */
void    
XP_FormTextAreaGetDimensions(LO_FormElementData *form,
							 int32 *rows, 
							 int32 *cols)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXTAREA);

  if (rows)
	*rows = form->ele_textarea.rows;
  if (cols)
	*cols = form->ele_textarea.cols;
}

uint8   
XP_FormTextAreaGetAutowrap(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXTAREA);

  return form->ele_textarea.auto_wrap;
}

/* radio/checkbox specific methods */
Bool    
XP_FormGetDefaultToggled(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_RADIO
			|| form->type == FORM_TYPE_CHECKBOX);

  return form->ele_toggle.default_toggle;
}

Bool    
XP_FormGetElementToggled(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_RADIO
			|| form->type == FORM_TYPE_CHECKBOX);

  return form->ele_toggle.toggled;
}

void    
XP_FormSetElementToggled(LO_FormElementData *form,
						 Bool toggled)
{
  XP_ASSERT(form->type == FORM_TYPE_RADIO
			|| form->type == FORM_TYPE_CHECKBOX);

  form->ele_toggle.toggled = toggled;
}

/* select specific methods */
int32   
XP_FormSelectGetSize(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_SELECT_ONE
			|| form->type == FORM_TYPE_SELECT_MULT);

  return form->ele_select.size;
}

Bool    
XP_FormSelectGetMultiple(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_SELECT_ONE
			|| form->type == FORM_TYPE_SELECT_MULT);

  return form->ele_select.multiple;
}

Bool    
XP_FormSelectGetOptionsValid(LO_FormElementData *form) /* XXX ? */
{
  XP_ASSERT(form->type == FORM_TYPE_SELECT_ONE
			|| form->type == FORM_TYPE_SELECT_MULT);

  return form->ele_select.options_valid;
}

int32   
XP_FormSelectGetOptionsCount(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_SELECT_ONE
			|| form->type == FORM_TYPE_SELECT_MULT);

  return form->ele_select.option_cnt;
}

lo_FormElementOptionData*
XP_FormSelectGetOption(LO_FormElementData *form,
					   int option_index)
{
  XP_ASSERT((form->type == FORM_TYPE_SELECT_ONE
			 || form->type == FORM_TYPE_SELECT_MULT)
			&& option_index >= 0
			&& option_index < form->ele_select.option_cnt);
  
  return &((lo_FormElementOptionData*)form->ele_select.options)[option_index];
}

/* option specific methods */
Bool    
XP_FormOptionGetDefaultSelected(lo_FormElementOptionData *option_element)
{
  return option_element->def_selected;
}

Bool    
XP_FormOptionGetSelected(lo_FormElementOptionData *option_element)
{
  return option_element->selected;
}

void    
XP_FormOptionSetSelected(lo_FormElementOptionData *option_element, 
						 Bool selected)
{
  option_element->selected = selected;
}

/* object */
LO_JavaAppStruct*   
XP_FormObjectGetJavaApp(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_OBJECT);

  return form->ele_object.object;
}

void    
XP_FormObjectSetJavaApp(LO_FormElementData *form,
						LO_JavaAppStruct *java_app)
{
  XP_ASSERT(form->type == FORM_TYPE_OBJECT);

  form->ele_object.object = java_app;
}

/* keygen */
PA_Block    
XP_FormKeygenGetChallenge(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_KEYGEN);

  return form->ele_keygen.challenge;
}

PA_Block    
XP_FormKeygenGetKeytype(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_KEYGEN);

  return form->ele_keygen.key_type;
}

PA_Block    
XP_FormKeygenGetPQG(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_KEYGEN);

  return form->ele_keygen.pqg;
}

char*       
XP_FormKeygenGetValueStr(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_KEYGEN);

  return form->ele_keygen.value_str;
}

/* setter routines? */
