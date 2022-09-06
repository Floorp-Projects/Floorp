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
class USVStringSequenceSequenceOrUSVStringUSVStringRecordOrUSVString;

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
      const USVStringSequenceSequenceOrUSVStringUSVStringRecordOrUSVString&
          aInit,
      ErrorResult& aRv);

  void ParseInput(const nsACString& aInput);

  void Serialize(nsAString& aValue) const;

  void Get(const nsAString& aName, nsString& aRetval);

  void GetAll(const nsAString& aName, nsTArray<nsString>& aRetval);

  void Set(const nsAString& aName, const nsAString& aValue);

  void Append(const nsAString& aName, const nsAString& aValue);

  bool Has(const nsAString& aName);

  void Delete(const nsAString& aName);

  uint32_t GetIterableLength() const;
  const nsAString& GetKeyAtIndex(uint32_t aIndex) const;
  const nsAString& GetValueAtIndex(uint32_t aIndex) const;

  void Sort(ErrorResult& aRv);

  void Stringify(nsString& aRetval) const { Serialize(aRetval); }

  static already_AddRefed<URLSearchParams> ReadStructuredClone(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      JSStructuredCloneReader* aReader);

  bool WriteStructuredClone(JSContext* aCx,
                            JSStructuredCloneWriter* aWriter) const;

  nsresult GetSendInfo(nsIInputStream** aBody, uint64_t* aContentLength,
                       nsACString& aContentTypeWithCharset,
                       nsACString& aCharset) const;

 private:
  bool ReadStructuredClone(JSStructuredCloneReader* aReader);

  bool WriteStructuredClone(JSStructuredCloneWriter* aWriter) const;

  void AppendInternal(const nsAString& aName, const nsAString& aValue);

  void DeleteAll();

  void NotifyObserver();

  UniquePtr<URLParams> mParams;
  nsCOMPtr<nsISupports> mParent;
  RefPtr<URLSearchParamsObserver> mObserver;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_URLSearchParams_h */
