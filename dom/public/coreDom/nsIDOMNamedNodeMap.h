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

#ifndef nsIDOMNamedNodeMap_h__
#define nsIDOMNamedNodeMap_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMNode;

#define NS_IDOMNAMEDNODEMAP_IID \
 { 0xa6cf907b, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMNamedNodeMap : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNAMEDNODEMAP_IID; return iid; }

  NS_IMETHOD    GetLength(PRUint32* aLength)=0;

  NS_IMETHOD    GetNamedItem(const nsString& aName, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    SetNamedItem(nsIDOMNode* aArg, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn)=0;
};


#define NS_DECL_IDOMNAMEDNODEMAP   \
  NS_IMETHOD    GetLength(PRUint32* aLength);  \
  NS_IMETHOD    GetNamedItem(const nsString& aName, nsIDOMNode** aReturn);  \
  NS_IMETHOD    SetNamedItem(nsIDOMNode* aArg, nsIDOMNode** aReturn);  \
  NS_IMETHOD    RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn);  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);  \



#define NS_FORWARD_IDOMNAMEDNODEMAP(_to)  \
  NS_IMETHOD    GetLength(PRUint32* aLength) { return _to GetLength(aLength); } \
  NS_IMETHOD    GetNamedItem(const nsString& aName, nsIDOMNode** aReturn) { return _to GetNamedItem(aName, aReturn); }  \
  NS_IMETHOD    SetNamedItem(nsIDOMNode* aArg, nsIDOMNode** aReturn) { return _to SetNamedItem(aArg, aReturn); }  \
  NS_IMETHOD    RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn) { return _to RemoveNamedItem(aName, aReturn); }  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn) { return _to Item(aIndex, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitNamedNodeMapClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptNamedNodeMap(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMNamedNodeMap_h__
