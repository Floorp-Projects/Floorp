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

#ifndef nsIDOMHTMLObjectElement_h__
#define nsIDOMHTMLObjectElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLFormElement;
class nsIDOMDocument;

#define NS_IDOMHTMLOBJECTELEMENT_IID \
 { 0xa6cf90ac, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLObjectElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLOBJECTELEMENT_IID; return iid; }

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;

  NS_IMETHOD    GetCode(nsAWritableString& aCode)=0;
  NS_IMETHOD    SetCode(const nsAReadableString& aCode)=0;

  NS_IMETHOD    GetAlign(nsAWritableString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign)=0;

  NS_IMETHOD    GetArchive(nsAWritableString& aArchive)=0;
  NS_IMETHOD    SetArchive(const nsAReadableString& aArchive)=0;

  NS_IMETHOD    GetBorder(nsAWritableString& aBorder)=0;
  NS_IMETHOD    SetBorder(const nsAReadableString& aBorder)=0;

  NS_IMETHOD    GetCodeBase(nsAWritableString& aCodeBase)=0;
  NS_IMETHOD    SetCodeBase(const nsAReadableString& aCodeBase)=0;

  NS_IMETHOD    GetCodeType(nsAWritableString& aCodeType)=0;
  NS_IMETHOD    SetCodeType(const nsAReadableString& aCodeType)=0;

  NS_IMETHOD    GetData(nsAWritableString& aData)=0;
  NS_IMETHOD    SetData(const nsAReadableString& aData)=0;

  NS_IMETHOD    GetDeclare(PRBool* aDeclare)=0;
  NS_IMETHOD    SetDeclare(PRBool aDeclare)=0;

  NS_IMETHOD    GetHeight(nsAWritableString& aHeight)=0;
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight)=0;

  NS_IMETHOD    GetHspace(nsAWritableString& aHspace)=0;
  NS_IMETHOD    SetHspace(const nsAReadableString& aHspace)=0;

  NS_IMETHOD    GetName(nsAWritableString& aName)=0;
  NS_IMETHOD    SetName(const nsAReadableString& aName)=0;

  NS_IMETHOD    GetStandby(nsAWritableString& aStandby)=0;
  NS_IMETHOD    SetStandby(const nsAReadableString& aStandby)=0;

  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex)=0;
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex)=0;

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;
  NS_IMETHOD    SetType(const nsAReadableString& aType)=0;

  NS_IMETHOD    GetUseMap(nsAWritableString& aUseMap)=0;
  NS_IMETHOD    SetUseMap(const nsAReadableString& aUseMap)=0;

  NS_IMETHOD    GetVspace(nsAWritableString& aVspace)=0;
  NS_IMETHOD    SetVspace(const nsAReadableString& aVspace)=0;

  NS_IMETHOD    GetWidth(nsAWritableString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth)=0;

  NS_IMETHOD    GetContentDocument(nsIDOMDocument** aContentDocument)=0;
  NS_IMETHOD    SetContentDocument(nsIDOMDocument* aContentDocument)=0;
};


#define NS_DECL_IDOMHTMLOBJECTELEMENT   \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm);  \
  NS_IMETHOD    GetCode(nsAWritableString& aCode);  \
  NS_IMETHOD    SetCode(const nsAReadableString& aCode);  \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign);  \
  NS_IMETHOD    GetArchive(nsAWritableString& aArchive);  \
  NS_IMETHOD    SetArchive(const nsAReadableString& aArchive);  \
  NS_IMETHOD    GetBorder(nsAWritableString& aBorder);  \
  NS_IMETHOD    SetBorder(const nsAReadableString& aBorder);  \
  NS_IMETHOD    GetCodeBase(nsAWritableString& aCodeBase);  \
  NS_IMETHOD    SetCodeBase(const nsAReadableString& aCodeBase);  \
  NS_IMETHOD    GetCodeType(nsAWritableString& aCodeType);  \
  NS_IMETHOD    SetCodeType(const nsAReadableString& aCodeType);  \
  NS_IMETHOD    GetData(nsAWritableString& aData);  \
  NS_IMETHOD    SetData(const nsAReadableString& aData);  \
  NS_IMETHOD    GetDeclare(PRBool* aDeclare);  \
  NS_IMETHOD    SetDeclare(PRBool aDeclare);  \
  NS_IMETHOD    GetHeight(nsAWritableString& aHeight);  \
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight);  \
  NS_IMETHOD    GetHspace(nsAWritableString& aHspace);  \
  NS_IMETHOD    SetHspace(const nsAReadableString& aHspace);  \
  NS_IMETHOD    GetName(nsAWritableString& aName);  \
  NS_IMETHOD    SetName(const nsAReadableString& aName);  \
  NS_IMETHOD    GetStandby(nsAWritableString& aStandby);  \
  NS_IMETHOD    SetStandby(const nsAReadableString& aStandby);  \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex);  \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex);  \
  NS_IMETHOD    GetType(nsAWritableString& aType);  \
  NS_IMETHOD    SetType(const nsAReadableString& aType);  \
  NS_IMETHOD    GetUseMap(nsAWritableString& aUseMap);  \
  NS_IMETHOD    SetUseMap(const nsAReadableString& aUseMap);  \
  NS_IMETHOD    GetVspace(nsAWritableString& aVspace);  \
  NS_IMETHOD    SetVspace(const nsAReadableString& aVspace);  \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth);  \
  NS_IMETHOD    GetContentDocument(nsIDOMDocument** aContentDocument);  \
  NS_IMETHOD    SetContentDocument(nsIDOMDocument* aContentDocument);  \



