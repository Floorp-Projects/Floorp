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

#ifndef nsIDOMScreen_h__
#define nsIDOMScreen_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMSCREEN_IID \
 { 0x77947960, 0xb4af, 0x11d2, \
  { 0xbd, 0x93, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4 } } 

class NS_NO_VTABLE nsIDOMScreen : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMSCREEN_IID)

  NS_IMETHOD    GetTop(PRInt32* aTop)=0;

  NS_IMETHOD    GetLeft(PRInt32* aLeft)=0;

  NS_IMETHOD    GetWidth(PRInt32* aWidth)=0;

  NS_IMETHOD    GetHeight(PRInt32* aHeight)=0;

  NS_IMETHOD    GetPixelDepth(PRInt32* aPixelDepth)=0;

  NS_IMETHOD    GetColorDepth(PRInt32* aColorDepth)=0;

  NS_IMETHOD    GetAvailWidth(PRInt32* aAvailWidth)=0;

  NS_IMETHOD    GetAvailHeight(PRInt32* aAvailHeight)=0;

  NS_IMETHOD    GetAvailLeft(PRInt32* aAvailLeft)=0;

  NS_IMETHOD    GetAvailTop(PRInt32* aAvailTop)=0;
};


#define NS_DECL_IDOMSCREEN   \
  NS_IMETHOD    GetTop(PRInt32* aTop);  \
  NS_IMETHOD    GetLeft(PRInt32* aLeft);  \
  NS_IMETHOD    GetWidth(PRInt32* aWidth);  \
  NS_IMETHOD    GetHeight(PRInt32* aHeight);  \
  NS_IMETHOD    GetPixelDepth(PRInt32* aPixelDepth);  \
  NS_IMETHOD    GetColorDepth(PRInt32* aColorDepth);  \
  NS_IMETHOD    GetAvailWidth(PRInt32* aAvailWidth);  \
  NS_IMETHOD    GetAvailHeight(PRInt32* aAvailHeight);  \
  NS_IMETHOD    GetAvailLeft(PRInt32* aAvailLeft);  \
  NS_IMETHOD    GetAvailTop(PRInt32* aAvailTop);  \



#define NS_FORWARD_IDOMSCREEN(_to)  \
  NS_IMETHOD    GetTop(PRInt32* aTop) { return _to GetTop(aTop); } \
  NS_IMETHOD    GetLeft(PRInt32* aLeft) { return _to GetLeft(aLeft); } \
  NS_IMETHOD    GetWidth(PRInt32* aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    GetHeight(PRInt32* aHeight) { return _to GetHeight(aHeight); } \
  NS_IMETHOD    GetPixelDepth(PRInt32* aPixelDepth) { return _to GetPixelDepth(aPixelDepth); } \
  NS_IMETHOD    GetColorDepth(PRInt32* aColorDepth) { return _to GetColorDepth(aColorDepth); } \
  NS_IMETHOD    GetAvailWidth(PRInt32* aAvailWidth) { return _to GetAvailWidth(aAvailWidth); } \
  NS_IMETHOD    GetAvailHeight(PRInt32* aAvailHeight) { return _to GetAvailHeight(aAvailHeight); } \
  NS_IMETHOD    GetAvailLeft(PRInt32* aAvailLeft) { return _to GetAvailLeft(aAvailLeft); } \
  NS_IMETHOD    GetAvailTop(PRInt32* aAvailTop) { return _to GetAvailTop(aAvailTop); } \


extern "C" NS_DOM nsresult NS_InitScreenClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptScreen(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMScreen_h__
