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
#include "nsTHashMap.h"
#include "nsError.h"
#include "nsHashKeys.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/DOMTokenListBinding.h"
#include "mozilla/ErrorResult.h"

using namespace mozilla;
using namespace mozilla::dom;

nsDOMTokenList::nsDOMTokenList(
    Element* aElement, nsAtom* aAttrAtom,
    const DOMTokenListSupportedTokenArray aSupportedTokens)
    : mElement(aElement),
      mAttrAtom(aAttrAtom),
      mSupportedTokens(aSupportedTokens) {
  // We don't add a reference to our element. If it goes away,
  // we'll be told to drop our reference
}

nsDOMTokenList::~nsDOMTokenList() = default;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsDOMTokenList, mElement)

NS_INTERFACE_MAP_BEGIN(nsDOMTokenList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsDOMTokenList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMTokenList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMTokenList)

const nsAttrValue* nsDOMTokenList::GetParsedAttr() {
  if (!mElement) {
    return nullptr;
  }
  return mElement->GetAttrInfo(kNameSpaceID_None, mAttrAtom).mValue;
}

static void RemoveDuplicates(const nsAttrValue* aAttr) {
  if (!aAttr || aAttr->Type() != nsAttrValue::eAtomArray) {
    return;
  }
  const_cast<nsAttrValue*>(aAttr)->RemoveDuplicatesFromAtomArray();
}

uint32_t nsDOMTokenList::Length() {
  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    return 0;
  }

  RemoveDuplicates(attr);
  return attr->GetAtomCount();
}

void nsDOMTokenList::IndexedGetter(uint32_t aIndex, bool& aFound,
                                   nsAString& aResult) {
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

void nsDOMTokenList::GetValue(nsAString& aResult) {
  if (!mElement) {
    aResult.Truncate();
    return;
  }

  mElement->GetAttr(mAttrAtom, aResult);
}

void nsDOMTokenList::SetValue(const nsAString& aValue, ErrorResult& rv) {
  if (!mElement) {
    return;
  }

  rv = mElement->SetAttr(kNameSpaceID_None, mAttrAtom, aValue, true);
}

void nsDOMTokenList::CheckToken(const nsAString& aToken, ErrorResult& aRv) {
  if (aToken.IsEmpty()) {
    return aRv.ThrowSyntaxError("The empty string is not a valid token.");
  }

  nsAString::const_iterator iter, end;
  aToken.BeginReading(iter);
  aToken.EndReading(end);

  while (iter != end) {
    if (nsContentUtils::IsHTMLWhitespace(*iter)) {
      return aRv.ThrowInvalidCharacterError(
          "The token can not contain whitespace.");
    }
    ++iter;
  }
}

void nsDOMTokenList::CheckTokens(const nsTArray<nsString>& aTokens,
                                 ErrorResult& aRv) {
  for (uint32_t i = 0, l = aTokens.Length(); i < l; ++i) {
    CheckToken(aTokens[i], aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

bool nsDOMTokenList::Contains(const nsAString& aToken) {
  const nsAttrValue* attr = GetParsedAttr();
  return attr && attr->Contains(aToken);
}

void nsDOMTokenList::AddInternal(const nsAttrValue* aAttr,
                                 const nsTArray<nsString>& aTokens) {
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

    if ((aAttr && aAttr->Contains(aToken)) || addedClasses.Contains(aToken)) {
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

void nsDOMTokenList::Add(const nsTArray<nsString>& aTokens,
                         ErrorResult& aError) {
  CheckTokens(aTokens, aError);
  if (aError.Failed()) {
    return;
  }

  const nsAttrValue* attr = GetParsedAttr();
  AddInternal(attr, aTokens);
}

void nsDOMTokenList::Add(const nsAString& aToken, ErrorResult& aError) {
  AutoTArray<nsString, 1> tokens;
  tokens.AppendElement(aToken);
  Add(tokens, aError);
}

void nsDOMTokenList::RemoveInternal(const nsAttrValue* aAttr,
                                    const nsTArray<nsString>& aTokens) {
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

void nsDOMTokenList::Remove(const nsTArray<nsString>& aTokens,
                            ErrorResult& aError) {
  CheckTokens(aTokens, aError);
  if (aError.Failed()) {
    return;
  }

  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    return;
  }

  RemoveInternal(attr, aTokens);
}

void nsDOMTokenList::Remove(const nsAString& aToken, ErrorResult& aError) {
  AutoTArray<nsString, 1> tokens;
  tokens.AppendElement(aToken);
  Remove(tokens, aError);
}

bool nsDOMTokenList::Toggle(const nsAString& aToken,
                            const Optional<bool>& aForce, ErrorResult& aError) {
  CheckToken(aToken, aError);
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

bool nsDOMTokenList::Replace(const nsAString& aToken,
                             const nsAString& aNewToken, ErrorResult& aError) {
  // Doing this here instead of using `CheckToken` because if aToken had invalid
  // characters, and aNewToken is empty, the returned error should be a
  // SyntaxError, not an InvalidCharacterError.
  if (aNewToken.IsEmpty()) {
    aError.ThrowSyntaxError("The empty string is not a valid token.");
    return false;
  }

  CheckToken(aToken, aError);
  if (aError.Failed()) {
    return false;
  }

  CheckToken(aNewToken, aError);
  if (aError.Failed()) {
    return false;
  }

  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    return false;
  }

  return ReplaceInternal(attr, aToken, aNewToken);
}

bool nsDOMTokenList::ReplaceInternal(const nsAttrValue* aAttr,
                                     const nsAString& aToken,
                                     const nsAString& aNewToken) {
  RemoveDuplicates(aAttr);

  // Trying to do a single pass here leads to really complicated code.  Just do
  // the simple thing.
  bool haveOld = false;
  for (uint32_t i = 0; i < aAttr->GetAtomCount(); ++i) {
    if (aAttr->AtomAt(i)->Equals(aToken)) {
      haveOld = true;
      break;
    }
  }
  if (!haveOld) {
    // Make sure to not touch the attribute value in this case.
    return false;
  }

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

  MOZ_ASSERT(sawIt, "How could we not have found our token this time?");
  mElement->SetAttr(kNameSpaceID_None, mAttrAtom, resultStr, true);
  return true;
}

bool nsDOMTokenList::Supports(const nsAString& aToken, ErrorResult& aError) {
  if (!mSupportedTokens) {
    aError.ThrowTypeError<MSG_TOKENLIST_NO_SUPPORTED_TOKENS>(
        NS_ConvertUTF16toUTF8(mElement->LocalName()),
        NS_ConvertUTF16toUTF8(nsDependentAtomString(mAttrAtom)));
    return false;
  }

  for (DOMTokenListSupportedToken* supportedToken = mSupportedTokens;
       *supportedToken; ++supportedToken) {
    if (aToken.LowerCaseEqualsASCII(*supportedToken)) {
      return true;
    }
  }

  return false;
}

DocGroup* nsDOMTokenList::GetDocGroup() const {
  return mElement ? mElement->OwnerDoc()->GetDocGroup() : nullptr;
}

JSObject* nsDOMTokenList::WrapObject(JSContext* cx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return DOMTokenList_Binding::Wrap(cx, this, aGivenProto);
}
