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

#ifndef nsIDOMHTMLAppletElement_h__
#define nsIDOMHTMLAppletElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLAPPLETELEMENT_IID \
{ 0x6f7652ef,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLAppletElement : public nsIDOMHTMLElement {
public:

  NS_IMETHOD    GetAlign(nsString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsString& aAlign)=0;

  NS_IMETHOD    GetAlt(nsString& aAlt)=0;
  NS_IMETHOD    SetAlt(const nsString& aAlt)=0;

  NS_IMETHOD    GetArchive(nsString& aArchive)=0;
  NS_IMETHOD    SetArchive(const nsString& aArchive)=0;

  NS_IMETHOD    GetCode(nsString& aCode)=0;
  NS_IMETHOD    SetCode(const nsString& aCode)=0;

  NS_IMETHOD    GetCodeBase(nsString& aCodeBase)=0;
  NS_IMETHOD    SetCodeBase(const nsString& aCodeBase)=0;

  NS_IMETHOD    GetHeight(nsString& aHeight)=0;
  NS_IMETHOD    SetHeight(const nsString& aHeight)=0;

  NS_IMETHOD    GetHspace(nsString& aHspace)=0;
  NS_IMETHOD    SetHspace(const nsString& aHspace)=0;

  NS_IMETHOD    GetName(nsString& aName)=0;
  NS_IMETHOD    SetName(const nsString& aName)=0;

  NS_IMETHOD    GetObject(nsString& aObject)=0;
  NS_IMETHOD    SetObject(const nsString& aObject)=0;

  NS_IMETHOD    GetVspace(nsString& aVspace)=0;
  NS_IMETHOD    SetVspace(const nsString& aVspace)=0;

  NS_IMETHOD    GetWidth(nsString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsString& aWidth)=0;
};


#define NS_DECL_IDOMHTMLAPPLETELEMENT   \
  NS_IMETHOD    GetAlign(nsString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsString& aAlign);  \
  NS_IMETHOD    GetAlt(nsString& aAlt);  \
  NS_IMETHOD    SetAlt(const nsString& aAlt);  \
  NS_IMETHOD    GetArchive(nsString& aArchive);  \
  NS_IMETHOD    SetArchive(const nsString& aArchive);  \
  NS_IMETHOD    GetCode(nsString& aCode);  \
  NS_IMETHOD    SetCode(const nsString& aCode);  \
  NS_IMETHOD    GetCodeBase(nsString& aCodeBase);  \
  NS_IMETHOD    SetCodeBase(const nsString& aCodeBase);  \
  NS_IMETHOD    GetHeight(nsString& aHeight);  \
  NS_IMETHOD    SetHeight(const nsString& aHeight);  \
  NS_IMETHOD    GetHspace(nsString& aHspace);  \
  NS_IMETHOD    SetHspace(const nsString& aHspace);  \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    SetName(const nsString& aName);  \
  NS_IMETHOD    GetObject(nsString& aObject);  \
  NS_IMETHOD    SetObject(const nsString& aObject);  \
  NS_IMETHOD    GetVspace(nsString& aVspace);  \
  NS_IMETHOD    SetVspace(const nsString& aVspace);  \
  NS_IMETHOD    GetWidth(nsString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsString& aWidth);  \



#define NS_FORWARD_IDOMHTMLAPPLETELEMENT(superClass)  \
  NS_IMETHOD    GetAlign(nsString& aAlign) { return superClass::GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsString& aAlign) { return superClass::SetAlign(aAlign); } \
  NS_IMETHOD    GetAlt(nsString& aAlt) { return superClass::GetAlt(aAlt); } \
  NS_IMETHOD    SetAlt(const nsString& aAlt) { return superClass::SetAlt(aAlt); } \
  NS_IMETHOD    GetArchive(nsString& aArchive) { return superClass::GetArchive(aArchive); } \
  NS_IMETHOD    SetArchive(const nsString& aArchive) { return superClass::SetArchive(aArchive); } \
  NS_IMETHOD    GetCode(nsString& aCode) { return superClass::GetCode(aCode); } \
  NS_IMETHOD    SetCode(const nsString& aCode) { return superClass::SetCode(aCode); } \
  NS_IMETHOD    GetCodeBase(nsString& aCodeBase) { return superClass::GetCodeBase(aCodeBase); } \
  NS_IMETHOD    SetCodeBase(const nsString& aCodeBase) { return superClass::SetCodeBase(aCodeBase); } \
  NS_IMETHOD    GetHeight(nsString& aHeight) { return superClass::GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(const nsString& aHeight) { return superClass::SetHeight(aHeight); } \
  NS_IMETHOD    GetHspace(nsString& aHspace) { return superClass::GetHspace(aHspace); } \
  NS_IMETHOD    SetHspace(const nsString& aHspace) { return superClass::SetHspace(aHspace); } \
  NS_IMETHOD    GetName(nsString& aName) { return superClass::GetName(aName); } \
  NS_IMETHOD    SetName(const nsString& aName) { return superClass::SetName(aName); } \
  NS_IMETHOD    GetObject(nsString& aObject) { return superClass::GetObject(aObject); } \
  NS_IMETHOD    SetObject(const nsString& aObject) { return superClass::SetObject(aObject); } \
  NS_IMETHOD    GetVspace(nsString& aVspace) { return superClass::GetVspace(aVspace); } \
  NS_IMETHOD    SetVspace(const nsString& aVspace) { return superClass::SetVspace(aVspace); } \
  NS_IMETHOD    GetWidth(nsString& aWidth) { return superClass::GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsString& aWidth) { return superClass::SetWidth(aWidth); } \


extern nsresult NS_InitHTMLAppletElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLAppletElement(nsIScriptContext *aContext, nsIDOMHTMLAppletElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLAppletElement_h__
