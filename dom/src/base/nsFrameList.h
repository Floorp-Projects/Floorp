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
#ifndef nsFrameList_h___
#define nsFrameList_h___

#include "nsISupports.h"
#include "nsIDOMWindowCollection.h"
#include "nsIScriptObjectOwner.h"
#include "nsString.h"

class nsIWebShell;
class nsIDOMWindow;

class nsFrameList : public nsIDOMWindowCollection, public nsIScriptObjectOwner {
public:
  nsFrameList(nsIWebShell *aWebShell);
  virtual ~nsFrameList();

  NS_DECL_ISUPPORTS

  //nsIDOMWindowCollection interface
  NS_IMETHOD GetLength(PRUint32* aLength);
  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMWindow** aReturn);
  NS_IMETHOD NamedItem(const nsAReadableString& aName, nsIDOMWindow** aReturn);

  //nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);
  
  //local methods
  NS_IMETHOD SetWebShell(nsIWebShell* aWebShell);

protected:
  nsIWebShell *mWebShell;
  void *mScriptObject;
};

#endif // nsFrameList_h___
