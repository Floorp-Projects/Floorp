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

#ifndef nsIDOMScreen_h__
#define nsIDOMScreen_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMSCREEN_IID \
 { 0x77947960, 0xb4af, 0x11d2, \
  { 0xbd, 0x93, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4 } } 

class nsIDOMScreen : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMSCREEN_IID; return iid; }

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
  NS_IMETHOD    GetWidth(PRInt32* aWidth);  \
  NS_IMETHOD    GetHeight(PRInt32* aHeight);  \
  NS_IMETHOD    GetPixelDepth(PRInt32* aPixelDepth);  \
  NS_IMETHOD    GetColorDepth(PRInt32* aColorDepth);  \
  NS_IMETHOD    GetAvailWidth(PRInt32* aAvailWidth);  \
  NS_IMETHOD    GetAvailHeight(PRInt32* aAvailHeight);  \
  NS_IMETHOD    GetAvailLeft(PRInt32* aAvailLeft);  \
  NS_IMETHOD    GetAvailTop(PRInt32* aAvailTop);  \



#define NS_FORWARD_IDOMSCREEN(_to)  \
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
