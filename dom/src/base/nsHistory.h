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
#ifndef nsHistory_h___
#define nsHistory_h___

#include "nsIScriptObjectOwner.h"
#include "nsIDOMHistory.h"
#include "nsISupports.h"
#include "nscore.h"
#include "nsIScriptContext.h"

class nsIDocShell;

// Script "History" object
class HistoryImpl : public nsIScriptObjectOwner, public nsIDOMHistory {
public:
  HistoryImpl(nsIDocShell* aDocShell);
  virtual ~HistoryImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  NS_IMETHOD_(void)       SetDocShell(nsIDocShell *aDocShell);

  NS_IMETHOD    GetLength(PRInt32* aLength);
  NS_IMETHOD    GetCurrent(nsString& aCurrent);
  NS_IMETHOD    GetPrevious(nsString& aPrevious);
  NS_IMETHOD    GetNext(nsString& aNext);
  NS_IMETHOD    Back();
  NS_IMETHOD    Forward();
  NS_IMETHOD    Go(JSContext* cx, jsval* argv, PRUint32 argc);
  NS_IMETHOD    Item(PRUint32 aIndex, nsString& aReturn);

protected:
  nsIDocShell* mDocShell;
  void *mScriptObject;
};

#endif /* nsHistory_h___ */
