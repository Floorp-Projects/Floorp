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

#ifndef nsIDOMRenderingContext_h__
#define nsIDOMRenderingContext_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMRENDERINGCONTEXT_IID \
{ 0x6f7652e0,  0xee43, 0x11d1, \
 { 0x9c, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMRenderingContext : public nsISupports {
public:

  NS_IMETHOD    GetColor(nsString& aColor)=0;
  NS_IMETHOD    SetColor(const nsString& aColor)=0;

  NS_IMETHOD    DrawLine2(PRInt32 aX0, PRInt32 aY0, PRInt32 aX1, PRInt32 aY1)=0;
};


#define NS_DECL_IDOMRENDERINGCONTEXT   \
  NS_IMETHOD    GetColor(nsString& aColor);  \
  NS_IMETHOD    SetColor(const nsString& aColor);  \
  NS_IMETHOD    DrawLine2(PRInt32 aX0, PRInt32 aY0, PRInt32 aX1, PRInt32 aY1);  \



#define NS_FORWARD_IDOMRENDERINGCONTEXT(_to)  \
  NS_IMETHOD    GetColor(nsString& aColor) { return _to##GetColor(aColor); } \
  NS_IMETHOD    SetColor(const nsString& aColor) { return _to##SetColor(aColor); } \
  NS_IMETHOD    DrawLine2(PRInt32 aX0, PRInt32 aY0, PRInt32 aX1, PRInt32 aY1) { return _to##DrawLine2(aX0, aY0, aX1, aY1); }  \


extern nsresult NS_InitRenderingContextClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_GFX nsresult NS_NewScriptRenderingContext(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMRenderingContext_h__
