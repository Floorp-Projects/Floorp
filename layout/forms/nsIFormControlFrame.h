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

#ifndef nsIFormControlFrame_h___
#define nsIFormControlFrame_h___

#include "nsISupports.h"
// Forward declarations
class nsFormFrame;
class nsIPresContext;
class nsString;
class nsIAtom;

// IID for the nsIFormControlFrame class
#define NS_IFORMCONTROLFRAME_IID    \
{ 0x38eb3980, 0x4d99, 0x11d2,  \
  { 0x80, 0x3f, 0x0, 0x60, 0x8, 0x15, 0xa7, 0x91 } }

/** 
  * nsIFormControlFrame is the common interface for frames of form controls. It
  * provides a uniform way of creating widgets, resizing, and painting.
  * @see nsLeafFrame and its base classes for more info
  */
class nsIFormControlFrame : public nsISupports {

public:

  NS_IMETHOD GetType(PRInt32* aType) const =  0;

  NS_IMETHOD GetName(nsString* aName) = 0;

  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue) = 0; 

  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue) = 0; 

  virtual void SetFocus(PRBool aOn = PR_TRUE, PRBool aRepaint = PR_FALSE) = 0;

  virtual void MouseClicked(nsIPresContext* aPresContext) = 0;

  virtual void Reset() = 0;

  virtual PRBool IsSuccessful(nsIFormControlFrame* aSubmitter) = 0;

  virtual PRInt32 GetMaxNumValues() = 0;

  virtual PRBool  GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                 nsString* aValues, nsString* aNames) = 0;

  virtual void SetFormFrame(nsFormFrame* aFrame) = 0;
};

#endif

