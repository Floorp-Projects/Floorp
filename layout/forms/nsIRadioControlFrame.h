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

#ifndef nsIRadioControlFrame_h___
#define nsIRadioControlFrame_h___

#include "nsISupports.h"
class nsIStyleContext;

// IID for the nsIRadioControlFrame class
// {06450E00-24D9-11d3-966B-00105A1B1B76}
#define NS_IRADIOCONTROLFRAME_IID    \
{ 0x6450e00, 0x24d9, 0x11d3,  \
  { 0x96, 0x6b, 0x0, 0x10, 0x5a, 0x1b, 0x1b, 0x76 } }


/** 
  * nsIRadioControlFrame is the common interface radio buttons.
  * @see nsFromControlFrame and its base classes for more info
  */
class nsIRadioControlFrame : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRADIOCONTROLFRAME_IID)

  /**
   * Sets the Pseudo Style Contexts for the Radio button
   *
   */
   NS_IMETHOD SetRadioButtonFaceStyleContext(nsIStyleContext *aRadioButtonFaceStyleContext) = 0;
};

#endif

