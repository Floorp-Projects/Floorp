/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   stubform.c --- stub functions dealing with front-end
                  handling of form elements.
*/

#include "stubform.h"

/*
** FE_GetFormElementInfo - create the FE representation of a form
** element.
**
** Note : This method can be called more than once for the same front
** end form element.
** 
** In those cases we don't reallocate the FEData*, and also don't
** recreate the widget, but we do set the form and context pointers in
** FEData, and we call the get_size method.
**
** Two cases I know of where layout calls this function more than once
** are: 1) when laying out a table with form elements in it -- layout
** moves our fe data over to a new LO_FormElementStruct.  So, you
** should always sync up pointers to the LO_FormElementStruct
** contained in your fe data.  2) when we're on a page with frames in
** it and the user goes back and forth.  In this case, the context is
** different, so you always need to sync that up as well.
**
** Also, layout could have told us to hide the widget without freeing
** our fe_data* (this happens when hide is TRUE in
** FE_GetFormElementValue.)  In this case, we need to do anything
** necessary to show the widget again.
**
** Also, this routine is responsible for setting the initial value of
** the form element.
**
** yuck.  :)
*/
void 
STUBFE_GetFormElementInfo (MWContext * context,
			   LO_FormElementStruct * form_element)
{
}


/*
** FE_GetFormElementValue - copy the value inside the fe
** representation into the FormElementStruct.
** 
** if hide is TRUE, hide the form element after doing the copy.  In
** the XFE, hide is taken to mean "delete" - so, if hide is TRUE, the
** XFE deletes the form element widget.  This is not strictly
** necessary, as you are guaranteed to have FE_FreeFormElement if the
** form element is *really* going away.
**
** But, given that this is called with hide == TRUE in the face of
** going back and forth in frame documents, it's probably safer to
** always delete the widget when hide == TRUE, as the widget's parent
** is destroyed and recreated.
*/
void 
STUBFE_GetFormElementValue (MWContext * context,
			    LO_FormElementStruct * form_element,
			    XP_Bool hide)
{
}

/*
** FE_ResetFormElement - reset the value of the form element to the default
** value stored in the LO_FormElementStruct.
*/
void 
STUBFE_ResetFormElement (MWContext * context,
			 LO_FormElementStruct * form_element)
{
}

/* 
** FE_SetFormElementToggle - set the toggle or radio button's state to
** the toggle parameter.
**
** only called on CHECKBOX and RADIO form elements.
*/
void 
STUBFE_SetFormElementToggle (MWContext * context,
			     LO_FormElementStruct * form_element,
			     XP_Bool toggle)
{
}

/*
** FE_FreeFormElement - free up all memory associated with the front
** end representation for this form element.
*/
void
FE_FreeFormElement(MWContext *context,
		   LO_FormElementData *form)
{
}

/*
** FE_BlurInputElement - force input to be defocused from the given
** element.  It's ok if the element didn't have the input focus.
*/
void
FE_BlurInputElement(MWContext *context,
		    LO_Element *element)
{
}

/*
** FE_FocusInputElement - force input to be focused on the given element.
** It's ok if the element already has the input focus.
*/
void
FE_FocusInputElement(MWContext *context,
		     LO_Element *element)
{
}

/*
** FE_SelectInputElement - select the contents of a form element.
**
** Only called on TEXT, TEXTAREA, PASSWORD, and FILE form elements (anything
** with text in it.)
**
** This function should select the entire text contained in the FE widget.
*/
void
FE_SelectInputElement(MWContext *context,
		      LO_Element *element)
{
}

/*
** FE_ClickInputElement - Simulate a click on a form element.
**
** Note: This function also handles clicks on HREF anchored LO_IMAGE
** and LO_TEXT LO_Elements.  In these cases, this function should
** simulate a user clicking on the LO_Element in question.
*/
void
FE_ClickInputElement(MWContext *context,
		     LO_Element *element)
{
}

/*
** FE_ChangeInputElement - handle change in form element value(s)
**
** This function should update the front end value for a given form
** element, including (in the case of TEXT-like elements) just the
** value, or (in the case of SELECT elements) both the list of
** allowable values as well as the selected value.
*/
void
FE_ChangeInputElement(MWContext *context,
		      LO_Element *element)
{
}

/*
** FE_SubmitInputElement -
**
** Tell the FE that a form is being submitted without a UI gesture indicating
** that fact, i.e., in a Mocha-automated fashion ("document.myform.submit()").
** The FE is responsible for emulating whatever happens when the user hits the
** submit button, or auto-submits by typing Enter in a single-field form.
*/
void
FE_SubmitInputElement(MWContext *context,
		      LO_Element *element)
{
}

int
FE_ClickAnyElement(MWContext *context,
		   LO_Element *element,
		   int haveXY,
		   int32 xx,
		   int32 yy)
{
}
