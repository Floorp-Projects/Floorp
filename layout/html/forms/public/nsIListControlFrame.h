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

#ifndef nsIListControlFrame_h___
#define nsIListControlFrame_h___

#include "nsISupports.h"
#include "nsFont.h"
class nsFormFrame;
class nsIPresContext;
class nsString;
class nsIContent;


// IID for the nsIListControlFrame class
#define NS_ILISTCONTROLFRAME_IID    \
{ 0xf44db101, 0xa73c, 0x11d2,  \
  { 0x8d, 0xcf, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

/** 
  * nsIListControlFrame is the interface for frame-based listboxes.
  */
class nsIListControlFrame : public nsISupports {

public:

  /**
   * Sets the ComboBoxFrame
   *
   */
  NS_IMETHOD SetComboboxFrame(nsIFrame* aComboboxFrame) = 0;

  /**
   * Get the Selected Item's String
   *
   */
  NS_IMETHOD GetSelectedItem(nsString & aStr) = 0;

  /**
   * Tells the list it is about to drop down
   *
   */
  NS_IMETHOD AboutToDropDown() = 0;

  /**
   * Initiates mouse capture for the listbox
   *
   */
  NS_IMETHOD CaptureMouseEvents(PRBool aGrabMouseEvents) = 0;


};

#endif

