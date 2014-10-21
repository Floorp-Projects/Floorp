/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InternalHeaders_h
#define mozilla_dom_InternalHeaders_h

// needed for HeadersGuardEnum.
#include "mozilla/dom/HeadersBinding.h"
#include "mozilla/dom/UnionTypes.h"

#include "nsClassHashtable.h"
#include "nsWrapperCache.h"

class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;

namespace dom {

template<typename T> class MozMap;
class HeadersOrByteStringSequenceSequenceOrByteStringMozMap;

class InternalHeaders MOZ_FINAL
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(InternalHeaders)

private:
  struct Entry
  {
    Entry(const nsACString& aName, const nsACString& aValue)
      : mName(aName)
      , mValue(aValue)
    { }

    Entry() { }

    nsCString mName;
    nsCString mValue;
  };

  HeadersGuardEnum mGuard;
  nsTArray<Entry> mList;

public:
  explicit InternalHeaders(HeadersGuardEnum aGuard = HeadersGuardEnum::None)
    : mGuard(aGuard)
  {
  }

  explicit InternalHeaders(const InternalHeaders& aOther)
    : mGuard(aOther.mGuard)
  {
    ErrorResult result;
    Fill(aOther, result);
    MOZ_ASSERT(!result.Failed());
  }

  void Append(const nsACString& aName, const nsACString& aValue,
              ErrorResult& aRv);
  void Delete(const nsACString& aName, ErrorResult& aRv);
  void Get(const nsACString& aName, nsCString& aValue, ErrorResult& aRv) const;
  void GetAll(const nsACString& aName, nsTArray<nsCString>& aResults,
              ErrorResult& aRv) const;
  bool Has(const nsACString& aName, ErrorResult& aRv) const;
  void Set(const nsACString& aName, const nsACString& aValue, ErrorResult& aRv);

  void Clear();

  HeadersGuardEnum Guard() const { return mGuard; }
  void SetGuard(HeadersGuardEnum aGuard, ErrorResult& aRv);

  void Fill(const InternalHeaders& aInit, ErrorResult& aRv);
  void Fill(const Sequence<Sequence<nsCString>>& aInit, ErrorResult& aRv);
  void Fill(const MozMap<nsCString>& aInit, ErrorResult& aRv);

  bool HasOnlySimpleHeaders() const;

  static already_AddRefed<InternalHeaders>
  BasicHeaders(InternalHeaders* aHeaders);

  static already_AddRefed<InternalHeaders>
  CORSHeaders(InternalHeaders* aHeaders);
private:
  virtual ~InternalHeaders();

  static bool IsSimpleHeader(const nsACString& aName,
                             const nsACString& aValue);
  static bool IsInvalidName(const nsACString& aName, ErrorResult& aRv);
  static bool IsInvalidValue(const nsACString& aValue, ErrorResult& aRv);
  bool IsImmutable(ErrorResult& aRv) const;
  bool IsForbiddenRequestHeader(const nsACString& aName) const;
  bool IsForbiddenRequestNoCorsHeader(const nsACString& aName) const;
  bool IsForbiddenRequestNoCorsHeader(const nsACString& aName,
                                      const nsACString& aValue) const;
  bool IsForbiddenResponseHeader(const nsACString& aName) const;

  bool IsInvalidMutableHeader(const nsACString& aName,
                              ErrorResult& aRv) const
  {
    return IsInvalidMutableHeader(aName, EmptyCString(), aRv);
  }

  bool IsInvalidMutableHeader(const nsACString& aName,
                              const nsACString& aValue,
                              ErrorResult& aRv) const
  {
    return IsInvalidName(aName, aRv) ||
           IsInvalidValue(aValue, aRv) ||
           IsImmutable(aRv) ||
           IsForbiddenRequestHeader(aName) ||
           IsForbiddenRequestNoCorsHeader(aName, aValue) ||
           IsForbiddenResponseHeader(aName);
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InternalHeaders_h
