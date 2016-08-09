/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsWhitespaceTokenizer.h"
#include "mozilla/ErrorResult.h"

using namespace mozilla;
using namespace mozilla::dom;

nsDOMTokenList::nsDOMTokenList(Element* aElement, nsIAtom* aAttrAtom,
                               const DOMTokenListSupportedTokenArray aSupportedTokens)
  : mElement(aElement),
    mAttrAtom(aAttrAtom),
    mSupportedTokens(aSupportedTokens)
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

void
nsDOMTokenList::SetValue(const nsAString& aValue, ErrorResult& rv)
{
  if (!mElement) {
    return;
  }

  rv = mElement->SetAttr(kNameSpaceID_None, mAttrAtom, aValue, true);
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
nsDOMTokenList::Contains(const nsAString& aToken)
{
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
  AutoTArray<nsString, 10> addedClasses;

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
nsDOMTokenList::Add(const nsAString& aToken, ErrorResult& aError)
{
  AutoTArray<nsString, 1> tokens;
  tokens.AppendElement(aToken);
  Add(tokens, aError);
}

void
nsDOMTokenList::RemoveInternal(const nsAttrValue* aAttr,
                               const nsTArray<nsString>& aTokens)
{
  MOZ_ASSERT(aAttr, "Need an attribute");

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
      MOZ_ASSERT(!lastTokenRemoved, "How did this happen?");

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
        MOZ_ASSERT(!nsContentUtils::IsHTMLWhitespace(output.Last()),
                   "Invalid last output token");
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
nsDOMTokenList::Remove(const nsAString& aToken, ErrorResult& aError)
{
  AutoTArray<nsString, 1> tokens;
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
  AutoTArray<nsString, 1> tokens;
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
nsDOMTokenList::Replace(const nsAString& aToken,
                        const nsAString& aNewToken,
                        ErrorResult& aError)
{
  // Doing this here instead of using `CheckToken` because if aToken had invalid
  // characters, and aNewToken is empty, the returned error should be a
  // SyntaxError, not an InvalidCharacterError.
  if (aNewToken.IsEmpty()) {
    aError.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  aError = CheckToken(aToken);
  if (aError.Failed()) {
    return;
  }

  aError = CheckToken(aNewToken);
  if (aError.Failed()) {
    return;
  }

  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    return;
  }

  ReplaceInternal(attr, aToken, aNewToken);
}

void
nsDOMTokenList::ReplaceInternal(const nsAttrValue* aAttr,
                                const nsAString& aToken,
                                const nsAString& aNewToken)
{
  nsAutoString attribute;
  aAttr->ToString(attribute);

  nsAutoString result;

  nsWhitespaceTokenizerTemplate<nsContentUtils::IsHTMLWhitespace>
    tokenizer(attribute);

  bool sawIt = false;
  while (tokenizer.hasMoreTokens()) {
    auto currentToken = tokenizer.nextToken();
    if (currentToken.Equals(aToken) || currentToken.Equals(aNewToken)) {
      if (!sawIt) {
        sawIt = true;
        if (!result.IsEmpty()) {
          result.Append(char16_t(' '));
        }
        result.Append(aNewToken);
      }
    } else {
      if (!result.IsEmpty()) {
        result.Append(char16_t(' '));
      }
      result.Append(currentToken);
    }
  }

  if (sawIt) {
    mElement->SetAttr(kNameSpaceID_None, mAttrAtom, result, true);
  }
}

bool
nsDOMTokenList::Supports(const nsAString& aToken,
                         ErrorResult& aError)
{
  if (!mSupportedTokens) {
    aError.ThrowTypeError<MSG_TOKENLIST_NO_SUPPORTED_TOKENS>(
      mElement->LocalName(),
      nsDependentAtomString(mAttrAtom));
    return false;
  }

  for (DOMTokenListSupportedToken* supportedToken = mSupportedTokens;
       *supportedToken;
       ++supportedToken) {
    if (aToken.LowerCaseEqualsASCII(*supportedToken)) {
      return true;
    }
  }

  return false;
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
nsDOMTokenList::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return DOMTokenListBinding::Wrap(cx, this, aGivenProto);
}

