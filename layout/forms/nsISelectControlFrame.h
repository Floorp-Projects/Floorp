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

#ifndef nsISelectControlFrame_h___
#define nsISelectControlFrame_h___

#include "nsISupports.h"

// IID for the nsISelectControlFrame class
#define NS_ISELECTCONTROLFRAME_IID    \
{ 0x162a2ae3, 0x5a79, 0x11d3,  \
  { 0x96, 0xea, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }
// {162A2AE3-5A79-11d3-96EA-0060B0FB9956}

/** 
  * nsISelectControlFrame is the interface for combo boxes and listboxes
  */
class nsISelectControlFrame : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISELECTCONTROLFRAME_IID)

  /**
   * Adds an option to the list at index
   */

  NS_IMETHOD AddOption(PRInt32 index) = 0;

  /**
   * Removes the option at index
   */

  NS_IMETHOD RemoveOption(PRInt32 index) = 0; 

  /**
   * Sets the select state of the option at index
   */

  NS_IMETHOD SetOptionSelected(PRInt32 index, PRBool value) = 0;
};

#endif
