/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsDOMStyleDeclaration_h___
#define nsDOMStyleDeclaration_h___

#include "nsISupports.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIScriptObjectOwner.h"

class nsIHTMLContent;
class nsICSSDeclaration;

class nsDOMStyleDeclaration : public nsIDOMCSSStyleDeclaration,
                              public nsIScriptObjectOwner
{
public:
  nsDOMStyleDeclaration(nsIHTMLContent *aContent);
  virtual ~nsDOMStyleDeclaration();

  NS_DECL_ISUPPORTS

  NS_DECL_IDOMCSSSTYLEDECLARATION
  
  // nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  void DropContent();

protected:
  nsresult GetContentStyle(nsICSSDeclaration **aDecl, PRBool aAllocate);

  void *mScriptObject;
  nsIHTMLContent *mContent;
};

#endif // nsDOMStyleDeclaration_h___
