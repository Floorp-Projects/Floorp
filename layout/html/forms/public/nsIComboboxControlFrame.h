/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIComboboxControlFrame_h___
#define nsIComboboxControlFrame_h___

#include "nsISupports.h"
#include "nsFont.h"

class nsFormFrame;
class nsIPresContext;
class nsString;
class nsIContent;
class nsVoidArray;


// IID for the nsIComboboxControlFrame class
#define NS_ICOMBOBOXCONTROLFRAME_IID    \
{ 0x6961f791, 0xa662, 0x11d2,  \
  { 0x8d, 0xcf, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

/** 
  * nsIComboboxControlFrame is the common interface for frames of form controls. It
  * provides a uniform way of creating widgets, resizing, and painting.
  * @see nsLeafFrame and its base classes for more info
  */
class nsIComboboxControlFrame : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICOMBOBOXCONTROLFRAME_IID)

  /**
   * Indicates whether the list is dropped down
   *
   */
  NS_IMETHOD IsDroppedDown(PRBool * aDoDropDown) = 0;

  /**
   * Shows or hides the drop down
   *
   */
  NS_IMETHOD ShowDropDown(PRBool aDoDropDown) = 0;

  /**
   * Gets the Drop Down List
   *
   */
  NS_IMETHOD GetDropDown(nsIFrame** aDropDownFrame) = 0;

  /**
   * Sets the Drop Down List
   *
   */
  NS_IMETHOD SetDropDown(nsIFrame* aDropDownFrame) = 0;

  /**
   * Notifies the Combobox the List was selected
   *
   */
  NS_IMETHOD ListWasSelected(nsIPresContext* aPresContext) = 0;

  /**
   * Asks the Combobox to update the display frame
   * aDoDispatchEvent - indicates whether an event should be dispatched to the DOM
   * aForceUpdate - indicates whether the indexx and the text value should both be
   *                whether the index has changed or not.
   *
   */
  NS_IMETHOD UpdateSelection(PRBool aDoDispatchEvent, PRBool aForceUpdate, PRInt32 aNewIndex) = 0;


};

#endif

