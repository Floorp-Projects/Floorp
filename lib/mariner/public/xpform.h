/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   xpform.h --- an API for accessing form element information.
   Created: Chris Toshok <toshok@netscape.com>, 29-Oct-1997.
 */


#ifndef _xp_forms_h
#define _xp_forms_h

#include "xp_mem.h"
#include "xp_core.h"
#include "ntypes.h"
#include "lo_ele.h"

XP_BEGIN_PROTOS

extern LO_FormElementData*	XP_GetFormElementData(LO_FormElementStruct *form);
extern LO_TextAttr*	XP_GetFormTextAttr(LO_FormElementStruct *form_element);
extern uint16 XP_GetFormEleAttrmask(LO_FormElementStruct *form_element);

/*
** Setters/Getters for LO_FormElementData fields.
*/
extern int32	XP_FormGetType(LO_FormElementData *form);

extern void	XP_FormSetFEData(LO_FormElementData *form, void* fe_data);
extern void*	XP_FormGetFEData(LO_FormElementData *form);

/* Methods that work on Hidden, Submit, Reset, Button, and Readonly form
   elements. */
extern PA_Block XP_FormGetValue(LO_FormElementData *form);

extern Bool	XP_FormGetDisabled(LO_FormElementData *form);

/* Methods that work on text or text area form elements */
extern PA_Block	XP_FormGetDefaultText(LO_FormElementData *text_or_text_area_element);

extern PA_Block	XP_FormGetCurrentText(LO_FormElementData *text_or_text_area_element);
extern void	XP_FormSetCurrentText(LO_FormElementData *text_or_text_area_element,
							  PA_Block current_text);
extern Bool	XP_FormGetReadonly(LO_FormElementData *text_or_text_area_element);

/* text specific methods. */
extern int32	XP_FormTextGetSize(LO_FormElementData *text_element);
extern int32	XP_FormTextGetMaxSize(LO_FormElementData *text_element);

/* text area specific methods. */
extern void	XP_FormTextAreaGetDimensions(LO_FormElementData *text_area_element,
									 int32 *rows, int32 *cols);
extern uint8	XP_FormTextAreaGetAutowrap(LO_FormElementData *text_area_element);

/* radio/checkbox specific methods */
extern Bool	XP_FormGetDefaultToggled(LO_FormElementData *radio_or_checkbox_element);
extern Bool	XP_FormGetElementToggled(LO_FormElementData *radio_or_checkbox_element);
extern void	XP_FormSetElementToggled(LO_FormElementData *radio_or_checkbox_element,
								 Bool toggled);

/* select specific methods */
extern int32	XP_FormSelectGetSize(LO_FormElementData *select_element);
extern Bool	XP_FormSelectGetMultiple(LO_FormElementData *select_element);
extern Bool	XP_FormSelectGetOptionsValid(LO_FormElementData *select_element); /* XXX ? */
extern int32	XP_FormSelectGetOptionsCount(LO_FormElementData *sel_element);
extern lo_FormElementOptionData *XP_FormSelectGetOption(LO_FormElementData *select_element,
														int option_index);

/* option specific methods */
extern Bool	XP_FormOptionGetDefaultSelected(lo_FormElementOptionData *element);
extern Bool	XP_FormOptionGetSelected(lo_FormElementOptionData *option_element);
extern void	XP_FormOptionSetSelected(lo_FormElementOptionData *option_element,
									 Bool selected);

/* object */
extern LO_JavaAppStruct*	XP_FormObjectGetJavaApp(LO_FormElementData *object_element);
extern void	XP_FormObjectSetJavaApp(LO_FormElementData *object_element,
									LO_JavaAppStruct *java_app);
/* keygen */
extern PA_Block	XP_FormKeygenGetChallenge(LO_FormElementData *keygen_element);
extern PA_Block	XP_FormKeygenGetKeytype(LO_FormElementData *keygen_element);
extern PA_Block	XP_FormKeygenGetPQG(LO_FormElementData *keygen_element);
extern char*	XP_FormKeygenGetValueStr(LO_FormElementData *keygen_element);
/* setter routines? */
											
XP_END_PROTOS

#endif /* _xp_forms_h */
