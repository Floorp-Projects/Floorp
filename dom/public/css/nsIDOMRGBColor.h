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

#ifndef nsIDOMRGBColor_h__
#define nsIDOMRGBColor_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMCSSPrimitiveValue;

#define NS_IDOMRGBCOLOR_IID \
 { 0x6aff3102, 0x320d, 0x4986, \
  { 0x97, 0x90, 0x12, 0x31, 0x6b, 0xb8, 0x7c, 0xf9 } } 

class NS_NO_VTABLE nsIDOMRGBColor : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMRGBCOLOR_IID)

  NS_IMETHOD    GetRed(nsIDOMCSSPrimitiveValue** aRed)=0;

  NS_IMETHOD    GetGreen(nsIDOMCSSPrimitiveValue** aGreen)=0;

  NS_IMETHOD    GetBlue(nsIDOMCSSPrimitiveValue** aBlue)=0;
};


#define NS_DECL_IDOMRGBCOLOR   \
  NS_IMETHOD    GetRed(nsIDOMCSSPrimitiveValue** aRed);  \
  NS_IMETHOD    GetGreen(nsIDOMCSSPrimitiveValue** aGreen);  \
  NS_IMETHOD    GetBlue(nsIDOMCSSPrimitiveValue** aBlue);  \



#define NS_FORWARD_IDOMRGBCOLOR(_to)  \
  NS_IMETHOD    GetRed(nsIDOMCSSPrimitiveValue** aRed) { return _to GetRed(aRed); } \
  NS_IMETHOD    GetGreen(nsIDOMCSSPrimitiveValue** aGreen) { return _to GetGreen(aGreen); } \
  NS_IMETHOD    GetBlue(nsIDOMCSSPrimitiveValue** aBlue) { return _to GetBlue(aBlue); } \


extern "C" NS_DOM nsresult NS_InitRGBColorClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptRGBColor(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMRGBColor_h__
