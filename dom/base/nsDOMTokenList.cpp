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
#include "nsAttrValueInlines.h"
#include "nsDataHashtable.h"
#include "nsError.h"
#include "nsHashKeys.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/DOMTokenListBinding.h"
#include "mozilla/BloomFilter.h"
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

void
nsDOMTokenList::RemoveDuplicates(const nsAttrValue* aAttr)
{
  if (!aAttr || aAttr->Type() != nsAttrValue::eAtomArray) {
    return;
  }

  BloomFilter<8, nsIAtom> filter;
  nsAttrValue::AtomArray* array = aAttr->GetAtomArrayValue();
  for (uint32_t i = 0; i < array->Length(); i++) {
    nsIAtom* atom = array->ElementAt(i);
    if (filter.mightContain(atom)) {
      // Start again, with a hashtable
      RemoveDuplicatesInternal(array, i);
      return;
    } else {
      filter.add(atom);
    }
  }
}

void
nsDOMTokenList::RemoveDuplicatesInternal(nsAttrValue::AtomArray* aArray,
                                         uint32_t aStart)
{
  nsDataHashtable<nsPtrHashKey<nsIAtom>, bool> tokens;

  for (uint32_t i = 0; i < aArray->Length(); i++) {
    nsIAtom* atom = aArray->ElementAt(i);
    // No need to check the hashtable below aStart
    if (i >= aStart && tokens.Get(atom)) {
      aArray->RemoveElementAt(i);
      i--;
    } else {
      tokens.Put(atom, true);
    }
  }
}

uint32_t
nsDOMTokenList::Length()
{
  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    return 0;
  }

  RemoveDuplicates(attr);
  return attr->GetAtomCount();
}

void
nsDOMTokenList::IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aResult)
{
  const nsAttrValue* attr = GetParsedAttr();

  if (!attr || aIndex >= static_cast<uint32_t>(attr->GetAtomCount())) {
    aFound = false;
    return;
  }

  RemoveDuplicates(attr);

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
    RemoveDuplicates(aAttr);
    for (uint32_t i = 0; i < aAttr->GetAtomCount(); i++) {
      if (i != 0) {
        resultStr.AppendLiteral(" ");
      }
      resultStr.Append(nsDependentAtomString(aAttr->AtomAt(i)));
    }
  }

  AutoTArray<nsString, 10> addedClasses;

  for (uint32_t i = 0, l = aTokens.Length(); i < l; ++i) {
    const nsString& aToken = aTokens[i];

    if ((aAttr && aAttr->Contains(aToken)) ||
        addedClasses.Contains(aToken)) {
      continue;
    }

    if (!resultStr.IsEmpty()) {
      resultStr.Append(' ');
    }
    resultStr.Append(aToken);

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

  RemoveDuplicates(aAttr);

  nsAutoString resultStr;
  for (uint32_t i = 0; i < aAttr->GetAtomCount(); i++) {
    if (aTokens.Contains(nsDependentAtomString(aAttr->AtomAt(i)))) {
      continue;
    }
    if (!resultStr.IsEmpty()) {
      resultStr.AppendLiteral(" ");
    }
    resultStr.Append(nsDependentAtomString(aAttr->AtomAt(i)));
  }

  mElement->SetAttr(kNameSpaceID_None, mAttrAtom, resultStr, true);
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

  if (isPresent && !forceOn) {
    RemoveInternal(attr, tokens);
    return false;
  }

  if (!isPresent && !forceOff) {
    AddInternal(attr, tokens);
    return true;
  }

  if (attr) {
    // Remove duplicates and whitespace from attr
    RemoveDuplicates(attr);

    nsAutoString resultStr;
    for (uint32_t i = 0; i < attr->GetAtomCount(); i++) {
      if (!resultStr.IsEmpty()) {
        resultStr.AppendLiteral(" ");
      }
      resultStr.Append(nsDependentAtomString(attr->AtomAt(i)));
    }

    mElement->SetAttr(kNameSpaceID_None, mAttrAtom, resultStr, true);
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
  RemoveDuplicates(aAttr);

  bool sawIt = false;
  nsAutoString resultStr;
  for (uint32_t i = 0; i < aAttr->GetAtomCount(); i++) {
    if (aAttr->AtomAt(i)->Equals(aToken) ||
        aAttr->AtomAt(i)->Equals(aNewToken)) {
      if (sawIt) {
        // We keep only the first
        continue;
      }
      sawIt = true;
      if (!resultStr.IsEmpty()) {
        resultStr.AppendLiteral(" ");
      }
      resultStr.Append(aNewToken);
      continue;
    }
    if (!resultStr.IsEmpty()) {
      resultStr.AppendLiteral(" ");
    }
    resultStr.Append(nsDependentAtomString(aAttr->AtomAt(i)));
  }

  mElement->SetAttr(kNameSpaceID_None, mAttrAtom, resultStr, true);
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

