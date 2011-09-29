/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher A. Aillon <christopher@aillon.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* DOM object representing lists of values in DOM computed style */

#include "nsDOMCSSValueList.h"
#include "nsCOMPtr.h"
#include "nsDOMError.h"
#include "prtypes.h"
#include "nsContentUtils.h"

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

