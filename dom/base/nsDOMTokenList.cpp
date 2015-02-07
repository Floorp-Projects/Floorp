/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOMTokenList specified by HTML5.
 */

#include "nsDOMTokenList.h"

#include "nsAttrValue.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/DOMTokenListBinding.h"
#include "mozilla/ErrorResult.h"

using namespace mozilla;
using namespace mozilla::dom;

nsDOMTokenList::nsDOMTokenList(Element* aElement, nsIAtom* aAttrAtom)
  : mElement(aElement),
    mAttrAtom(aAttrAtom)
{
  // We don't add a reference to our element. If it goes away,
  // we'll be told to drop our reference
}

nsDOMTokenList::~nsDOMTokenList() { }

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsDOMTokenList, mElement)

NS_INTERFACE_MAP_BEGIN(nsDOMTokenList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsDOMTokenList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMTokenList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMTokenList)

const nsAttrValue*
nsDOMTokenList::GetParsedAttr()
{
  if (!mElement) {
    return nullptr;
  }
  return mElement->GetAttrInfo(kNameSpaceID_None, mAttrAtom).mValue;
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

nsresult
nsDOMTokenList::CheckTokens(const nsTArray<nsString>& aTokens)
{
  for (uint32_t i = 0, l = aTokens.Length(); i < l; ++i) {
    nsresult rv = CheckToken(aTokens[i]);
    if (NS_FAILED(rv)) {
      return rv;
    }
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

void
nsDOMTokenList::AddInternal(const nsAttrValue* aAttr,
                            const nsTArray<nsString>& aTokens)
{
  if (!mElement) {
    return;
  }

  nsAutoString resultStr;

  if (aAttr) {
    aAttr->ToString(resultStr);
  }

  bool oneWasAdded = false;
  nsAutoTArray<nsString, 10> addedClasses;

  for (uint32_t i = 0, l = aTokens.Length(); i < l; ++i) {
    const nsString& aToken = aTokens[i];

    if ((aAttr && aAttr->Contains(aToken)) ||
        addedClasses.Contains(aToken)) {
      continue;
    }

    if (oneWasAdded ||
        (!resultStr.IsEmpty() &&
        !nsContentUtils::IsHTMLWhitespace(resultStr.Last()))) {
      resultStr.Append(' ');
      resultStr.Append(aToken);
    } else {
      resultStr.Append(aToken);
    }

    oneWasAdded = true;
    addedClasses.AppendElement(aToken);
  }

  mElement->SetAttr(kNameSpaceID_None, mAttrAtom, resultStr, true);
}

void
nsDOMTokenList::Add(const nsTArray<nsString>& aTokens, ErrorResult& aError)
{
  aError = CheckTokens(aTokens);
  if (aError.Failed()) {
    return;
  }

  const nsAttrValue* attr = GetParsedAttr();
  AddInternal(attr, aTokens);
}

void
nsDOMTokenList::Add(const nsAString& aToken, mozilla::ErrorResult& aError)
{
  nsAutoTArray<nsString, 1> tokens;
  tokens.AppendElement(aToken);
  Add(tokens, aError);
}

void
nsDOMTokenList::RemoveInternal(const nsAttrValue* aAttr,
                               const nsTArray<nsString>& aTokens)
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

    if (aTokens.Contains(Substring(tokenStart, iter))) {

      // Skip whitespace after the token, it will be collapsed.
      while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
        ++iter;
      }
      copyStart = iter;
      lastTokenRemoved = true;

    } else {

      if (lastTokenRemoved && !output.IsEmpty()) {
        NS_ABORT_IF_FALSE(!nsContentUtils::IsHTMLWhitespace(
          output.Last()), "Invalid last output token");
        output.Append(char16_t(' '));
      }
      lastTokenRemoved = false;
      output.Append(Substring(copyStart, iter));
      copyStart = iter;
    }
  }

  mElement->SetAttr(kNameSpaceID_None, mAttrAtom, output, true);
}

void
nsDOMTokenList::Remove(const nsTArray<nsString>& aTokens, ErrorResult& aError)
{
  aError = CheckTokens(aTokens);
  if (aError.Failed()) {
    return;
  }

  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    return;
  }

  RemoveInternal(attr, aTokens);
}

void
nsDOMTokenList::Remove(const nsAString& aToken, mozilla::ErrorResult& aError)
{
  nsAutoTArray<nsString, 1> tokens;
  tokens.AppendElement(aToken);
  Remove(tokens, aError);
}

bool
nsDOMTokenList::Toggle(const nsAString& aToken,
                       const Optional<bool>& aForce,
                       ErrorResult& aError)
{
  aError = CheckToken(aToken);
  if (aError.Failed()) {
    return false;
  }

  const nsAttrValue* attr = GetParsedAttr();
  const bool forceOn = aForce.WasPassed() && aForce.Value();
  const bool forceOff = aForce.WasPassed() && !aForce.Value();

  bool isPresent = attr && attr->Contains(aToken);
  nsAutoTArray<nsString, 1> tokens;
  (*tokens.AppendElement()).Rebind(aToken.Data(), aToken.Length());

  if (isPresent) {
    if (!forceOn) {
      RemoveInternal(attr, tokens);
      isPresent = false;
    }
  } else {
    if (!forceOff) {
      AddInternal(attr, tokens);
      isPresent = true;
    }
  }

  return isPresent;
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

JSObject*
nsDOMTokenList::WrapObject(JSContext *cx)
{
  return DOMTokenListBinding::Wrap(cx, this);
}

