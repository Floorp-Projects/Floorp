/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing lists of values in DOM computed style */

#include "nsDOMCSSValueList.h"
#include "nsCOMPtr.h"
#include "nsDOMError.h"
#include "prtypes.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"

nsDOMCSSValueList::nsDOMCSSValueList(bool aCommaDelimited, bool aReadonly)
  : mCommaDelimited(aCommaDelimited), mReadonly(aReadonly)
{
}

nsDOMCSSValueList::~nsDOMCSSValueList()
{
}

NS_IMPL_ADDREF(nsDOMCSSValueList)
NS_IMPL_RELEASE(nsDOMCSSValueList)

DOMCI_DATA(CSSValueList, nsDOMCSSValueList)

// QueryInterface implementation for nsDOMCSSValueList
NS_INTERFACE_MAP_BEGIN(nsDOMCSSValueList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSValueList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSValue)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSValueList)
NS_INTERFACE_MAP_END

void
nsDOMCSSValueList::AppendCSSValue(nsIDOMCSSValue* aValue)
{
  mCSSValues.AppendElement(aValue);
}

// nsIDOMCSSValueList

NS_IMETHODIMP
nsDOMCSSValueList::GetLength(PRUint32* aLength)
{
  *aLength = mCSSValues.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSValueList::Item(PRUint32 aIndex, nsIDOMCSSValue **aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  NS_IF_ADDREF(*aReturn = GetItemAt(aIndex));

  return NS_OK;
}

// nsIDOMCSSValue

NS_IMETHODIMP
nsDOMCSSValueList::GetCssText(nsAString& aCssText)
{
  aCssText.Truncate();

  PRUint32 count = mCSSValues.Length();

  nsAutoString separator;
  if (mCommaDelimited) {
    separator.AssignLiteral(", ");
  }
  else {
    separator.Assign(PRUnichar(' '));
  }

  nsCOMPtr<nsIDOMCSSValue> cssValue;
  nsAutoString tmpStr;
  for (PRUint32 i = 0; i < count; ++i) {
    cssValue = mCSSValues[i];
    NS_ASSERTION(cssValue, "Eek!  Someone filled the value list with null CSSValues!");
    if (cssValue) {
      cssValue->GetCssText(tmpStr);

      if (tmpStr.IsEmpty()) {

#ifdef DEBUG_caillon
        NS_ERROR("Eek!  An empty CSSValue!  Bad!");
#endif

        continue;
      }

      // If this isn't the first item in the list, then
      // it's ok to append a separator.
      if (!aCssText.IsEmpty()) {
        aCssText.Append(separator);
      }
      aCssText.Append(tmpStr);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSValueList::SetCssText(const nsAString& aCssText)
{
  if (mReadonly) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  NS_NOTYETIMPLEMENTED("Can't SetCssText yet: please write me!");
  return NS_OK;
}


NS_IMETHODIMP
nsDOMCSSValueList::GetCssValueType(PRUint16* aValueType)
{
  NS_ENSURE_ARG_POINTER(aValueType);
  *aValueType = nsIDOMCSSValue::CSS_VALUE_LIST;
  return NS_OK;
}

