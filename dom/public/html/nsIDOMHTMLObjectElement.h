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

#ifndef nsIDOMHTMLObjectElement_h__
#define nsIDOMHTMLObjectElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLFormElement;

#define NS_IDOMHTMLOBJECTELEMENT_IID \
{ 0x6f765312,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLObjectElement : public nsIDOMHTMLElement {
public:

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;
  NS_IMETHOD    SetForm(nsIDOMHTMLFormElement* aForm)=0;

  NS_IMETHOD    GetCode(nsString& aCode)=0;
  NS_IMETHOD    SetCode(const nsString& aCode)=0;

  NS_IMETHOD    GetAlign(nsString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsString& aAlign)=0;

  NS_IMETHOD    GetArchive(nsString& aArchive)=0;
  NS_IMETHOD    SetArchive(const nsString& aArchive)=0;

  NS_IMETHOD    GetBorder(nsString& aBorder)=0;
  NS_IMETHOD    SetBorder(const nsString& aBorder)=0;

  NS_IMETHOD    GetCodeBase(nsString& aCodeBase)=0;
  NS_IMETHOD    SetCodeBase(const nsString& aCodeBase)=0;

  NS_IMETHOD    GetCodeType(nsString& aCodeType)=0;
  NS_IMETHOD    SetCodeType(const nsString& aCodeType)=0;

  NS_IMETHOD    GetData(nsString& aData)=0;
  NS_IMETHOD    SetData(const nsString& aData)=0;

  NS_IMETHOD    GetDeclare(PRBool* aDeclare)=0;
  NS_IMETHOD    SetDeclare(PRBool aDeclare)=0;

  NS_IMETHOD    GetHeight(nsString& aHeight)=0;
  NS_IMETHOD    SetHeight(const nsString& aHeight)=0;

  NS_IMETHOD    GetHspace(nsString& aHspace)=0;
  NS_IMETHOD    SetHspace(const nsString& aHspace)=0;

  NS_IMETHOD    GetName(nsString& aName)=0;
  NS_IMETHOD    SetName(const nsString& aName)=0;

  NS_IMETHOD    GetStandby(nsString& aStandby)=0;
  NS_IMETHOD    SetStandby(const nsString& aStandby)=0;

  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex)=0;
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex)=0;

  NS_IMETHOD    GetType(nsString& aType)=0;
  NS_IMETHOD    SetType(const nsString& aType)=0;

  NS_IMETHOD    GetUseMap(nsString& aUseMap)=0;
  NS_IMETHOD    SetUseMap(const nsString& aUseMap)=0;

  NS_IMETHOD    GetVspace(nsString& aVspace)=0;
  NS_IMETHOD    SetVspace(const nsString& aVspace)=0;

  NS_IMETHOD    GetWidth(nsString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsString& aWidth)=0;
};


#define NS_DECL_IDOMHTMLOBJECTELEMENT   \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm);  \
  NS_IMETHOD    SetForm(nsIDOMHTMLFormElement* aForm);  \
  NS_IMETHOD    GetCode(nsString& aCode);  \
  NS_IMETHOD    SetCode(const nsString& aCode);  \
  NS_IMETHOD    GetAlign(nsString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsString& aAlign);  \
  NS_IMETHOD    GetArchive(nsString& aArchive);  \
  NS_IMETHOD    SetArchive(const nsString& aArchive);  \
  NS_IMETHOD    GetBorder(nsString& aBorder);  \
  NS_IMETHOD    SetBorder(const nsString& aBorder);  \
  NS_IMETHOD    GetCodeBase(nsString& aCodeBase);  \
  NS_IMETHOD    SetCodeBase(const nsString& aCodeBase);  \
  NS_IMETHOD    GetCodeType(nsString& aCodeType);  \
  NS_IMETHOD    SetCodeType(const nsString& aCodeType);  \
  NS_IMETHOD    GetData(nsString& aData);  \
  NS_IMETHOD    SetData(const nsString& aData);  \
  NS_IMETHOD    GetDeclare(PRBool* aDeclare);  \
  NS_IMETHOD    SetDeclare(PRBool aDeclare);  \
  NS_IMETHOD    GetHeight(nsString& aHeight);  \
  NS_IMETHOD    SetHeight(const nsString& aHeight);  \
  NS_IMETHOD    GetHspace(nsString& aHspace);  \
  NS_IMETHOD    SetHspace(const nsString& aHspace);  \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    SetName(const nsString& aName);  \
  NS_IMETHOD    GetStandby(nsString& aStandby);  \
  NS_IMETHOD    SetStandby(const nsString& aStandby);  \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex);  \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex);  \
  NS_IMETHOD    GetType(nsString& aType);  \
  NS_IMETHOD    SetType(const nsString& aType);  \
  NS_IMETHOD    GetUseMap(nsString& aUseMap);  \
  NS_IMETHOD    SetUseMap(const nsString& aUseMap);  \
  NS_IMETHOD    GetVspace(nsString& aVspace);  \
  NS_IMETHOD    SetVspace(const nsString& aVspace);  \
  NS_IMETHOD    GetWidth(nsString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsString& aWidth);  \



