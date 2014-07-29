/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Headers.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/UnionTypes.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsDOMString.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(Headers)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Headers)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Headers, mOwner)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Headers)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// static
already_AddRefed<Headers>
Headers::Constructor(const GlobalObject& aGlobal,
                     const Optional<HeadersOrByteStringSequenceSequenceOrByteStringMozMap>& aInit,
                     ErrorResult& aRv)
{
  nsRefPtr<Headers> headers = new Headers(aGlobal.GetAsSupports());

  if (!aInit.WasPassed()) {
    return headers.forget();
  }

  if (aInit.Value().IsHeaders()) {
    headers->Fill(aInit.Value().GetAsHeaders(), aRv);
  } else if (aInit.Value().IsByteStringSequenceSequence()) {
    headers->Fill(aInit.Value().GetAsByteStringSequenceSequence(), aRv);
  } else if (aInit.Value().IsByteStringMozMap()) {
    headers->Fill(aInit.Value().GetAsByteStringMozMap(), aRv);
  }

  if (aRv.Failed()) {
    return nullptr;
  }

  return headers.forget();
}

void
Headers::Append(const nsACString& aName, const nsACString& aValue,
                ErrorResult& aRv)
{
  nsAutoCString lowerName;
  ToLowerCase(aName, lowerName);

  if (IsInvalidMutableHeader(lowerName, &aValue, aRv)) {
    return;
  }

  mList.AppendElement(Entry(lowerName, aValue));
}

void
Headers::Delete(const nsACString& aName, ErrorResult& aRv)
{
  nsAutoCString lowerName;
  ToLowerCase(aName, lowerName);

  if (IsInvalidMutableHeader(lowerName, nullptr, aRv)) {
    return;
  }

  // remove in reverse order to minimize copying
  for (int32_t i = mList.Length() - 1; i >= 0; --i) {
    if (lowerName == mList[i].mName) {
      mList.RemoveElementAt(i);
    }
  }
}

void
Headers::Get(const nsACString& aName, nsCString& aValue, ErrorResult& aRv) const
{
  nsAutoCString lowerName;
  ToLowerCase(aName, lowerName);

  if (IsInvalidName(lowerName, aRv)) {
    return;
  }

  for (uint32_t i = 0; i < mList.Length(); ++i) {
    if (lowerName == mList[i].mName) {
      aValue = mList[i].mValue;
      return;
    }
  }

  // No value found, so return null to content
  aValue.SetIsVoid(true);
}

void
Headers::GetAll(const nsACString& aName, nsTArray<nsCString>& aResults,
                ErrorResult& aRv) const
{
  nsAutoCString lowerName;
  ToLowerCase(aName, lowerName);

  if (IsInvalidName(lowerName, aRv)) {
    return;
  }

  aResults.SetLength(0);
  for (uint32_t i = 0; i < mList.Length(); ++i) {
    if (lowerName == mList[i].mName) {
      aResults.AppendElement(mList[i].mValue);
    }
  }
}

bool
Headers::Has(const nsACString& aName, ErrorResult& aRv) const
{
  nsAutoCString lowerName;
  ToLowerCase(aName, lowerName);

  if (IsInvalidName(lowerName, aRv)) {
    return false;
  }

  for (uint32_t i = 0; i < mList.Length(); ++i) {
    if (lowerName == mList[i].mName) {
      return true;
    }
  }
  return false;
}

void
Headers::Set(const nsACString& aName, const nsACString& aValue, ErrorResult& aRv)
{
  nsAutoCString lowerName;
  ToLowerCase(aName, lowerName);

  if (IsInvalidMutableHeader(lowerName, &aValue, aRv)) {
    return;
  }

  int32_t firstIndex = INT32_MAX;

  // remove in reverse order to minimize copying
  for (int32_t i = mList.Length() - 1; i >= 0; --i) {
    if (lowerName == mList[i].mName) {
      firstIndex = std::min(firstIndex, i);
      mList.RemoveElementAt(i);
    }
  }

  if (firstIndex < INT32_MAX) {
    Entry* entry = mList.InsertElementAt(firstIndex);
    entry->mName = lowerName;
    entry->mValue = aValue;
  } else {
    mList.AppendElement(Entry(lowerName, aValue));
  }
}

