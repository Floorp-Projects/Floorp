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

#ifndef nsIDOMHTMLAppletElement_h__
#define nsIDOMHTMLAppletElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLAPPLETELEMENT_IID \
 { 0xa6cf90ae, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLAppletElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLAPPLETELEMENT_IID; return iid; }

  NS_IMETHOD    GetAlign(nsAWritableString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign)=0;

  NS_IMETHOD    GetAlt(nsAWritableString& aAlt)=0;
  NS_IMETHOD    SetAlt(const nsAReadableString& aAlt)=0;

  NS_IMETHOD    GetArchive(nsAWritableString& aArchive)=0;
  NS_IMETHOD    SetArchive(const nsAReadableString& aArchive)=0;

  NS_IMETHOD    GetCode(nsAWritableString& aCode)=0;
  NS_IMETHOD    SetCode(const nsAReadableString& aCode)=0;

  NS_IMETHOD    GetCodeBase(nsAWritableString& aCodeBase)=0;
  NS_IMETHOD    SetCodeBase(const nsAReadableString& aCodeBase)=0;

  NS_IMETHOD    GetHeight(nsAWritableString& aHeight)=0;
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight)=0;

  NS_IMETHOD    GetHspace(nsAWritableString& aHspace)=0;
  NS_IMETHOD    SetHspace(const nsAReadableString& aHspace)=0;

  NS_IMETHOD    GetName(nsAWritableString& aName)=0;
  NS_IMETHOD    SetName(const nsAReadableString& aName)=0;

  NS_IMETHOD    GetObject(nsAWritableString& aObject)=0;
  NS_IMETHOD    SetObject(const nsAReadableString& aObject)=0;

  NS_IMETHOD    GetVspace(nsAWritableString& aVspace)=0;
  NS_IMETHOD    SetVspace(const nsAReadableString& aVspace)=0;

  NS_IMETHOD    GetWidth(nsAWritableString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth)=0;
};


#define NS_DECL_IDOMHTMLAPPLETELEMENT   \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign);  \
  NS_IMETHOD    GetAlt(nsAWritableString& aAlt);  \
  NS_IMETHOD    SetAlt(const nsAReadableString& aAlt);  \
  NS_IMETHOD    GetArchive(nsAWritableString& aArchive);  \
  NS_IMETHOD    SetArchive(const nsAReadableString& aArchive);  \
  NS_IMETHOD    GetCode(nsAWritableString& aCode);  \
  NS_IMETHOD    SetCode(const nsAReadableString& aCode);  \
  NS_IMETHOD    GetCodeBase(nsAWritableString& aCodeBase);  \
  NS_IMETHOD    SetCodeBase(const nsAReadableString& aCodeBase);  \
  NS_IMETHOD    GetHeight(nsAWritableString& aHeight);  \
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight);  \
  NS_IMETHOD    GetHspace(nsAWritableString& aHspace);  \
  NS_IMETHOD    SetHspace(const nsAReadableString& aHspace);  \
  NS_IMETHOD    GetName(nsAWritableString& aName);  \
  NS_IMETHOD    SetName(const nsAReadableString& aName);  \
  NS_IMETHOD    GetObject(nsAWritableString& aObject);  \
  NS_IMETHOD    SetObject(const nsAReadableString& aObject);  \
  NS_IMETHOD    GetVspace(nsAWritableString& aVspace);  \
  NS_IMETHOD    SetVspace(const nsAReadableString& aVspace);  \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth);  \



#define NS_FORWARD_IDOMHTMLAPPLETELEMENT(_to)  \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetAlt(nsAWritableString& aAlt) { return _to GetAlt(aAlt); } \
  NS_IMETHOD    SetAlt(const nsAReadableString& aAlt) { return _to SetAlt(aAlt); } \
  NS_IMETHOD    GetArchive(nsAWritableString& aArchive) { return _to GetArchive(aArchive); } \
  NS_IMETHOD    SetArchive(const nsAReadableString& aArchive) { return _to SetArchive(aArchive); } \
  NS_IMETHOD    GetCode(nsAWritableString& aCode) { return _to GetCode(aCode); } \
  NS_IMETHOD    SetCode(const nsAReadableString& aCode) { return _to SetCode(aCode); } \
  NS_IMETHOD    GetCodeBase(nsAWritableString& aCodeBase) { return _to GetCodeBase(aCodeBase); } \
  NS_IMETHOD    SetCodeBase(const nsAReadableString& aCodeBase) { return _to SetCodeBase(aCodeBase); } \
  NS_IMETHOD    GetHeight(nsAWritableString& aHeight) { return _to GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight) { return _to SetHeight(aHeight); } \
  NS_IMETHOD    GetHspace(nsAWritableString& aHspace) { return _to GetHspace(aHspace); } \
  NS_IMETHOD    SetHspace(const nsAReadableString& aHspace) { return _to SetHspace(aHspace); } \
  NS_IMETHOD    GetName(nsAWritableString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsAReadableString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetObject(nsAWritableString& aObject) { return _to GetObject(aObject); } \
  NS_IMETHOD    SetObject(const nsAReadableString& aObject) { return _to SetObject(aObject); } \
  NS_IMETHOD    GetVspace(nsAWritableString& aVspace) { return _to GetVspace(aVspace); } \
  NS_IMETHOD    SetVspace(const nsAReadableString& aVspace) { return _to SetVspace(aVspace); } \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth) { return _to SetWidth(aWidth); } \


extern "C" NS_DOM nsresult NS_InitHTMLAppletElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLAppletElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLAppletElement_h__
