/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsROCSSPrimitiveValue.h"

#include "nsCOMPtr.h"
#include "nsDOMError.h"
#include "prprf.h"
#include "nsContentUtils.h"

nsROCSSPrimitiveValue::nsROCSSPrimitiveValue(nsISupports *aOwner, float aT2P)
  : mType(CSS_PX), mTwips(0), mString(), mOwner(aOwner), mT2P(aT2P)
{
  NS_INIT_REFCNT();
}


nsROCSSPrimitiveValue::~nsROCSSPrimitiveValue()
{
}


NS_IMPL_ADDREF(nsROCSSPrimitiveValue);
NS_IMPL_RELEASE(nsROCSSPrimitiveValue);


// QueryInterface implementation for nsROCSSPrimitiveValue
NS_INTERFACE_MAP_BEGIN(nsROCSSPrimitiveValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSPrimitiveValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSValue)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMCSSPrimitiveValue)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(ROCSSPrimitiveValue)
NS_INTERFACE_MAP_END


// nsIDOMCSSValue


NS_IMETHODIMP
nsROCSSPrimitiveValue::GetCssText(nsAWritableString& aCssText)
{
  nsAutoString tmpStr;

  aCssText.Truncate();

  switch (mType) {
    case CSS_PX :
      {
        PRInt32 px = NSTwipsToIntPixels(mTwips, mT2P);
        tmpStr.AppendInt(px);
        tmpStr.AppendWithConversion("px");

        break;
      }
    case CSS_CM :
      {
        float val = NS_TWIPS_TO_CENTIMETERS(mTwips);
        char buf[64];
        PR_snprintf(buf, 63, "%.2fcm", val);
        tmpStr.AppendWithConversion("cm");
        break;
      }
    case CSS_MM :
      {
        float val = NS_TWIPS_TO_MILLIMETERS(mTwips);
        char buf[64];
        PR_snprintf(buf, 63, "%.2fcm", val);
        tmpStr.AppendWithConversion("mm");
        break;
      }
    case CSS_IN :
      {
        float val = NS_TWIPS_TO_INCHES(mTwips);
        char buf[64];
        PR_snprintf(buf, 63, "%.2fcm", val);
        tmpStr.AppendWithConversion("in");
        break;
      }
    case CSS_PT :
      {
        float val = NSTwipsToFloatPoints(mTwips);
        char buf[64];
        PR_snprintf(buf, 63, "%.2fcm", val);
        tmpStr.AppendWithConversion("pt");
        break;
      }
    case CSS_STRING :
      {
        tmpStr.Append(mString);
        break;
      }
    case CSS_PC :
    case CSS_UNKNOWN :
    case CSS_NUMBER :
    case CSS_PERCENTAGE :
    case CSS_EMS :
    case CSS_EXS :
    case CSS_DEG :
    case CSS_RAD :
    case CSS_GRAD :
    case CSS_MS :
    case CSS_S :
    case CSS_HZ :
    case CSS_KHZ :
    case CSS_DIMENSION :
    case CSS_URI :
    case CSS_IDENT :
    case CSS_ATTR :
    case CSS_COUNTER :
    case CSS_RECT :
    case CSS_RGBCOLOR :
      return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  aCssText.Assign(tmpStr);

  return NS_OK;
}


NS_IMETHODIMP
nsROCSSPrimitiveValue::SetCssText(const nsAReadableString& aCssText)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsROCSSPrimitiveValue::GetCssValueType(PRUint16* aValueType)
{
  NS_ENSURE_ARG_POINTER(aValueType);
  *aValueType = nsIDOMCSSValue::CSS_PRIMITIVE_VALUE;
  return NS_OK;
}


// nsIDOMCSSPrimitiveValue

NS_IMETHODIMP
nsROCSSPrimitiveValue::GetPrimitiveType(PRUint16* aPrimitiveType)
{
  NS_ENSURE_ARG_POINTER(aPrimitiveType);
  *aPrimitiveType = mType;

  return NS_OK;
}


NS_IMETHODIMP
nsROCSSPrimitiveValue::SetFloatValue(PRUint16 aUnitType, float aFloatValue)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsROCSSPrimitiveValue::GetFloatValue(PRUint16 aUnitType, float* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = 0;

  if (mType == CSS_STRING) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  switch(aUnitType) {
    case CSS_PX :
      *aReturn = NSTwipsToFloatPixels(mTwips, mT2P);
      break;
    case CSS_CM :
      *aReturn = NS_TWIPS_TO_CENTIMETERS(mTwips);
      break;
    case CSS_MM :
      *aReturn = NS_TWIPS_TO_MILLIMETERS(mTwips);
      break;
    case CSS_IN :
      *aReturn = NS_TWIPS_TO_INCHES(mTwips);
      break;
    case CSS_PT :
      *aReturn = NSTwipsToFloatPoints(mTwips);
      break;
    case CSS_PC :
    case CSS_UNKNOWN :
    case CSS_NUMBER :
    case CSS_PERCENTAGE :
    case CSS_EMS :
    case CSS_EXS :
    case CSS_DEG :
    case CSS_RAD :
    case CSS_GRAD :
    case CSS_MS :
    case CSS_S :
    case CSS_HZ :
    case CSS_KHZ :
    case CSS_DIMENSION :
    case CSS_STRING :
    case CSS_URI :
    case CSS_IDENT :
    case CSS_ATTR :
    case CSS_COUNTER :
    case CSS_RECT :
    case CSS_RGBCOLOR :
      return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsROCSSPrimitiveValue::SetStringValue(PRUint16 aStringType,
                                      const nsAReadableString& aStringValue)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsROCSSPrimitiveValue::GetStringValue(nsAWritableString& aReturn)
{
  aReturn.Assign(mString);
  return NS_OK;
}


NS_IMETHODIMP
nsROCSSPrimitiveValue::GetCounterValue(nsIDOMCounter** aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsROCSSPrimitiveValue::GetRectValue(nsIDOMRect** aReturn)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}


NS_IMETHODIMP 
nsROCSSPrimitiveValue::GetRGBColorValue(nsIDOMRGBColor** aReturn)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

