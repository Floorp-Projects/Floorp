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

class nsIDOMAttribute;
class nsIDOMNodeList;

#define NS_IDOMELEMENT_IID \
{ 0x6f7652e8,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMElement : public nsIDOMNode {
public:

  NS_IMETHOD    GetTagName(nsString& aTagName)=0;

  NS_IMETHOD    GetDOMAttribute(const nsString& aName, nsString& aReturn)=0;

  NS_IMETHOD    SetDOMAttribute(const nsString& aName, const nsString& aValue)=0;

  NS_IMETHOD    RemoveAttribute(const nsString& aName)=0;

  NS_IMETHOD    GetAttributeNode(const nsString& aName, nsIDOMAttribute** aReturn)=0;

  NS_IMETHOD    SetAttributeNode(nsIDOMAttribute* aNewAttr)=0;

  NS_IMETHOD    RemoveAttributeNode(nsIDOMAttribute* aOldAttr)=0;

  NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn)=0;

  NS_IMETHOD    Normalize()=0;
};


#define NS_DECL_IDOMELEMENT   \
  NS_IMETHOD    GetTagName(nsString& aTagName);  \
  NS_IMETHOD    GetDOMAttribute(const nsString& aName, nsString& aReturn);  \
  NS_IMETHOD    SetDOMAttribute(const nsString& aName, const nsString& aValue);  \
  NS_IMETHOD    RemoveAttribute(const nsString& aName);  \
  NS_IMETHOD    GetAttributeNode(const nsString& aName, nsIDOMAttribute** aReturn);  \
  NS_IMETHOD    SetAttributeNode(nsIDOMAttribute* aNewAttr);  \
  NS_IMETHOD    RemoveAttributeNode(nsIDOMAttribute* aOldAttr);  \
  NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn);  \
  NS_IMETHOD    Normalize();  \



#define NS_FORWARD_IDOMELEMENT(_to)  \
  NS_IMETHOD    GetTagName(nsString& aTagName) { return _to##GetTagName(aTagName); } \
  NS_IMETHOD    GetDOMAttribute(const nsString& aName, nsString& aReturn) { return _to##GetDOMAttribute(aName, aReturn); }  \
  NS_IMETHOD    SetDOMAttribute(const nsString& aName, const nsString& aValue) { return _to##SetDOMAttribute(aName, aValue); }  \
  NS_IMETHOD    RemoveAttribute(const nsString& aName) { return _to##RemoveAttribute(aName); }  \
  NS_IMETHOD    GetAttributeNode(const nsString& aName, nsIDOMAttribute** aReturn) { return _to##GetAttributeNode(aName, aReturn); }  \
  NS_IMETHOD    SetAttributeNode(nsIDOMAttribute* aNewAttr) { return _to##SetAttributeNode(aNewAttr); }  \
  NS_IMETHOD    RemoveAttributeNode(nsIDOMAttribute* aOldAttr) { return _to##RemoveAttributeNode(aOldAttr); }  \
  NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn) { return _to##GetElementsByTagName(aTagname, aReturn); }  \
  NS_IMETHOD    Normalize() { return _to##Normalize(); }  \


extern nsresult NS_InitElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMElement_h__
