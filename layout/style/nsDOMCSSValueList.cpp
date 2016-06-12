/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing lists of values in DOM computed style */

#include "nsDOMCSSValueList.h"
#include "mozilla/dom/CSSValueListBinding.h"
#include "mozilla/Move.h"

using namespace mozilla;

nsDOMCSSValueList::nsDOMCSSValueList(bool aCommaDelimited, bool aReadonly)
  : CSSValue(), mCommaDelimited(aCommaDelimited), mReadonly(aReadonly)
{
}

nsDOMCSSValueList::~nsDOMCSSValueList()
{
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMCSSValueList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMCSSValueList)

// QueryInterface implementation for nsDOMCSSValueList
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMCSSValueList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSValueList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, CSSValue)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsDOMCSSValueList, mCSSValues)

JSObject*
nsDOMCSSValueList::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::CSSValueListBinding::Wrap(cx, this, aGivenProto);
}

void
nsDOMCSSValueList::AppendCSSValue(already_AddRefed<CSSValue> aValue)
{
  RefPtr<CSSValue> val = aValue;
  mCSSValues.AppendElement(Move(val));
}

// nsIDOMCSSValue

NS_IMETHODIMP
nsDOMCSSValueList::GetCssText(nsAString& aCssText)
{
  aCssText.Truncate();

  uint32_t count = mCSSValues.Length();

  nsAutoString separator;
  if (mCommaDelimited) {
    separator.AssignLiteral(", ");
  }
  else {
    separator.Assign(char16_t(' '));
  }

  nsAutoString tmpStr;
  for (uint32_t i = 0; i < count; ++i) {
    CSSValue *cssValue = mCSSValues[i];
    NS_ASSERTION(cssValue, "Eek!  Someone filled the value list with null CSSValues!");
    ErrorResult dummy;
    if (cssValue) {
      cssValue->GetCssText(tmpStr, dummy);

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

void
nsDOMCSSValueList::GetCssText(nsString& aText, ErrorResult& aRv)
{
  aRv = GetCssText(aText);
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

void
nsDOMCSSValueList::SetCssText(const nsAString& aText, ErrorResult& aRv)
{
  aRv = SetCssText(aText);
}

NS_IMETHODIMP
nsDOMCSSValueList::GetCssValueType(uint16_t* aValueType)
{
  NS_ENSURE_ARG_POINTER(aValueType);
  *aValueType = nsIDOMCSSValue::CSS_VALUE_LIST;
  return NS_OK;
}

uint16_t
nsDOMCSSValueList::CssValueType() const
{
  return nsIDOMCSSValue::CSS_VALUE_LIST;
}
