/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of nsIDOMDOMTokenList specified by HTML5.
 */

#include "nsDOMTokenList.h"

#include "nsAttrValue.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsGenericElement.h"
#include "mozilla/dom/DOMTokenListBinding.h"
#include "dombindings.h"
#include "mozilla/ErrorResult.h"

using namespace mozilla;
using namespace mozilla::dom;

nsDOMTokenList::nsDOMTokenList(nsGenericElement* aElement, nsIAtom* aAttrAtom)
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
  mElement = nullptr;
}

uint32_t
nsDOMTokenList::Length()
{
  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    return 0;
  }

  return attr->GetAtomCount();
}

NS_IMETHODIMP
nsDOMTokenList::GetLength(uint32_t *aLength)
{
  *aLength = Length();

  return NS_OK;
}

void
nsDOMTokenList::IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aResult)
{
  const nsAttrValue* attr = GetParsedAttr();

  if (attr && aIndex < static_cast<uint32_t>(attr->GetAtomCount())) {
    aFound = true;
    attr->AtomAt(aIndex)->ToString(aResult);
  } else {
    aFound = false;
  }
}

NS_IMETHODIMP
nsDOMTokenList::MozItem(uint32_t aIndex, nsAString& aResult)
{
  Item(aIndex, aResult);
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

bool
nsDOMTokenList::Contains(const nsAString& aToken, ErrorResult& aError)
{
  aError = CheckToken(aToken);
  if (aError.Failed()) {
    return false;
  }

  const nsAttrValue* attr = GetParsedAttr();
  return attr && attr->Contains(aToken);
}

NS_IMETHODIMP
nsDOMTokenList::Contains(const nsAString& aToken, bool* aResult)
{
  ErrorResult rv;
  *aResult = Contains(aToken, rv);
  return rv.ErrorCode();
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

void
nsDOMTokenList::Add(const nsAString& aToken, ErrorResult& aError)
{
  aError = CheckToken(aToken);
  if (aError.Failed()) {
    return;
  }

  const nsAttrValue* attr = GetParsedAttr();

  if (attr && attr->Contains(aToken)) {
    return;
  }

  AddInternal(attr, aToken);
}

NS_IMETHODIMP
nsDOMTokenList::Add(const nsAString& aToken)
{
  ErrorResult rv;
  Add(aToken, rv);
  return rv.ErrorCode();
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

void
nsDOMTokenList::Remove(const nsAString& aToken, ErrorResult& aError)
{
  aError = CheckToken(aToken);
  if (aError.Failed()) {
    return;
  }

  const nsAttrValue* attr = GetParsedAttr();
  if (!attr || !attr->Contains(aToken)) {
    return;
  }

  RemoveInternal(attr, aToken);
}

NS_IMETHODIMP
nsDOMTokenList::Remove(const nsAString& aToken)
{
  ErrorResult rv;
  Remove(aToken, rv);
  return rv.ErrorCode();
}

bool
nsDOMTokenList::Toggle(const nsAString& aToken, ErrorResult& aError)
{
  aError = CheckToken(aToken);
  if (aError.Failed()) {
    return false;
  }

  const nsAttrValue* attr = GetParsedAttr();

  if (attr && attr->Contains(aToken)) {
    RemoveInternal(attr, aToken);
    return false;
  }

  AddInternal(attr, aToken);
  return true;
}

NS_IMETHODIMP
nsDOMTokenList::Toggle(const nsAString& aToken, bool* aResult)
{
  ErrorResult rv;
  *aResult = Toggle(aToken, rv);
  return rv.ErrorCode();
}

void
nsDOMTokenList::Stringify(nsAString& aResult)
{
  if (!mElement) {
    aResult.Truncate();
    return;
  }

  mElement->GetAttr(kNameSpaceID_None, mAttrAtom, aResult);
}

NS_IMETHODIMP
nsDOMTokenList::ToString(nsAString& aResult)
{
  Stringify(aResult);
  return NS_OK;
}

JSObject*
nsDOMTokenList::WrapObject(JSContext *cx, JSObject *scope, bool *triedToWrap)
{
  JSObject* obj = DOMTokenListBinding::Wrap(cx, scope, this, triedToWrap);
  if (obj || *triedToWrap) {
    return obj;
  }

  *triedToWrap = true;
  return oldproxybindings::DOMTokenList::create(cx, scope, this);
}

