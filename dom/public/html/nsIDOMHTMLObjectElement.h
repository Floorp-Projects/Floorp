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
 { 0xa6cf90ac, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLObjectElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLOBJECTELEMENT_IID; return iid; }

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;

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



#define NS_FORWARD_IDOMHTMLOBJECTELEMENT(_to)  \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm) { return _to GetForm(aForm); } \
  NS_IMETHOD    GetCode(nsString& aCode) { return _to GetCode(aCode); } \
  NS_IMETHOD    SetCode(const nsString& aCode) { return _to SetCode(aCode); } \
  NS_IMETHOD    GetAlign(nsString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetArchive(nsString& aArchive) { return _to GetArchive(aArchive); } \
  NS_IMETHOD    SetArchive(const nsString& aArchive) { return _to SetArchive(aArchive); } \
  NS_IMETHOD    GetBorder(nsString& aBorder) { return _to GetBorder(aBorder); } \
  NS_IMETHOD    SetBorder(const nsString& aBorder) { return _to SetBorder(aBorder); } \
  NS_IMETHOD    GetCodeBase(nsString& aCodeBase) { return _to GetCodeBase(aCodeBase); } \
  NS_IMETHOD    SetCodeBase(const nsString& aCodeBase) { return _to SetCodeBase(aCodeBase); } \
  NS_IMETHOD    GetCodeType(nsString& aCodeType) { return _to GetCodeType(aCodeType); } \
  NS_IMETHOD    SetCodeType(const nsString& aCodeType) { return _to SetCodeType(aCodeType); } \
  NS_IMETHOD    GetData(nsString& aData) { return _to GetData(aData); } \
  NS_IMETHOD    SetData(const nsString& aData) { return _to SetData(aData); } \
  NS_IMETHOD    GetDeclare(PRBool* aDeclare) { return _to GetDeclare(aDeclare); } \
  NS_IMETHOD    SetDeclare(PRBool aDeclare) { return _to SetDeclare(aDeclare); } \
  NS_IMETHOD    GetHeight(nsString& aHeight) { return _to GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(const nsString& aHeight) { return _to SetHeight(aHeight); } \
  NS_IMETHOD    GetHspace(nsString& aHspace) { return _to GetHspace(aHspace); } \
  NS_IMETHOD    SetHspace(const nsString& aHspace) { return _to SetHspace(aHspace); } \
  NS_IMETHOD    GetName(nsString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetStandby(nsString& aStandby) { return _to GetStandby(aStandby); } \
  NS_IMETHOD    SetStandby(const nsString& aStandby) { return _to SetStandby(aStandby); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return _to GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return _to SetTabIndex(aTabIndex); } \
  NS_IMETHOD    GetType(nsString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    SetType(const nsString& aType) { return _to SetType(aType); } \
  NS_IMETHOD    GetUseMap(nsString& aUseMap) { return _to GetUseMap(aUseMap); } \
  NS_IMETHOD    SetUseMap(const nsString& aUseMap) { return _to SetUseMap(aUseMap); } \
  NS_IMETHOD    GetVspace(nsString& aVspace) { return _to GetVspace(aVspace); } \
  NS_IMETHOD    SetVspace(const nsString& aVspace) { return _to SetVspace(aVspace); } \
  NS_IMETHOD    GetWidth(nsString& aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsString& aWidth) { return _to SetWidth(aWidth); } \


extern "C" NS_DOM nsresult NS_InitHTMLObjectElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLObjectElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLObjectElement_h__
