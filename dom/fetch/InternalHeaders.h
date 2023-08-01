/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InternalHeaders_h
#define mozilla_dom_InternalHeaders_h

// needed for HeadersGuardEnum.
#include "mozilla/dom/HeadersBinding.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/UnionTypes.h"

#include "nsClassHashtable.h"
#include "nsWrapperCache.h"

class nsIRequest;

namespace mozilla {

class ErrorResult;

namespace dom {

template <typename K, typename V>
class Record;
class HeadersEntry;

class InternalHeaders final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(InternalHeaders)

 public:
  struct Entry {
    Entry(const nsACString& aName, const nsACString& aValue)
        : mName(aName), mValue(aValue) {}

    Entry() = default;

    nsCString mName;
    nsCString mValue;
  };

 private:
  HeadersGuardEnum mGuard;
  nsTArray<Entry> mList;

  nsTArray<Entry> mSortedList;

  // This boolean is set to true at any writing operation to mList. It's set to
  // false when mSortedList is regenerated. This happens when the header is
  // iterated.
  bool mListDirty;

 public:
  explicit InternalHeaders(HeadersGuardEnum aGuard = HeadersGuardEnum::None)
      : mGuard(aGuard), mListDirty(false) {}

  explicit InternalHeaders(const InternalHeaders& aOther)
      : mGuard(HeadersGuardEnum::None), mListDirty(true) {
    ErrorResult result;
    Fill(aOther, result);
    MOZ_ASSERT(!result.Failed());
    // Note that it's important to set the guard after Fill(), to make sure
    // that Fill() doesn't fail if aOther is immutable.
    mGuard = aOther.mGuard;
  }

  explicit InternalHeaders(nsTArray<Entry>&& aHeaders,
                           HeadersGuardEnum aGuard = HeadersGuardEnum::None);

  InternalHeaders(const nsTArray<HeadersEntry>& aHeadersEntryList,
                  HeadersGuardEnum aGuard);

  void ToIPC(nsTArray<HeadersEntry>& aIPCHeaders, HeadersGuardEnum& aGuard);

  void Append(const nsACString& aName, const nsACString& aValue,
              ErrorResult& aRv);
  void Delete(const nsACString& aName, ErrorResult& aRv);
  void Get(const nsACString& aName, nsACString& aValue, ErrorResult& aRv) const;
  void GetSetCookie(nsTArray<nsCString>& aValues) const;
  void GetFirst(const nsACString& aName, nsACString& aValue,
                ErrorResult& aRv) const;
  bool Has(const nsACString& aName, ErrorResult& aRv) const;
  void Set(const nsACString& aName, const nsACString& aValue, ErrorResult& aRv);

  uint32_t GetIterableLength() {
    MaybeSortList();
    return mSortedList.Length();
  }
  const NS_ConvertASCIItoUTF16 GetKeyAtIndex(unsigned aIndex) {
    MaybeSortList();
    MOZ_ASSERT(aIndex < mSortedList.Length());
    return NS_ConvertASCIItoUTF16(mSortedList[aIndex].mName);
  }
  const NS_ConvertASCIItoUTF16 GetValueAtIndex(unsigned aIndex) {
    MaybeSortList();
    MOZ_ASSERT(aIndex < mSortedList.Length());
    return NS_ConvertASCIItoUTF16(mSortedList[aIndex].mValue);
  }

  void Clear();

  HeadersGuardEnum Guard() const { return mGuard; }
  void SetGuard(HeadersGuardEnum aGuard, ErrorResult& aRv);

  void Fill(const InternalHeaders& aInit, ErrorResult& aRv);
  void Fill(const Sequence<Sequence<nsCString>>& aInit, ErrorResult& aRv);
  void Fill(const Record<nsCString, nsCString>& aInit, ErrorResult& aRv);
  void FillResponseHeaders(nsIRequest* aRequest);

  bool HasOnlySimpleHeaders() const;

  bool HasRevalidationHeaders() const;

  static already_AddRefed<InternalHeaders> BasicHeaders(
      InternalHeaders* aHeaders);

  static already_AddRefed<InternalHeaders> CORSHeaders(
      InternalHeaders* aHeaders,
      RequestCredentials aCredentialsMode = RequestCredentials::Omit);

  void GetEntries(nsTArray<InternalHeaders::Entry>& aEntries) const;

  void GetUnsafeHeaders(nsTArray<nsCString>& aNames) const;

 private:
  virtual ~InternalHeaders();

  static bool IsInvalidName(const nsACString& aName, ErrorResult& aRv);
  static bool IsInvalidValue(const nsACString& aValue, ErrorResult& aRv);
  bool IsValidHeaderValue(const nsCString& aLowerName,
                          const nsCString& aNormalizedValue, ErrorResult& aRv);
  bool IsImmutable(ErrorResult& aRv) const;
  bool IsForbiddenRequestHeader(const nsCString& aName,
                                const nsACString& aValue) const;
  bool IsForbiddenRequestNoCorsHeader(const nsCString& aName) const;
  bool IsForbiddenRequestNoCorsHeader(const nsCString& aName,
                                      const nsACString& aValue) const;
  bool IsForbiddenResponseHeader(const nsCString& aName) const;

  bool IsInvalidMutableHeader(const nsCString& aName, ErrorResult& aRv) const {
    return IsInvalidMutableHeader(aName, ""_ns, aRv);
  }

  bool IsInvalidMutableHeader(const nsCString& aName, const nsACString& aValue,
                              ErrorResult& aRv) const {
    return IsInvalidName(aName, aRv) || IsInvalidValue(aValue, aRv) ||
           IsImmutable(aRv) || IsForbiddenRequestHeader(aName, aValue) ||
           IsForbiddenRequestNoCorsHeader(aName, aValue) ||
           IsForbiddenResponseHeader(aName);
  }

  // This method updates the passed name to match the capitalization of a header
  // with the same name (ignoring case, per the spec).
  void ReuseExistingNameIfExists(nsCString& aName) const;

  void RemovePrivilegedNoCorsRequestHeaders();

  void GetInternal(const nsCString& aLowerName, nsACString& aValue,
                   ErrorResult& aRv) const;

  bool DeleteInternal(const nsCString& aLowerName, ErrorResult& aRv);

  static bool IsNoCorsSafelistedRequestHeaderName(const nsCString& aName);

  static bool IsPrivilegedNoCorsRequestHeaderName(const nsCString& aName);

  static bool IsRevalidationHeader(const nsCString& aName);

  void MaybeSortList();
  void SetListDirty();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_InternalHeaders_h
