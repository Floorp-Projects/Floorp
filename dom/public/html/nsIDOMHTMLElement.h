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

#ifndef nsIDOMHTMLElement_h__
#define nsIDOMHTMLElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMElement.h"

class nsIDOMElement;
class nsIDOMCSSStyleDeclaration;

#define NS_IDOMHTMLELEMENT_IID \
 { 0xa6cf9085, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLElement : public nsIDOMElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLELEMENT_IID; return iid; }

  NS_IMETHOD    GetId(nsString& aId)=0;
  NS_IMETHOD    SetId(const nsString& aId)=0;

  NS_IMETHOD    GetTitle(nsString& aTitle)=0;
  NS_IMETHOD    SetTitle(const nsString& aTitle)=0;

  NS_IMETHOD    GetLang(nsString& aLang)=0;
  NS_IMETHOD    SetLang(const nsString& aLang)=0;

  NS_IMETHOD    GetDir(nsString& aDir)=0;
  NS_IMETHOD    SetDir(const nsString& aDir)=0;

  NS_IMETHOD    GetClassName(nsString& aClassName)=0;
  NS_IMETHOD    SetClassName(const nsString& aClassName)=0;

  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle)=0;

  NS_IMETHOD    GetOffsetTop(PRInt32* aOffsetTop)=0;

  NS_IMETHOD    GetOffsetLeft(PRInt32* aOffsetLeft)=0;

  NS_IMETHOD    GetOffsetWidth(PRInt32* aOffsetWidth)=0;

  NS_IMETHOD    GetOffsetHeight(PRInt32* aOffsetHeight)=0;

  NS_IMETHOD    GetOffsetParent(nsIDOMElement** aOffsetParent)=0;

  NS_IMETHOD    GetInnerHTML(nsString& aInnerHTML)=0;
  NS_IMETHOD    SetInnerHTML(const nsString& aInnerHTML)=0;
};


#define NS_DECL_IDOMHTMLELEMENT   \
  NS_IMETHOD    GetId(nsString& aId);  \
  NS_IMETHOD    SetId(const nsString& aId);  \
  NS_IMETHOD    GetTitle(nsString& aTitle);  \
  NS_IMETHOD    SetTitle(const nsString& aTitle);  \
  NS_IMETHOD    GetLang(nsString& aLang);  \
  NS_IMETHOD    SetLang(const nsString& aLang);  \
  NS_IMETHOD    GetDir(nsString& aDir);  \
  NS_IMETHOD    SetDir(const nsString& aDir);  \
  NS_IMETHOD    GetClassName(nsString& aClassName);  \
  NS_IMETHOD    SetClassName(const nsString& aClassName);  \
  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle);  \
  NS_IMETHOD    GetOffsetTop(PRInt32* aOffsetTop);  \
  NS_IMETHOD    GetOffsetLeft(PRInt32* aOffsetLeft);  \
  NS_IMETHOD    GetOffsetWidth(PRInt32* aOffsetWidth);  \
  NS_IMETHOD    GetOffsetHeight(PRInt32* aOffsetHeight);  \
  NS_IMETHOD    GetOffsetParent(nsIDOMElement** aOffsetParent);  \
  NS_IMETHOD    GetInnerHTML(nsString& aInnerHTML);  \
  NS_IMETHOD    SetInnerHTML(const nsString& aInnerHTML);  \



#define NS_FORWARD_IDOMHTMLELEMENT(_to)  \
  NS_IMETHOD    GetId(nsString& aId) { return _to GetId(aId); } \
  NS_IMETHOD    SetId(const nsString& aId) { return _to SetId(aId); } \
  NS_IMETHOD    GetTitle(nsString& aTitle) { return _to GetTitle(aTitle); } \
  NS_IMETHOD    SetTitle(const nsString& aTitle) { return _to SetTitle(aTitle); } \
  NS_IMETHOD    GetLang(nsString& aLang) { return _to GetLang(aLang); } \
  NS_IMETHOD    SetLang(const nsString& aLang) { return _to SetLang(aLang); } \
  NS_IMETHOD    GetDir(nsString& aDir) { return _to GetDir(aDir); } \
  NS_IMETHOD    SetDir(const nsString& aDir) { return _to SetDir(aDir); } \
  NS_IMETHOD    GetClassName(nsString& aClassName) { return _to GetClassName(aClassName); } \
  NS_IMETHOD    SetClassName(const nsString& aClassName) { return _to SetClassName(aClassName); } \
  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle) { return _to GetStyle(aStyle); } \
  NS_IMETHOD    GetOffsetTop(PRInt32* aOffsetTop) { return _to GetOffsetTop(aOffsetTop); } \
  NS_IMETHOD    GetOffsetLeft(PRInt32* aOffsetLeft) { return _to GetOffsetLeft(aOffsetLeft); } \
  NS_IMETHOD    GetOffsetWidth(PRInt32* aOffsetWidth) { return _to GetOffsetWidth(aOffsetWidth); } \
  NS_IMETHOD    GetOffsetHeight(PRInt32* aOffsetHeight) { return _to GetOffsetHeight(aOffsetHeight); } \
  NS_IMETHOD    GetOffsetParent(nsIDOMElement** aOffsetParent) { return _to GetOffsetParent(aOffsetParent); } \
  NS_IMETHOD    GetInnerHTML(nsString& aInnerHTML) { return _to GetInnerHTML(aInnerHTML); } \
  NS_IMETHOD    SetInnerHTML(const nsString& aInnerHTML) { return _to SetInnerHTML(aInnerHTML); } \


extern "C" NS_DOM nsresult NS_InitHTMLElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLElement_h__
