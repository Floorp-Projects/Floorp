/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsIFormControl_h___
#define nsIFormControl_h___

#include "nsISupports.h"
class nsIDOMHTMLFormElement;

#define NS_FORM_BROWSE          0
#define NS_FORM_BUTTON_BUTTON   1
#define NS_FORM_BUTTON_RESET    2
#define NS_FORM_BUTTON_SUBMIT   3
#define NS_FORM_FIELDSET        4
#define NS_FORM_INPUT_BUTTON    5
#define NS_FORM_INPUT_CHECKBOX  6
#define NS_FORM_INPUT_FILE      7
#define NS_FORM_INPUT_HIDDEN    8
#define NS_FORM_INPUT_RESET     9
#define NS_FORM_INPUT_IMAGE    10
#define NS_FORM_INPUT_PASSWORD 11
#define NS_FORM_INPUT_RADIO    12
#define NS_FORM_INPUT_SUBMIT   13
#define NS_FORM_INPUT_TEXT     14
#define NS_FORM_LABEL          15
#define NS_FORM_OPTION         16
#define NS_FORM_OPTGROUP       17
#define NS_FORM_LEGEND         18
#define NS_FORM_SELECT         19
#define NS_FORM_TEXTAREA       20

#define NS_FORM_NOTOK          0xFFFFFFF7
#define NS_FORM_NOTSET         0xFFFFFFF7

#define NS_IFORMCONTROL_IID   \
{ 0x282ff440, 0xcd7e, 0x11d1, \
  {0x89, 0xad, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }


/**
  * Interface which all form controls (e.g. buttons, checkboxes, text,
  * radio buttons, select, etc) implement in addition to their dom specific interface. 
 **/
class nsIFormControl : public nsISupports {
public:

  static const nsIID& GetIID() { static nsIID iid = NS_IFORMCONTROL_IID; return iid; }

  /**
    * Get the form for this form control. 
    * @param aForm the form to get
    * @return NS_OK
    */
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm) = 0;

  /**
    * Set the form for this form control.
    * @param aForm the form
    * @return NS_OK
    */
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm,
                     PRBool aRemoveFromForm = PR_TRUE) = 0;

  /**
    * Get the type of this control
    * @param aType the type to be returned
    * @return NS_OK
    */
  NS_IMETHOD GetType(PRInt32* aType) = 0;

  NS_IMETHOD Init() = 0;

};

#endif /* nsIFormControl_h___ */