#define NS_FORWARD_IDOMHTMLOBJECTELEMENT(superClass)  \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm) { return superClass::GetForm(aForm); } \
  NS_IMETHOD    SetForm(nsIDOMHTMLFormElement* aForm) { return superClass::SetForm(aForm); } \
  NS_IMETHOD    GetCode(nsString& aCode) { return superClass::GetCode(aCode); } \
  NS_IMETHOD    SetCode(const nsString& aCode) { return superClass::SetCode(aCode); } \
  NS_IMETHOD    GetAlign(nsString& aAlign) { return superClass::GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsString& aAlign) { return superClass::SetAlign(aAlign); } \
  NS_IMETHOD    GetArchive(nsString& aArchive) { return superClass::GetArchive(aArchive); } \
  NS_IMETHOD    SetArchive(const nsString& aArchive) { return superClass::SetArchive(aArchive); } \
  NS_IMETHOD    GetBorder(nsString& aBorder) { return superClass::GetBorder(aBorder); } \
  NS_IMETHOD    SetBorder(const nsString& aBorder) { return superClass::SetBorder(aBorder); } \
  NS_IMETHOD    GetCodeBase(nsString& aCodeBase) { return superClass::GetCodeBase(aCodeBase); } \
  NS_IMETHOD    SetCodeBase(const nsString& aCodeBase) { return superClass::SetCodeBase(aCodeBase); } \
  NS_IMETHOD    GetCodeType(nsString& aCodeType) { return superClass::GetCodeType(aCodeType); } \
  NS_IMETHOD    SetCodeType(const nsString& aCodeType) { return superClass::SetCodeType(aCodeType); } \
  NS_IMETHOD    GetData(nsString& aData) { return superClass::GetData(aData); } \
  NS_IMETHOD    SetData(const nsString& aData) { return superClass::SetData(aData); } \
  NS_IMETHOD    GetDeclare(PRBool* aDeclare) { return superClass::GetDeclare(aDeclare); } \
  NS_IMETHOD    SetDeclare(PRBool aDeclare) { return superClass::SetDeclare(aDeclare); } \
  NS_IMETHOD    GetHeight(nsString& aHeight) { return superClass::GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(const nsString& aHeight) { return superClass::SetHeight(aHeight); } \
  NS_IMETHOD    GetHspace(nsString& aHspace) { return superClass::GetHspace(aHspace); } \
  NS_IMETHOD    SetHspace(const nsString& aHspace) { return superClass::SetHspace(aHspace); } \
  NS_IMETHOD    GetName(nsString& aName) { return superClass::GetName(aName); } \
  NS_IMETHOD    SetName(const nsString& aName) { return superClass::SetName(aName); } \
  NS_IMETHOD    GetStandby(nsString& aStandby) { return superClass::GetStandby(aStandby); } \
  NS_IMETHOD    SetStandby(const nsString& aStandby) { return superClass::SetStandby(aStandby); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return superClass::GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return superClass::SetTabIndex(aTabIndex); } \
  NS_IMETHOD    GetType(nsString& aType) { return superClass::GetType(aType); } \
  NS_IMETHOD    SetType(const nsString& aType) { return superClass::SetType(aType); } \
  NS_IMETHOD    GetUseMap(nsString& aUseMap) { return superClass::GetUseMap(aUseMap); } \
  NS_IMETHOD    SetUseMap(const nsString& aUseMap) { return superClass::SetUseMap(aUseMap); } \
  NS_IMETHOD    GetVspace(nsString& aVspace) { return superClass::GetVspace(aVspace); } \
  NS_IMETHOD    SetVspace(const nsString& aVspace) { return superClass::SetVspace(aVspace); } \
  NS_IMETHOD    GetWidth(nsString& aWidth) { return superClass::GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsString& aWidth) { return superClass::SetWidth(aWidth); } \


extern nsresult NS_InitHTMLObjectElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLObjectElement(nsIScriptContext *aContext, nsIDOMHTMLObjectElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLObjectElement_h__
