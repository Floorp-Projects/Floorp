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

#ifndef nsICheckControlFrame_h___
#define nsICheckControlFrame_h___

#include "nsISupports.h"
class nsIStyleContext;

// IID for the nsICheckControlFrame class
// {401347ED-0101-11d4-9706-0060B0FB9956}
#define NS_ICHECKBOXCONTROLFRAME_IID    \
{ 0x401347ed, 0x101, 0x11d4,  \
  { 0x97, 0x6, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

/** 
  * nsICheckControlFrame is the common interface radio buttons.
  * @see nsFromControlFrame and its base classes for more info
  */
class nsICheckboxControlFrame : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICHECKBOXCONTROLFRAME_IID)

  /**
   * Sets the Pseudo Style Contexts for the Check button
   *
   */
   NS_IMETHOD SetCheckboxFaceStyleContext(nsIStyleContext *aCheckboxFaceStyleContext) = 0;

  /**
   * When a user clicks on a checkbox the value needs to be set after the onmouseup
   * and before the onclick event is processed via script. The EVM always lets script
   * get first crack at the processing, and script can cancel further processing of 
   * the event by return null.
   *
   * This means the checkbox needs to have it's new value set before it goes to script 
   * to process the onclick and then if script cancels the event it needs to be set back.
   * In Nav and IE there is a flash of it being set and then unset
   * 
   * We have added this extra method to the checkbox to tell it to temporarily return the
   * opposite value while processing the click event. This way script gets the correct "future"
   * value of the checkbox, but there is no visual updating until after script is done processing.
   * That way if the event is cancelled then the checkbox will not flash.
   *
   * So get the Frame for the checkbox and tell it we are processing an onclick event
   * (see also: nsHTMLInputElement.cpp)
   */
   NS_IMETHOD SetIsInClickEvent(PRBool aVal) = 0;

  
};

#endif

