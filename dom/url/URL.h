/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URL_h
#define mozilla_dom_URL_h

#include "mozilla/dom/URLSearchParams.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsWrapperCache.h"

class nsISupports;

namespace mozilla {

class ErrorResult;

namespace dom {

class Blob;
class MediaSource;
class GlobalObject;
template <typename T>
class Optional;

class URL final : public URLSearchParamsObserver, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(URL)

  explicit URL(nsISupports* aParent) : mParent(aParent) {}

  // WebIDL methods
  nsISupports* GetParentObject() const { return mParent; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<URL> Constructor(const GlobalObject& aGlobal,
                                           const nsAString& aURL,
                                           const Optional<nsAString>& aBase,
                                           ErrorResult& aRv);

  static already_AddRefed<URL> Constructor(nsISupports* aParent,
                                           const nsAString& aURL,
                                           const nsAString& aBase,
                                           ErrorResult& aRv);

  static already_AddRefed<URL> Constructor(nsISupports* aParent,
                                           const nsAString& aURL, nsIURI* aBase,
                                           ErrorResult& aRv);

  static void CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                              nsAString& aResult, ErrorResult& aRv);

  static void CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                              nsAString& aResult, ErrorResult& aRv);

  static void RevokeObjectURL(const GlobalObject& aGlobal,
                              const nsAString& aURL, ErrorResult& aRv);

  static bool IsValidURL(const GlobalObject& aGlobal, const nsAString& aURL,
                         ErrorResult& aRv);

  void GetHref(nsAString& aHref) const;

  void SetHref(const nsAString& aHref, ErrorResult& aRv);

  void GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const;

  void GetProtocol(nsAString& aProtocol) const;

  void SetProtocol(const nsAString& aProtocol, ErrorResult& aRv);

  void GetUsername(nsAString& aUsername) const;

  void SetUsername(const nsAString& aUsername);

  void GetPassword(nsAString& aPassword) const;

  void SetPassword(const nsAString& aPassword);

  void GetHost(nsAString& aHost) const;

  void SetHost(const nsAString& aHost);

  void GetHostname(nsAString& aHostname) const;

  void SetHostname(const nsAString& aHostname);

  void GetPort(nsAString& aPort) const;

  void SetPort(const nsAString& aPort);

  void GetPathname(nsAString& aPathname) const;

  void SetPathname(const nsAString& aPathname);

  void GetSearch(nsAString& aSearch) const;

  void SetSearch(const nsAString& aSearch);

  URLSearchParams* SearchParams();

  void GetHash(nsAString& aHost) const;

  void SetHash(const nsAString& aHash);

  void ToJSON(nsAString& aResult) const { GetHref(aResult); }

  // URLSearchParamsObserver
  void URLSearchParamsUpdated(URLSearchParams* aSearchParams) override;

 private:
  ~URL() = default;

  void SetURI(already_AddRefed<nsIURI> aURI);

  nsIURI* GetURI() const;

  void UpdateURLSearchParams();

 private:
  void SetSearchInternal(const nsAString& aSearch);

  void CreateSearchParamsIfNeeded();

  nsCOMPtr<nsISupports> mParent;
  RefPtr<URLSearchParams> mSearchParams;
  nsCOMPtr<nsIURI> mURI;
};

bool IsChromeURI(nsIURI* aURI);

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_URL_h */