void
Headers::SetGuard(HeadersGuardEnum aGuard, ErrorResult& aRv)
{
  // Rather than re-validate all current headers, just require code to set
  // this prior to populating the Headers object.  Allow setting immutable
  // late, though, as that is pretty much required to have a  useful, immutable
  // headers object.
  if (aGuard != HeadersGuardEnum::Immutable && mList.Length() > 0) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
  mGuard = aGuard;
}

JSObject*
Headers::WrapObject(JSContext* aCx)
{
  return mozilla::dom::HeadersBinding::Wrap(aCx, this);
}

Headers::~Headers()
{
}

// static
bool
Headers::IsSimpleHeader(const nsACString& aName, const nsACString* aValue)
{
  // Note, we must allow a null content-type value here to support
  // get("content-type"), but the IsInvalidValue() check will prevent null
  // from being set or appended.
  return aName.EqualsLiteral("accept") ||
         aName.EqualsLiteral("accept-language") ||
         aName.EqualsLiteral("content-language") ||
         (aName.EqualsLiteral("content-type") &&
          (!aValue || nsContentUtils::IsAllowedNonCorsContentType(*aValue)));
}

//static
bool
Headers::IsInvalidName(const nsACString& aName, ErrorResult& aRv)
{
  if (!NS_IsValidHTTPToken(aName)) {
    NS_ConvertUTF8toUTF16 label(aName);
    aRv.ThrowTypeError(MSG_INVALID_HEADER_NAME, &label);
    return true;
  }

  return false;
}

// static
bool
Headers::IsInvalidValue(const nsACString& aValue, ErrorResult& aRv)
{
  if (!NS_IsReasonableHTTPHeaderValue(aValue)) {
    NS_ConvertUTF8toUTF16 label(aValue);
    aRv.ThrowTypeError(MSG_INVALID_HEADER_VALUE, &label);
    return true;
  }
  return false;
}

bool
Headers::IsImmutable(ErrorResult& aRv) const
{
  if (mGuard == HeadersGuardEnum::Immutable) {
    aRv.ThrowTypeError(MSG_HEADERS_IMMUTABLE);
    return true;
  }
  return false;
}

bool
Headers::IsForbiddenRequestHeader(const nsACString& aName) const
{
  return mGuard == HeadersGuardEnum::Request &&
         nsContentUtils::IsForbiddenRequestHeader(aName);
}

bool
Headers::IsForbiddenRequestNoCorsHeader(const nsACString& aName,
                                        const nsACString* aValue) const
{
  return mGuard == HeadersGuardEnum::Request_no_cors &&
         !IsSimpleHeader(aName, aValue);
}

bool
Headers::IsForbiddenResponseHeader(const nsACString& aName) const
{
  return mGuard == HeadersGuardEnum::Response &&
         nsContentUtils::IsForbiddenResponseHeader(aName);
}

void
Headers::Fill(const Headers& aInit, ErrorResult&)
{
  mList = aInit.mList;
}

void
Headers::Fill(const Sequence<Sequence<nsCString>>& aInit, ErrorResult& aRv)
{
  for (uint32_t i = 0; i < aInit.Length() && !aRv.Failed(); ++i) {
    const Sequence<nsCString>& tuple = aInit[i];
    if (tuple.Length() != 2) {
      aRv.ThrowTypeError(MSG_INVALID_HEADER_SEQUENCE);
      return;
    }
    Append(tuple[0], tuple[1], aRv);
  }
}

void
Headers::Fill(const MozMap<nsCString>& aInit, ErrorResult& aRv)
{
  nsTArray<nsString> keys;
  aInit.GetKeys(keys);
  for (uint32_t i = 0; i < keys.Length() && !aRv.Failed(); ++i) {
    Append(NS_ConvertUTF16toUTF8(keys[i]), aInit.Get(keys[i]), aRv);
  }
}

} // namespace dom
} // namespace mozilla
