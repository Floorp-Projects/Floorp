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
#ifndef nsHistory_h___
#define nsHistory_h___

#include "nsIScriptObjectOwner.h"
#include "nsIDOMHistory.h"
#include "nsISupports.h"
#include "nscore.h"
#include "nsIScriptContext.h"

class nsIWebShell;

// Script "History" object
class HistoryImpl : public nsIScriptObjectOwner, public nsIDOMHistory {
public:
  HistoryImpl();
  virtual ~HistoryImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  NS_IMETHOD_(void)       SetWebShell(nsIWebShell *aWebShell);

  NS_IMETHOD    GetLength(PRInt32* aLength);
  NS_IMETHOD    GetCurrent(nsString& aCurrent);
  NS_IMETHOD    GetPrevious(nsString& aPrevious);
  NS_IMETHOD    GetNext(nsString& aNext);
  NS_IMETHOD    Back();
  NS_IMETHOD    Forward();
  NS_IMETHOD    Go(PRInt32 aIndex);

protected:
  nsIWebShell* mWebShell;
  void *mScriptObject;
};

#endif /* nsHistory_h___ */
