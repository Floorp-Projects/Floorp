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

#ifndef nsIDOMHTMLFrameElement_h__
#define nsIDOMHTMLFrameElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLFRAMEELEMENT_IID \
 { 0xa6cf90b9, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLFrameElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLFRAMEELEMENT_IID; return iid; }

  NS_IMETHOD    GetFrameBorder(nsString& aFrameBorder)=0;
  NS_IMETHOD    SetFrameBorder(const nsString& aFrameBorder)=0;

  NS_IMETHOD    GetLongDesc(nsString& aLongDesc)=0;
  NS_IMETHOD    SetLongDesc(const nsString& aLongDesc)=0;

  NS_IMETHOD    GetMarginHeight(nsString& aMarginHeight)=0;
  NS_IMETHOD    SetMarginHeight(const nsString& aMarginHeight)=0;

  NS_IMETHOD    GetMarginWidth(nsString& aMarginWidth)=0;
  NS_IMETHOD    SetMarginWidth(const nsString& aMarginWidth)=0;

  NS_IMETHOD    GetName(nsString& aName)=0;
  NS_IMETHOD    SetName(const nsString& aName)=0;

  NS_IMETHOD    GetNoResize(PRBool* aNoResize)=0;
  NS_IMETHOD    SetNoResize(PRBool aNoResize)=0;

  NS_IMETHOD    GetScrolling(nsString& aScrolling)=0;
  NS_IMETHOD    SetScrolling(const nsString& aScrolling)=0;

  NS_IMETHOD    GetSrc(nsString& aSrc)=0;
  NS_IMETHOD    SetSrc(const nsString& aSrc)=0;
};


#define NS_DECL_IDOMHTMLFRAMEELEMENT   \
  NS_IMETHOD    GetFrameBorder(nsString& aFrameBorder);  \
  NS_IMETHOD    SetFrameBorder(const nsString& aFrameBorder);  \
  NS_IMETHOD    GetLongDesc(nsString& aLongDesc);  \
  NS_IMETHOD    SetLongDesc(const nsString& aLongDesc);  \
  NS_IMETHOD    GetMarginHeight(nsString& aMarginHeight);  \
  NS_IMETHOD    SetMarginHeight(const nsString& aMarginHeight);  \
  NS_IMETHOD    GetMarginWidth(nsString& aMarginWidth);  \
  NS_IMETHOD    SetMarginWidth(const nsString& aMarginWidth);  \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    SetName(const nsString& aName);  \
  NS_IMETHOD    GetNoResize(PRBool* aNoResize);  \
  NS_IMETHOD    SetNoResize(PRBool aNoResize);  \
  NS_IMETHOD    GetScrolling(nsString& aScrolling);  \
  NS_IMETHOD    SetScrolling(const nsString& aScrolling);  \
  NS_IMETHOD    GetSrc(nsString& aSrc);  \
  NS_IMETHOD    SetSrc(const nsString& aSrc);  \



#define NS_FORWARD_IDOMHTMLFRAMEELEMENT(_to)  \
  NS_IMETHOD    GetFrameBorder(nsString& aFrameBorder) { return _to GetFrameBorder(aFrameBorder); } \
  NS_IMETHOD    SetFrameBorder(const nsString& aFrameBorder) { return _to SetFrameBorder(aFrameBorder); } \
  NS_IMETHOD    GetLongDesc(nsString& aLongDesc) { return _to GetLongDesc(aLongDesc); } \
  NS_IMETHOD    SetLongDesc(const nsString& aLongDesc) { return _to SetLongDesc(aLongDesc); } \
  NS_IMETHOD    GetMarginHeight(nsString& aMarginHeight) { return _to GetMarginHeight(aMarginHeight); } \
  NS_IMETHOD    SetMarginHeight(const nsString& aMarginHeight) { return _to SetMarginHeight(aMarginHeight); } \
  NS_IMETHOD    GetMarginWidth(nsString& aMarginWidth) { return _to GetMarginWidth(aMarginWidth); } \
  NS_IMETHOD    SetMarginWidth(const nsString& aMarginWidth) { return _to SetMarginWidth(aMarginWidth); } \
  NS_IMETHOD    GetName(nsString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetNoResize(PRBool* aNoResize) { return _to GetNoResize(aNoResize); } \
  NS_IMETHOD    SetNoResize(PRBool aNoResize) { return _to SetNoResize(aNoResize); } \
  NS_IMETHOD    GetScrolling(nsString& aScrolling) { return _to GetScrolling(aScrolling); } \
  NS_IMETHOD    SetScrolling(const nsString& aScrolling) { return _to SetScrolling(aScrolling); } \
  NS_IMETHOD    GetSrc(nsString& aSrc) { return _to GetSrc(aSrc); } \
  NS_IMETHOD    SetSrc(const nsString& aSrc) { return _to SetSrc(aSrc); } \


extern "C" NS_DOM nsresult NS_InitHTMLFrameElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLFrameElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLFrameElement_h__
