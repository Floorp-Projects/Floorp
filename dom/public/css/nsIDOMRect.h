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

#ifndef nsIDOMRect_h__
#define nsIDOMRect_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMCSSPrimitiveValue;

#define NS_IDOMRECT_IID \
 { 0x71735f62, 0xac5c, 0x4236, \
  { 0x9a, 0x1f, 0x5f, 0xfb, 0x28, 0x0d, 0x53, 0x1c } } 

class NS_NO_VTABLE nsIDOMRect : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMRECT_IID; return iid; }

  NS_IMETHOD    GetTop(nsIDOMCSSPrimitiveValue** aTop)=0;

  NS_IMETHOD    GetRight(nsIDOMCSSPrimitiveValue** aRight)=0;

  NS_IMETHOD    GetBottom(nsIDOMCSSPrimitiveValue** aBottom)=0;

  NS_IMETHOD    GetLeft(nsIDOMCSSPrimitiveValue** aLeft)=0;
};


#define NS_DECL_IDOMRECT   \
  NS_IMETHOD    GetTop(nsIDOMCSSPrimitiveValue** aTop);  \
  NS_IMETHOD    GetRight(nsIDOMCSSPrimitiveValue** aRight);  \
  NS_IMETHOD    GetBottom(nsIDOMCSSPrimitiveValue** aBottom);  \
  NS_IMETHOD    GetLeft(nsIDOMCSSPrimitiveValue** aLeft);  \



#define NS_FORWARD_IDOMRECT(_to)  \
  NS_IMETHOD    GetTop(nsIDOMCSSPrimitiveValue** aTop) { return _to GetTop(aTop); } \
  NS_IMETHOD    GetRight(nsIDOMCSSPrimitiveValue** aRight) { return _to GetRight(aRight); } \
  NS_IMETHOD    GetBottom(nsIDOMCSSPrimitiveValue** aBottom) { return _to GetBottom(aBottom); } \
  NS_IMETHOD    GetLeft(nsIDOMCSSPrimitiveValue** aLeft) { return _to GetLeft(aLeft); } \


extern "C" NS_DOM nsresult NS_InitRectClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptRect(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMRect_h__
