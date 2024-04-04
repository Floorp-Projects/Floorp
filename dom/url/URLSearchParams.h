/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URLSearchParams_h
#define mozilla_dom_URLSearchParams_h

#include <cstdint>
#include "ErrorList.h"
#include "js/RootingAPI.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class JSObject;
class nsIGlobalObject;
class nsIInputStream;
struct JSContext;
struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;

namespace mozilla {

class ErrorResult;
class URLParams;

namespace dom {

class GlobalObject;
class URLSearchParams;
class UTF8StringSequenceSequenceOrUTF8StringUTF8StringRecordOrUTF8String;
template <typename T>
class Optional;

class URLSearchParamsObserver : public nsISupports {
 public:
  virtual ~URLSearchParamsObserver() = default;

  virtual void URLSearchParamsUpdated(URLSearchParams* aFromThis) = 0;
};

class URLSearchParams final : public nsISupports, public nsWrapperCache {
  ~URLSearchParams();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(URLSearchParams)

  explicit URLSearchParams(nsISupports* aParent,
                           URLSearchParamsObserver* aObserver = nullptr);

  // WebIDL methods
  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<URLSearchParams> Constructor(
      const GlobalObject& aGlobal,
      const UTF8StringSequenceSequenceOrUTF8StringUTF8StringRecordOrUTF8String&
          aInit,
      ErrorResult& aRv);

  void ParseInput(const nsACString& aInput);

  void Serialize(nsACString& aValue) const;

  uint32_t Size() const;

  void Get(const nsACString& aName, nsACString& aRetval);

  void GetAll(const nsACString& aName, nsTArray<nsCString>& aRetval);

  void Set(const nsACString& aName, const nsACString& aValue);

  void Append(const nsACString& aName, const nsACString& aValue);

  bool Has(const nsACString& aName, const Optional<nsACString>& aValue);

  void Delete(const nsACString& aName, const Optional<nsACString>& aValue);

  uint32_t GetIterableLength() const;
  const nsACString& GetKeyAtIndex(uint32_t aIndex) const;
  const nsACString& GetValueAtIndex(uint32_t aIndex) const;

  void Sort(ErrorResult& aRv);

  void Stringify(nsAString&) const;

  nsresult GetSendInfo(nsIInputStream** aBody, uint64_t* aContentLength,
                       nsACString& aContentTypeWithCharset,
                       nsACString& aCharset) const;

 private:
  void AppendInternal(const nsACString& aName, const nsACString& aValue);

  void DeleteAll();

  void NotifyObserver();

  UniquePtr<URLParams> mParams;
  nsCOMPtr<nsISupports> mParent;
  RefPtr<URLSearchParamsObserver> mObserver;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_URLSearchParams_h */
