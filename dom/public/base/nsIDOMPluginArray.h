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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMPluginArray_h__
#define nsIDOMPluginArray_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMPlugin;

#define NS_IDOMPLUGINARRAY_IID \
 { 0xf6134680, 0xf28b, 0x11d2, { 0x83, 0x60, 0xc9, 0x08, 0x99, 0x04, 0x9c, 0x3c } } 

class nsIDOMPluginArray : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMPLUGINARRAY_IID; return iid; }

  NS_IMETHOD    GetLength(PRUint32* aLength)=0;

  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMPlugin** aReturn)=0;

  NS_IMETHOD    NamedItem(const nsString& aName, nsIDOMPlugin** aReturn)=0;

  NS_IMETHOD    Refresh(PRBool aReloadDocuments)=0;
};


#define NS_DECL_IDOMPLUGINARRAY   \
  NS_IMETHOD    GetLength(PRUint32* aLength);  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMPlugin** aReturn);  \
  NS_IMETHOD    NamedItem(const nsString& aName, nsIDOMPlugin** aReturn);  \
  NS_IMETHOD    Refresh(PRBool aReloadDocuments);  \



#define NS_FORWARD_IDOMPLUGINARRAY(_to)  \
  NS_IMETHOD    GetLength(PRUint32* aLength) { return _to GetLength(aLength); } \
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMPlugin** aReturn) { return _to Item(aIndex, aReturn); }  \
  NS_IMETHOD    NamedItem(const nsString& aName, nsIDOMPlugin** aReturn) { return _to NamedItem(aName, aReturn); }  \
  NS_IMETHOD    Refresh(PRBool aReloadDocuments) { return _to Refresh(aReloadDocuments); }  \


extern "C" NS_DOM nsresult NS_InitPluginArrayClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptPluginArray(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMPluginArray_h__
