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

#ifndef nsIDOMElement_h__
#define nsIDOMElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMNode.h"

class nsIDOMAttributeList;
class nsIDOMElement;
class nsIDOMNodeIterator;
class nsIDOMAttribute;

#define NS_IDOMELEMENT_IID \
{ 0x6f7652e7,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMElement : public nsIDOMNode {
public:

  NS_IMETHOD    GetTagName(nsString& aReturn)=0;

  NS_IMETHOD    GetAttributes(nsIDOMAttributeList** aReturn)=0;

  NS_IMETHOD    GetDOMAttribute(nsString& aName, nsString& aReturn)=0;

  NS_IMETHOD    SetDOMAttribute(nsString& aName, nsString& aValue)=0;

  NS_IMETHOD    RemoveAttribute(nsString& aName)=0;

  NS_IMETHOD    GetAttributeNode(nsString& aName, nsIDOMAttribute** aReturn)=0;

  NS_IMETHOD    SetAttributeNode(nsIDOMAttribute* aNewAttr)=0;

  NS_IMETHOD    RemoveAttributeNode(nsIDOMAttribute* aOldAttr)=0;

  NS_IMETHOD    GetElementsByTagName(nsString& aTagname, nsIDOMNodeIterator** aReturn)=0;

  NS_IMETHOD    Normalize()=0;
};

extern nsresult NS_InitElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM NS_NewScriptElement(nsIScriptContext *aContext, nsIDOMElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMElement_h__
