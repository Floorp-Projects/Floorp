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

#ifndef nsIDOMHTMLFrameElement_h__
#define nsIDOMHTMLFrameElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMDocument;

#define NS_IDOMHTMLFRAMEELEMENT_IID \
 { 0xa6cf90b9, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLFrameElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLFRAMEELEMENT_IID; return iid; }

  NS_IMETHOD    GetFrameBorder(nsAWritableString& aFrameBorder)=0;
  NS_IMETHOD    SetFrameBorder(const nsAReadableString& aFrameBorder)=0;

  NS_IMETHOD    GetLongDesc(nsAWritableString& aLongDesc)=0;
  NS_IMETHOD    SetLongDesc(const nsAReadableString& aLongDesc)=0;

  NS_IMETHOD    GetMarginHeight(nsAWritableString& aMarginHeight)=0;
  NS_IMETHOD    SetMarginHeight(const nsAReadableString& aMarginHeight)=0;

  NS_IMETHOD    GetMarginWidth(nsAWritableString& aMarginWidth)=0;
  NS_IMETHOD    SetMarginWidth(const nsAReadableString& aMarginWidth)=0;

  NS_IMETHOD    GetName(nsAWritableString& aName)=0;
  NS_IMETHOD    SetName(const nsAReadableString& aName)=0;

  NS_IMETHOD    GetNoResize(PRBool* aNoResize)=0;
  NS_IMETHOD    SetNoResize(PRBool aNoResize)=0;

  NS_IMETHOD    GetScrolling(nsAWritableString& aScrolling)=0;
  NS_IMETHOD    SetScrolling(const nsAReadableString& aScrolling)=0;

  NS_IMETHOD    GetSrc(nsAWritableString& aSrc)=0;
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc)=0;

  NS_IMETHOD    GetContentDocument(nsIDOMDocument** aContentDocument)=0;
  NS_IMETHOD    SetContentDocument(nsIDOMDocument* aContentDocument)=0;
};


#define NS_DECL_IDOMHTMLFRAMEELEMENT   \
  NS_IMETHOD    GetFrameBorder(nsAWritableString& aFrameBorder);  \
  NS_IMETHOD    SetFrameBorder(const nsAReadableString& aFrameBorder);  \
  NS_IMETHOD    GetLongDesc(nsAWritableString& aLongDesc);  \
  NS_IMETHOD    SetLongDesc(const nsAReadableString& aLongDesc);  \
  NS_IMETHOD    GetMarginHeight(nsAWritableString& aMarginHeight);  \
  NS_IMETHOD    SetMarginHeight(const nsAReadableString& aMarginHeight);  \
  NS_IMETHOD    GetMarginWidth(nsAWritableString& aMarginWidth);  \
  NS_IMETHOD    SetMarginWidth(const nsAReadableString& aMarginWidth);  \
  NS_IMETHOD    GetName(nsAWritableString& aName);  \
  NS_IMETHOD    SetName(const nsAReadableString& aName);  \
  NS_IMETHOD    GetNoResize(PRBool* aNoResize);  \
  NS_IMETHOD    SetNoResize(PRBool aNoResize);  \
  NS_IMETHOD    GetScrolling(nsAWritableString& aScrolling);  \
  NS_IMETHOD    SetScrolling(const nsAReadableString& aScrolling);  \
  NS_IMETHOD    GetSrc(nsAWritableString& aSrc);  \
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc);  \
  NS_IMETHOD    GetContentDocument(nsIDOMDocument** aContentDocument);  \
  NS_IMETHOD    SetContentDocument(nsIDOMDocument* aContentDocument);  \



#define NS_FORWARD_IDOMHTMLFRAMEELEMENT(_to)  \
  NS_IMETHOD    GetFrameBorder(nsAWritableString& aFrameBorder) { return _to GetFrameBorder(aFrameBorder); } \
  NS_IMETHOD    SetFrameBorder(const nsAReadableString& aFrameBorder) { return _to SetFrameBorder(aFrameBorder); } \
  NS_IMETHOD    GetLongDesc(nsAWritableString& aLongDesc) { return _to GetLongDesc(aLongDesc); } \
  NS_IMETHOD    SetLongDesc(const nsAReadableString& aLongDesc) { return _to SetLongDesc(aLongDesc); } \
  NS_IMETHOD    GetMarginHeight(nsAWritableString& aMarginHeight) { return _to GetMarginHeight(aMarginHeight); } \
  NS_IMETHOD    SetMarginHeight(const nsAReadableString& aMarginHeight) { return _to SetMarginHeight(aMarginHeight); } \
  NS_IMETHOD    GetMarginWidth(nsAWritableString& aMarginWidth) { return _to GetMarginWidth(aMarginWidth); } \
  NS_IMETHOD    SetMarginWidth(const nsAReadableString& aMarginWidth) { return _to SetMarginWidth(aMarginWidth); } \
  NS_IMETHOD    GetName(nsAWritableString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsAReadableString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetNoResize(PRBool* aNoResize) { return _to GetNoResize(aNoResize); } \
  NS_IMETHOD    SetNoResize(PRBool aNoResize) { return _to SetNoResize(aNoResize); } \
  NS_IMETHOD    GetScrolling(nsAWritableString& aScrolling) { return _to GetScrolling(aScrolling); } \
  NS_IMETHOD    SetScrolling(const nsAReadableString& aScrolling) { return _to SetScrolling(aScrolling); } \
  NS_IMETHOD    GetSrc(nsAWritableString& aSrc) { return _to GetSrc(aSrc); } \
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc) { return _to SetSrc(aSrc); } \
  NS_IMETHOD    GetContentDocument(nsIDOMDocument** aContentDocument) { return _to GetContentDocument(aContentDocument); } \
  NS_IMETHOD    SetContentDocument(nsIDOMDocument* aContentDocument) { return _to SetContentDocument(aContentDocument); } \


extern "C" NS_DOM nsresult NS_InitHTMLFrameElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLFrameElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLFrameElement_h__
