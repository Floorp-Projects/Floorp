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
class nsIFormControl;

// IID for the nsIFormManager interface
#define NS_IFORMMANAGER_IID    \
{ 0x592daa01, 0xcb23, 0x11d1,  \
  { 0x80, 0x2d, 0x0, 0x60, 0x8, 0x15, 0xa7, 0x91 } }

/** 
 * Abstract form manager interface. Form managers are responsible for
 * the management of form controls. This includes gathering of data
 * for form submission; resetting form controls to their initial state
 * and other programmatic control during form processing.
 */
class nsIFormManager : public nsISupports {
public:
  // callback for reset button controls. 
  virtual void OnReset() = 0;

  // callback for text and textarea controls. If there is a single
  // text/textarea and a return is entered, then this is equavalent to
  // a submit.
  virtual void OnReturn() = 0;

  // callback for submit button controls.
  virtual void OnSubmit() = 0;

  // callback for tabs on controls that can gain focus. This will
  // eventually need to be handled at the document level to support
  // the tabindex attribute.
  virtual void OnTab() = 0;

  virtual PRInt32 GetFormControlCount() const = 0;

  virtual nsIFormControl* GetFormControlAt(PRInt32 aIndex) const = 0;

  virtual PRBool AddFormControl(nsIFormControl* aFormControl) = 0;

  virtual PRBool RemoveFormControl(nsIFormControl* aFormControl, 
                                   PRBool aChildIsRef = PR_TRUE) = 0;

  virtual void SetAttribute(const nsString& aName, const nsString& aValue) = 0;

  virtual PRBool GetAttribute(const nsString& aName,
                              nsString& aResult) const = 0;

  virtual nsrefcnt GetRefCount() const = 0;
};

#endif /* nsIFormManager_h___ */
