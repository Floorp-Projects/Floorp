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

#ifndef nsIDOMHTMLLayerElement_h__
#define nsIDOMHTMLLayerElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMDocument;

#define NS_IDOMHTMLLAYERELEMENT_IID \
 { 0xa6cf9100, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLLayerElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLLAYERELEMENT_IID; return iid; }

  NS_IMETHOD    GetTop(PRInt32* aTop)=0;
  NS_IMETHOD    SetTop(PRInt32 aTop)=0;

  NS_IMETHOD    GetLeft(PRInt32* aLeft)=0;
  NS_IMETHOD    SetLeft(PRInt32 aLeft)=0;

  NS_IMETHOD    GetVisibility(nsString& aVisibility)=0;
  NS_IMETHOD    SetVisibility(const nsString& aVisibility)=0;

  NS_IMETHOD    GetBackground(nsString& aBackground)=0;
  NS_IMETHOD    SetBackground(const nsString& aBackground)=0;

  NS_IMETHOD    GetBgColor(nsString& aBgColor)=0;
  NS_IMETHOD    SetBgColor(const nsString& aBgColor)=0;

  NS_IMETHOD    GetName(nsString& aName)=0;
  NS_IMETHOD    SetName(const nsString& aName)=0;

  NS_IMETHOD    GetZIndex(PRInt32* aZIndex)=0;
  NS_IMETHOD    SetZIndex(PRInt32 aZIndex)=0;

  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument)=0;
};


#define NS_DECL_IDOMHTMLLAYERELEMENT   \
  NS_IMETHOD    GetTop(PRInt32* aTop);  \
  NS_IMETHOD    SetTop(PRInt32 aTop);  \
  NS_IMETHOD    GetLeft(PRInt32* aLeft);  \
  NS_IMETHOD    SetLeft(PRInt32 aLeft);  \
  NS_IMETHOD    GetVisibility(nsString& aVisibility);  \
  NS_IMETHOD    SetVisibility(const nsString& aVisibility);  \
  NS_IMETHOD    GetBackground(nsString& aBackground);  \
  NS_IMETHOD    SetBackground(const nsString& aBackground);  \
  NS_IMETHOD    GetBgColor(nsString& aBgColor);  \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor);  \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    SetName(const nsString& aName);  \
  NS_IMETHOD    GetZIndex(PRInt32* aZIndex);  \
  NS_IMETHOD    SetZIndex(PRInt32 aZIndex);  \
  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument);  \



#define NS_FORWARD_IDOMHTMLLAYERELEMENT(_to)  \
  NS_IMETHOD    GetTop(PRInt32* aTop) { return _to GetTop(aTop); } \
  NS_IMETHOD    SetTop(PRInt32 aTop) { return _to SetTop(aTop); } \
  NS_IMETHOD    GetLeft(PRInt32* aLeft) { return _to GetLeft(aLeft); } \
  NS_IMETHOD    SetLeft(PRInt32 aLeft) { return _to SetLeft(aLeft); } \
  NS_IMETHOD    GetVisibility(nsString& aVisibility) { return _to GetVisibility(aVisibility); } \
  NS_IMETHOD    SetVisibility(const nsString& aVisibility) { return _to SetVisibility(aVisibility); } \
  NS_IMETHOD    GetBackground(nsString& aBackground) { return _to GetBackground(aBackground); } \
  NS_IMETHOD    SetBackground(const nsString& aBackground) { return _to SetBackground(aBackground); } \
  NS_IMETHOD    GetBgColor(nsString& aBgColor) { return _to GetBgColor(aBgColor); } \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor) { return _to SetBgColor(aBgColor); } \
  NS_IMETHOD    GetName(nsString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetZIndex(PRInt32* aZIndex) { return _to GetZIndex(aZIndex); } \
  NS_IMETHOD    SetZIndex(PRInt32 aZIndex) { return _to SetZIndex(aZIndex); } \
  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument) { return _to GetDocument(aDocument); } \


extern "C" NS_DOM nsresult NS_InitHTMLLayerElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLLayerElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLLayerElement_h__
