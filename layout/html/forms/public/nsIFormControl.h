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
#ifndef nsIFormControl_h___
#define nsIFormControl_h___

#include "nsISupports.h"
class nsIFormManager;

#define NS_IFORMCONTROL_IID   \
{ 0x282ff440, 0xcd7e, 0x11d1, \
  {0x89, 0xad, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

class nsIFormControl : public nsISupports {
public:
  virtual PRBool GetName(nsString& aResult) = 0;

  virtual PRInt32 GetMaxNumValues() = 0;

  virtual PRBool GetValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                           nsString* aValues) = 0;

  virtual void Reset() = 0;

  virtual nsIFormManager* GetFormManager() const = 0;

  virtual void SetFormManager(nsIFormManager* aFormMan, PRBool aDecrementRef = PR_TRUE) = 0;

  virtual nsrefcnt GetRefCount() const = 0;
};

#endif /* nsIFormControl_h___ */
