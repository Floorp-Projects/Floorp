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
#ifndef nsIFormManager_h___
#define nsIFormManager_h___

#include "nsISupports.h"
class nsIPresContext;
class nsIFrame;

// IID for the nsIFormManager interface
#define NS_IFORMMANAGER_IID    \
{ 0x592daa01, 0xcb23, 0x11d1,  \
  { 0x80, 0x2d, 0x0, 0x60, 0x8, 0x15, 0xa7, 0x91 } }


/** 
  * Interface to provide submitting and resetting forms
 **/
class nsIFormManager : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFORMMANAGER_IID)
  /**
    * Reset the values of all of this manager's controls back to their
    * initial values. This is in response to a reset button being pushed.
    */
  NS_IMETHOD OnReset() = 0;

  /**
    * Submit the values of this manager's controls depending on its action,
    * method attributes. This in response to a submit button being clicked.
    * @param aPresContext the presentation context
    * @param aFrame the frame of the submit button 
    * @param aSubmitter the control that caused the submit 
    */
  NS_IMETHOD OnSubmit(nsIPresContext* aPresContext, nsIFrame* aFrame) = 0;

};

#endif /* nsIFormManager_h___ */
