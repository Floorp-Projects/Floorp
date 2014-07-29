/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Headers_h
#define mozilla_dom_Headers_h

#include "mozilla/dom/HeadersBinding.h"
#include "nsClassHashtable.h"
#include "nsWrapperCache.h"

class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;

namespace dom {

template<typename T> class MozMap;
class HeadersOrByteStringSequenceSequenceOrByteStringMozMap;

class Headers MOZ_FINAL : public nsISupports
                        , public nsWrapperCache
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Headers)

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

  nsRefPtr<nsISupports> mOwner;
  HeadersGuardEnum mGuard;
  nsTArray<Entry> mList;

public:
  explicit Headers(nsISupports* aOwner, HeadersGuardEnum aGuard = HeadersGuardEnum::None)
    : mOwner(aOwner)
    , mGuard(aGuard)
  {
    SetIsDOMBinding();
  }

  static bool PrefEnabled(JSContext* cx, JSObject* obj);

  static already_AddRefed<Headers>
  Constructor(const GlobalObject& aGlobal,
              const Optional<HeadersOrByteStringSequenceSequenceOrByteStringMozMap>& aInit,
              ErrorResult& aRv);

  void Append(const nsACString& aName, const nsACString& aValue,
              ErrorResult& aRv);
  void Delete(const nsACString& aName, ErrorResult& aRv);
  void Get(const nsACString& aName, nsCString& aValue, ErrorResult& aRv) const;
  void GetAll(const nsACString& aName, nsTArray<nsCString>& aResults,
              ErrorResult& aRv) const;
  bool Has(const nsACString& aName, ErrorResult& aRv) const;
  void Set(const nsACString& aName, const nsACString& aValue, ErrorResult& aRv);

  // ChromeOnly
  HeadersGuardEnum Guard() const { return mGuard; }
  void SetGuard(HeadersGuardEnum aGuard, ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx);
  nsISupports* GetParentObject() const { return mOwner; }

private:
  Headers(const Headers& aOther) MOZ_DELETE;
  virtual ~Headers();

  static bool IsSimpleHeader(const nsACString& aName,
                             const nsACString* aValue = nullptr);
  static bool IsInvalidName(const nsACString& aName, ErrorResult& aRv);
  static bool IsInvalidValue(const nsACString& aValue, ErrorResult& aRv);
  bool IsImmutable(ErrorResult& aRv) const;
  bool IsForbiddenRequestHeader(const nsACString& aName) const;
  bool IsForbiddenRequestNoCorsHeader(const nsACString& aName,
                                      const nsACString* aValue = nullptr) const;
  bool IsForbiddenResponseHeader(const nsACString& aName) const;

  bool IsInvalidMutableHeader(const nsACString& aName,
                              const nsACString* aValue,
                              ErrorResult& aRv) const
  {
    return IsInvalidName(aName, aRv) ||
           (aValue && IsInvalidValue(*aValue, aRv)) ||
           IsImmutable(aRv) ||
           IsForbiddenRequestHeader(aName) ||
           IsForbiddenRequestNoCorsHeader(aName, aValue) ||
           IsForbiddenResponseHeader(aName);
  }

  void Fill(const Headers& aInit, ErrorResult& aRv);
  void Fill(const Sequence<Sequence<nsCString>>& aInit, ErrorResult& aRv);
  void Fill(const MozMap<nsCString>& aInit, ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Headers_h
