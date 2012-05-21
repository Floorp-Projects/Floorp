/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of nsIDOMDOMTokenList specified by HTML5.
 */

#include "nsDOMTokenList.h"

#include "nsAttrValue.h"
#include "nsContentUtils.h"
#include "nsDOMError.h"
#include "nsGenericElement.h"
#include "dombindings.h"


nsDOMTokenList::nsDOMTokenList(nsGenericElement *aElement, nsIAtom* aAttrAtom)
  : mElement(aElement),
    mAttrAtom(aAttrAtom)
{
  // We don't add a reference to our element. If it goes away,
  // we'll be told to drop our reference
  SetIsDOMBinding();
}

nsDOMTokenList::~nsDOMTokenList() { }

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsDOMTokenList)

DOMCI_DATA(DOMTokenList, nsDOMTokenList)

NS_INTERFACE_TABLE_HEAD(nsDOMTokenList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_TABLE1(nsDOMTokenList,
                      nsIDOMDOMTokenList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsDOMTokenList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMTokenList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMTokenList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMTokenList)

void
nsDOMTokenList::DropReference()
{
  mElement = nsnull;
}

NS_IMETHODIMP
nsDOMTokenList::GetLength(PRUint32 *aLength)
{
  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    *aLength = 0;
    return NS_OK;
  }

  *aLength = attr->GetAtomCount();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMTokenList::Item(PRUint32 aIndex, nsAString& aResult)
{
  const nsAttrValue* attr = GetParsedAttr();

  if (!attr || aIndex >= static_cast<PRUint32>(attr->GetAtomCount())) {
    SetDOMStringToNull(aResult);
    return NS_OK;
  }
  attr->AtomAt(aIndex)->ToString(aResult);

  return NS_OK;
}

nsresult
nsDOMTokenList::CheckToken(const nsAString& aStr)
{
  if (aStr.IsEmpty()) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  nsAString::const_iterator iter, end;
  aStr.BeginReading(iter);
  aStr.EndReading(end);

  while (iter != end) {
    if (nsContentUtils::IsHTMLWhitespace(*iter))
      return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
    ++iter;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMTokenList::Contains(const nsAString& aToken, bool* aResult)
{
  nsresult rv = CheckToken(aToken);
  NS_ENSURE_SUCCESS(rv, rv);

  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    *aResult = false;
    return NS_OK;
  }

  *aResult = attr->Contains(aToken);

  return NS_OK;
}

void
nsDOMTokenList::AddInternal(const nsAttrValue* aAttr,
                            const nsAString& aToken)
{
  if (!mElement) {
    return;
  }

  nsAutoString resultStr;

  if (aAttr) {
    aAttr->ToString(resultStr);
  }

  if (!resultStr.IsEmpty() &&
      !nsContentUtils::IsHTMLWhitespace(
          resultStr.CharAt(resultStr.Length() - 1))) {
    resultStr.Append(NS_LITERAL_STRING(" ") + aToken);
  } else {
    resultStr.Append(aToken);
  }
  mElement->SetAttr(kNameSpaceID_None, mAttrAtom, resultStr, true);
}

NS_IMETHODIMP
nsDOMTokenList::Add(const nsAString& aToken)
{
  nsresult rv = CheckToken(aToken);
  NS_ENSURE_SUCCESS(rv, rv);

  const nsAttrValue* attr = GetParsedAttr();

  if (attr && attr->Contains(aToken)) {
    return NS_OK;
  }

  AddInternal(attr, aToken);

  return NS_OK;
}

void
nsDOMTokenList::RemoveInternal(const nsAttrValue* aAttr,
                               const nsAString& aToken)
{
  NS_ABORT_IF_FALSE(aAttr, "Need an attribute");

  nsAutoString input;
  aAttr->ToString(input);

  nsAString::const_iterator copyStart, tokenStart, iter, end;
  input.BeginReading(iter);
  input.EndReading(end);
  copyStart = iter;

  nsAutoString output;
  bool lastTokenRemoved = false;

  while (iter != end) {
    // skip whitespace.
    while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
      ++iter;
    }

    if (iter == end) {
      // At this point we're sure the last seen token (if any) wasn't to be
      // removed. So the trailing spaces will need to be kept.
      NS_ABORT_IF_FALSE(!lastTokenRemoved, "How did this happen?");

      output.Append(Substring(copyStart, end));
      break;
    }

    tokenStart = iter;
    do {
      ++iter;
    } while (iter != end && !nsContentUtils::IsHTMLWhitespace(*iter));

    if (Substring(tokenStart, iter).Equals(aToken)) {

      // Skip whitespace after the token, it will be collapsed.
      while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
        ++iter;
      }
      copyStart = iter;
      lastTokenRemoved = true;

    } else {

      if (lastTokenRemoved && !output.IsEmpty()) {
        NS_ABORT_IF_FALSE(!nsContentUtils::IsHTMLWhitespace(
          output.CharAt(output.Length() - 1)), "Invalid last output token");
        output.Append(PRUnichar(' '));
      }
      lastTokenRemoved = false;
      output.Append(Substring(copyStart, iter));
      copyStart = iter;
    }
  }

  mElement->SetAttr(kNameSpaceID_None, mAttrAtom, output, true);
}

NS_IMETHODIMP
nsDOMTokenList::Remove(const nsAString& aToken)
{
  nsresult rv = CheckToken(aToken);
  NS_ENSURE_SUCCESS(rv, rv);

  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    return NS_OK;
  }

  if (!attr->Contains(aToken)) {
    return NS_OK;
  }

  RemoveInternal(attr, aToken);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMTokenList::Toggle(const nsAString& aToken, bool* aResult)
{
  nsresult rv = CheckToken(aToken);
  NS_ENSURE_SUCCESS(rv, rv);

  const nsAttrValue* attr = GetParsedAttr();

  if (attr && attr->Contains(aToken)) {
    RemoveInternal(attr, aToken);
    *aResult = false;
  } else {
    AddInternal(attr, aToken);
    *aResult = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMTokenList::ToString(nsAString& aResult)
{
  if (!mElement) {
    aResult.Truncate();
    return NS_OK;
  }

  mElement->GetAttr(kNameSpaceID_None, mAttrAtom, aResult);

  return NS_OK;
}

JSObject*
nsDOMTokenList::WrapObject(JSContext *cx, JSObject *scope, bool *triedToWrap)
{
  return mozilla::dom::binding::DOMTokenList::create(cx, scope, this,
                                                     triedToWrap);
}

