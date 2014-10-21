/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/InternalHeaders.h"

#include "mozilla/ErrorResult.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"

namespace mozilla {
namespace dom {

void
InternalHeaders::Append(const nsACString& aName, const nsACString& aValue,
                        ErrorResult& aRv)
{
  nsAutoCString lowerName;
  ToLowerCase(aName, lowerName);

  if (IsInvalidMutableHeader(lowerName, aValue, aRv)) {
    return;
  }

  mList.AppendElement(Entry(lowerName, aValue));
}

void
InternalHeaders::Delete(const nsACString& aName, ErrorResult& aRv)
{
  nsAutoCString lowerName;
  ToLowerCase(aName, lowerName);

  if (IsInvalidMutableHeader(lowerName, aRv)) {
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
InternalHeaders::Get(const nsACString& aName, nsCString& aValue, ErrorResult& aRv) const
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
InternalHeaders::GetAll(const nsACString& aName, nsTArray<nsCString>& aResults,
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
InternalHeaders::Has(const nsACString& aName, ErrorResult& aRv) const
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
InternalHeaders::Set(const nsACString& aName, const nsACString& aValue, ErrorResult& aRv)
{
  nsAutoCString lowerName;
  ToLowerCase(aName, lowerName);

  if (IsInvalidMutableHeader(lowerName, aValue, aRv)) {
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
InternalHeaders::Clear()
{
  mList.Clear();
}

void
InternalHeaders::SetGuard(HeadersGuardEnum aGuard, ErrorResult& aRv)
{
  // Rather than re-validate all current headers, just require code to set
  // this prior to populating the InternalHeaders object.  Allow setting immutable
  // late, though, as that is pretty much required to have a  useful, immutable
  // headers object.
  if (aGuard != HeadersGuardEnum::Immutable && mList.Length() > 0) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
  mGuard = aGuard;
}

InternalHeaders::~InternalHeaders()
{
}

// static
bool
InternalHeaders::IsSimpleHeader(const nsACString& aName, const nsACString& aValue)
{
  // Note, we must allow a null content-type value here to support
  // get("content-type"), but the IsInvalidValue() check will prevent null
  // from being set or appended.
  return aName.EqualsLiteral("accept") ||
         aName.EqualsLiteral("accept-language") ||
         aName.EqualsLiteral("content-language") ||
         (aName.EqualsLiteral("content-type") &&
          nsContentUtils::IsAllowedNonCorsContentType(aValue));
}

//static
bool
InternalHeaders::IsInvalidName(const nsACString& aName, ErrorResult& aRv)
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
InternalHeaders::IsInvalidValue(const nsACString& aValue, ErrorResult& aRv)
{
  if (!NS_IsReasonableHTTPHeaderValue(aValue)) {
    NS_ConvertUTF8toUTF16 label(aValue);
    aRv.ThrowTypeError(MSG_INVALID_HEADER_VALUE, &label);
    return true;
  }
  return false;
}

bool
InternalHeaders::IsImmutable(ErrorResult& aRv) const
{
  if (mGuard == HeadersGuardEnum::Immutable) {
    aRv.ThrowTypeError(MSG_HEADERS_IMMUTABLE);
    return true;
  }
  return false;
}

bool
InternalHeaders::IsForbiddenRequestHeader(const nsACString& aName) const
{
  return mGuard == HeadersGuardEnum::Request &&
         nsContentUtils::IsForbiddenRequestHeader(aName);
}

bool
InternalHeaders::IsForbiddenRequestNoCorsHeader(const nsACString& aName) const
{
  return mGuard == HeadersGuardEnum::Request_no_cors &&
         !IsSimpleHeader(aName, EmptyCString());
}

bool
InternalHeaders::IsForbiddenRequestNoCorsHeader(const nsACString& aName,
                                                const nsACString& aValue) const
{
  return mGuard == HeadersGuardEnum::Request_no_cors &&
         !IsSimpleHeader(aName, aValue);
}

bool
InternalHeaders::IsForbiddenResponseHeader(const nsACString& aName) const
{
  return mGuard == HeadersGuardEnum::Response &&
         nsContentUtils::IsForbiddenResponseHeader(aName);
}

void
InternalHeaders::Fill(const InternalHeaders& aInit, ErrorResult& aRv)
{
  const nsTArray<Entry>& list = aInit.mList;
  for (uint32_t i = 0; i < list.Length() && !aRv.Failed(); ++i) {
    const Entry& entry = list[i];
    Append(entry.mName, entry.mValue, aRv);
  }
}

void
InternalHeaders::Fill(const Sequence<Sequence<nsCString>>& aInit, ErrorResult& aRv)
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
InternalHeaders::Fill(const MozMap<nsCString>& aInit, ErrorResult& aRv)
{
  nsTArray<nsString> keys;
  aInit.GetKeys(keys);
  for (uint32_t i = 0; i < keys.Length() && !aRv.Failed(); ++i) {
    Append(NS_ConvertUTF16toUTF8(keys[i]), aInit.Get(keys[i]), aRv);
  }
}

bool
InternalHeaders::HasOnlySimpleHeaders() const
{
  for (uint32_t i = 0; i < mList.Length(); ++i) {
    if (!IsSimpleHeader(mList[i].mName, mList[i].mValue)) {
      return false;
    }
  }

  return true;
}

// static
already_AddRefed<InternalHeaders>
InternalHeaders::BasicHeaders(InternalHeaders* aHeaders)
{
  nsRefPtr<InternalHeaders> basic = new InternalHeaders(*aHeaders);
  ErrorResult result;
  // The Set-Cookie headers cannot be invalid mutable headers, so the Delete
  // must succeed.
  basic->Delete(NS_LITERAL_CSTRING("Set-Cookie"), result);
  MOZ_ASSERT(!result.Failed());
  basic->Delete(NS_LITERAL_CSTRING("Set-Cookie2"), result);
  MOZ_ASSERT(!result.Failed());
  return basic.forget();
}

// static
already_AddRefed<InternalHeaders>
InternalHeaders::CORSHeaders(InternalHeaders* aHeaders)
{
  nsRefPtr<InternalHeaders> cors = new InternalHeaders(aHeaders->mGuard);
  ErrorResult result;

  nsAutoTArray<nsCString, 1> acExposedNames;
  aHeaders->GetAll(NS_LITERAL_CSTRING("Access-Control-Expose-Headers"), acExposedNames, result);
  MOZ_ASSERT(!result.Failed());

  nsCaseInsensitiveCStringArrayComparator comp;
  for (uint32_t i = 0; i < aHeaders->mList.Length(); ++i) {
    const Entry& entry = aHeaders->mList[i];
    if (entry.mName.EqualsASCII("cache-control") ||
        entry.mName.EqualsASCII("content-language") ||
        entry.mName.EqualsASCII("content-type") ||
        entry.mName.EqualsASCII("expires") ||
        entry.mName.EqualsASCII("last-modified") ||
        entry.mName.EqualsASCII("pragma") ||
        acExposedNames.Contains(entry.mName, comp)) {
      cors->Append(entry.mName, entry.mValue, result);
      MOZ_ASSERT(!result.Failed());
    }
  }

  return cors.forget();
}
} // namespace dom
} // namespace mozilla
