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

#ifndef nsIDOMCSSPrimitiveValue_h__
#define nsIDOMCSSPrimitiveValue_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMCSSValue.h"

class nsIDOMRGBColor;
class nsIDOMCounter;
class nsIDOMRect;

#define NS_IDOMCSSPRIMITIVEVALUE_IID \
 { 0xe249031f, 0x8df9, 0x4e7a, \
  { 0xb6, 0x44, 0x18, 0x94, 0x6d, 0xce, 0x00, 0x19 } } 

class nsIDOMCSSPrimitiveValue : public nsIDOMCSSValue {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCSSPRIMITIVEVALUE_IID; return iid; }
  enum {
    CSS_UNKNOWN = 0,
    CSS_NUMBER = 1,
    CSS_PERCENTAGE = 2,
    CSS_EMS = 3,
    CSS_EXS = 4,
    CSS_PX = 5,
    CSS_CM = 6,
    CSS_MM = 7,
    CSS_IN = 8,
    CSS_PT = 9,
    CSS_PC = 10,
    CSS_DEG = 11,
    CSS_RAD = 12,
    CSS_GRAD = 13,
    CSS_MS = 14,
    CSS_S = 15,
    CSS_HZ = 16,
    CSS_KHZ = 17,
    CSS_DIMENSION = 18,
    CSS_STRING = 19,
    CSS_URI = 20,
    CSS_IDENT = 21,
    CSS_ATTR = 22,
    CSS_COUNTER = 23,
    CSS_RECT = 24,
    CSS_RGBCOLOR = 25
  };

  NS_IMETHOD    GetPrimitiveType(PRUint16* aPrimitiveType)=0;

  NS_IMETHOD    SetFloatValue(PRUint16 aUnitType, float aFloatValue)=0;

  NS_IMETHOD    GetFloatValue(PRUint16 aUnitType, float* aReturn)=0;

  NS_IMETHOD    SetStringValue(PRUint16 aStringType, const nsAReadableString& aStringValue)=0;

  NS_IMETHOD    GetStringValue(nsAWritableString& aReturn)=0;

  NS_IMETHOD    GetCounterValue(nsIDOMCounter** aReturn)=0;

  NS_IMETHOD    GetRectValue(nsIDOMRect** aReturn)=0;

  NS_IMETHOD    GetRGBColorValue(nsIDOMRGBColor** aReturn)=0;
};


#define NS_DECL_IDOMCSSPRIMITIVEVALUE   \
  NS_IMETHOD    GetPrimitiveType(PRUint16* aPrimitiveType);  \
  NS_IMETHOD    SetFloatValue(PRUint16 aUnitType, float aFloatValue);  \
  NS_IMETHOD    GetFloatValue(PRUint16 aUnitType, float* aReturn);  \
  NS_IMETHOD    SetStringValue(PRUint16 aStringType, const nsAReadableString& aStringValue);  \
  NS_IMETHOD    GetStringValue(nsAWritableString& aReturn);  \
  NS_IMETHOD    GetCounterValue(nsIDOMCounter** aReturn);  \
  NS_IMETHOD    GetRectValue(nsIDOMRect** aReturn);  \
  NS_IMETHOD    GetRGBColorValue(nsIDOMRGBColor** aReturn);  \



#define NS_FORWARD_IDOMCSSPRIMITIVEVALUE(_to)  \
  NS_IMETHOD    GetPrimitiveType(PRUint16* aPrimitiveType) { return _to GetPrimitiveType(aPrimitiveType); } \
  NS_IMETHOD    SetFloatValue(PRUint16 aUnitType, float aFloatValue) { return _to SetFloatValue(aUnitType, aFloatValue); }  \
  NS_IMETHOD    GetFloatValue(PRUint16 aUnitType, float* aReturn) { return _to GetFloatValue(aUnitType, aReturn); }  \
  NS_IMETHOD    SetStringValue(PRUint16 aStringType, const nsAReadableString& aStringValue) { return _to SetStringValue(aStringType, aStringValue); }  \
  NS_IMETHOD    GetStringValue(nsAWritableString& aReturn) { return _to GetStringValue(aReturn); }  \
  NS_IMETHOD    GetCounterValue(nsIDOMCounter** aReturn) { return _to GetCounterValue(aReturn); }  \
  NS_IMETHOD    GetRectValue(nsIDOMRect** aReturn) { return _to GetRectValue(aReturn); }  \
  NS_IMETHOD    GetRGBColorValue(nsIDOMRGBColor** aReturn) { return _to GetRGBColorValue(aReturn); }  \


extern "C" NS_DOM nsresult NS_InitCSSPrimitiveValueClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCSSPrimitiveValue(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCSSPrimitiveValue_h__
