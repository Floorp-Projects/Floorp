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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMPkcs11_h__
#define nsIDOMPkcs11_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMPKCS11_IID \
 {0x9fd42950, 0x25e7, 0x11d4, \
       {0x8a, 0x7d, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3}} 

class nsIDOMPkcs11 : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMPKCS11_IID; return iid; }

  NS_IMETHOD    Deletemodule(const nsAReadableString& aModuleName, PRInt32* aReturn)=0;

  NS_IMETHOD    Addmodule(const nsAReadableString& aModuleName, const nsAReadableString& aLibraryFullPath, PRInt32 aCryptoMechanismFlags, PRInt32 aCipherFlags, PRInt32* aReturn)=0;
};


#define NS_DECL_IDOMPKCS11   \
  NS_IMETHOD    Deletemodule(const nsAReadableString& aModuleName, PRInt32* aReturn);  \
  NS_IMETHOD    Addmodule(const nsAReadableString& aModuleName, const nsAReadableString& aLibraryFullPath, PRInt32 aCryptoMechanismFlags, PRInt32 aCipherFlags, PRInt32* aReturn);  \



#define NS_FORWARD_IDOMPKCS11(_to)  \
  NS_IMETHOD    Deletemodule(const nsAReadableString& aModuleName, PRInt32* aReturn) { return _to Deletemodule(aModuleName, aReturn); }  \
  NS_IMETHOD    Addmodule(const nsAReadableString& aModuleName, const nsAReadableString& aLibraryFullPath, PRInt32 aCryptoMechanismFlags, PRInt32 aCipherFlags, PRInt32* aReturn) { return _to Addmodule(aModuleName, aLibraryFullPath, aCryptoMechanismFlags, aCipherFlags, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitPkcs11Class(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptPkcs11(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMPkcs11_h__