#define NS_FORWARD_IDOMHTMLOBJECTELEMENT(_to)  \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm) { return _to GetForm(aForm); } \
  NS_IMETHOD    GetCode(nsAWritableString& aCode) { return _to GetCode(aCode); } \
  NS_IMETHOD    SetCode(const nsAReadableString& aCode) { return _to SetCode(aCode); } \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetArchive(nsAWritableString& aArchive) { return _to GetArchive(aArchive); } \
  NS_IMETHOD    SetArchive(const nsAReadableString& aArchive) { return _to SetArchive(aArchive); } \
  NS_IMETHOD    GetBorder(nsAWritableString& aBorder) { return _to GetBorder(aBorder); } \
  NS_IMETHOD    SetBorder(const nsAReadableString& aBorder) { return _to SetBorder(aBorder); } \
  NS_IMETHOD    GetCodeBase(nsAWritableString& aCodeBase) { return _to GetCodeBase(aCodeBase); } \
  NS_IMETHOD    SetCodeBase(const nsAReadableString& aCodeBase) { return _to SetCodeBase(aCodeBase); } \
  NS_IMETHOD    GetCodeType(nsAWritableString& aCodeType) { return _to GetCodeType(aCodeType); } \
  NS_IMETHOD    SetCodeType(const nsAReadableString& aCodeType) { return _to SetCodeType(aCodeType); } \
  NS_IMETHOD    GetData(nsAWritableString& aData) { return _to GetData(aData); } \
  NS_IMETHOD    SetData(const nsAReadableString& aData) { return _to SetData(aData); } \
  NS_IMETHOD    GetDeclare(PRBool* aDeclare) { return _to GetDeclare(aDeclare); } \
  NS_IMETHOD    SetDeclare(PRBool aDeclare) { return _to SetDeclare(aDeclare); } \
  NS_IMETHOD    GetHeight(nsAWritableString& aHeight) { return _to GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight) { return _to SetHeight(aHeight); } \
  NS_IMETHOD    GetHspace(nsAWritableString& aHspace) { return _to GetHspace(aHspace); } \
  NS_IMETHOD    SetHspace(const nsAReadableString& aHspace) { return _to SetHspace(aHspace); } \
  NS_IMETHOD    GetName(nsAWritableString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsAReadableString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetStandby(nsAWritableString& aStandby) { return _to GetStandby(aStandby); } \
  NS_IMETHOD    SetStandby(const nsAReadableString& aStandby) { return _to SetStandby(aStandby); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return _to GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return _to SetTabIndex(aTabIndex); } \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    SetType(const nsAReadableString& aType) { return _to SetType(aType); } \
  NS_IMETHOD    GetUseMap(nsAWritableString& aUseMap) { return _to GetUseMap(aUseMap); } \
  NS_IMETHOD    SetUseMap(const nsAReadableString& aUseMap) { return _to SetUseMap(aUseMap); } \
  NS_IMETHOD    GetVspace(nsAWritableString& aVspace) { return _to GetVspace(aVspace); } \
  NS_IMETHOD    SetVspace(const nsAReadableString& aVspace) { return _to SetVspace(aVspace); } \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth) { return _to SetWidth(aWidth); } \
  NS_IMETHOD    GetContentDocument(nsIDOMDocument** aContentDocument) { return _to GetContentDocument(aContentDocument); } \
  NS_IMETHOD    SetContentDocument(nsIDOMDocument* aContentDocument) { return _to SetContentDocument(aContentDocument); } \


extern "C" NS_DOM nsresult NS_InitHTMLObjectElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLObjectElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLObjectElement_h__
