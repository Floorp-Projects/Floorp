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
{ 0x6f7652ea,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMNamedNodeMap : public nsISupports {
public:

  NS_IMETHOD    GetLength(PRUint32* aLength)=0;

  NS_IMETHOD    GetNamedItem(const nsString& aName, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    SetNamedItem(nsIDOMNode* aNode)=0;

  NS_IMETHOD    RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn)=0;
};


#define NS_DECL_IDOMNAMEDNODEMAP   \
  NS_IMETHOD    GetLength(PRUint32* aLength);  \
  NS_IMETHOD    GetNamedItem(const nsString& aName, nsIDOMNode** aReturn);  \
  NS_IMETHOD    SetNamedItem(nsIDOMNode* aNode);  \
  NS_IMETHOD    RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn);  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);  \



#define NS_FORWARD_IDOMNAMEDNODEMAP(_to)  \
  NS_IMETHOD    GetLength(PRUint32* aLength) { return _to##GetLength(aLength); } \
  NS_IMETHOD    GetNamedItem(const nsString& aName, nsIDOMNode** aReturn) { return _to##GetNamedItem(aName, aReturn); }  \
  NS_IMETHOD    SetNamedItem(nsIDOMNode* aNode) { return _to##SetNamedItem(aNode); }  \
  NS_IMETHOD    RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn) { return _to##RemoveNamedItem(aName, aReturn); }  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn) { return _to##Item(aIndex, aReturn); }  \


extern nsresult NS_InitNamedNodeMapClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptNamedNodeMap(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMNamedNodeMap_h__
