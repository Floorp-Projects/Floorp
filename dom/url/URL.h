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

  explicit URL(nsISupports* aParent, nsCOMPtr<nsIURI> aURI)
      : mParent(aParent), mURI(std::move(aURI)) {
    MOZ_ASSERT(mURI);
  }

  // WebIDL methods
  nsISupports* GetParentObject() const { return mParent; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<URL> Constructor(const GlobalObject& aGlobal,
                                           const nsACString& aURL,
                                           const Optional<nsACString>& aBase,
                                           ErrorResult& aRv);

  static already_AddRefed<URL> Constructor(nsISupports* aParent,
                                           const nsACString& aURL,
                                           const nsACString& aBase,
                                           ErrorResult& aRv);

  static already_AddRefed<URL> Constructor(nsISupports* aParent,
                                           const nsACString& aURL,
                                           nsIURI* aBase, ErrorResult& aRv);

  static void CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                              nsACString& aResult, ErrorResult& aRv);

  static void CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                              nsACString& aResult, ErrorResult& aRv);

  static void RevokeObjectURL(const GlobalObject& aGlobal,
                              const nsACString& aURL, ErrorResult& aRv);

  static bool IsValidObjectURL(const GlobalObject& aGlobal,
                               const nsACString& aURL, ErrorResult& aRv);

  static already_AddRefed<URL> Parse(const GlobalObject& aGlobal,
                                     const nsACString& aURL,
                                     const Optional<nsACString>& aBase);

  static bool CanParse(const GlobalObject& aGlobal, const nsACString& aURL,
                       const Optional<nsACString>& aBase);

  void GetHref(nsACString& aHref) const;
  void SetHref(const nsACString& aHref, ErrorResult& aRv);

  void GetOrigin(nsACString& aOrigin) const;

  void GetProtocol(nsACString& aProtocol) const;
  void SetProtocol(const nsACString& aProtocol);

  void GetUsername(nsACString& aUsername) const;
  void SetUsername(const nsACString& aUsername);

  void GetPassword(nsACString& aPassword) const;
  void SetPassword(const nsACString& aPassword);

  void GetHost(nsACString& aHost) const;
  void SetHost(const nsACString& aHost);

  void GetHostname(nsACString& aHostname) const;
  void SetHostname(const nsACString& aHostname);

  void GetPort(nsACString& aPort) const;
  void SetPort(const nsACString& aPort);

  void GetPathname(nsACString& aPathname) const;
  void SetPathname(const nsACString& aPathname);

  void GetSearch(nsACString& aSearch) const;
  void SetSearch(const nsACString& aSearch);

  URLSearchParams* SearchParams();

  void GetHash(nsACString& aHash) const;
  void SetHash(const nsACString& aHash);

  void ToJSON(nsACString& aResult) const { GetHref(aResult); }

  // URLSearchParamsObserver
  void URLSearchParamsUpdated(URLSearchParams* aSearchParams) override;

  nsIURI* URI() const;
  static already_AddRefed<URL> FromURI(GlobalObject&, nsIURI*);

 private:
  ~URL() = default;

  static already_AddRefed<nsIURI> ParseURI(const nsACString& aURL,
                                           const Optional<nsACString>& aBase);

  void UpdateURLSearchParams();

 private:
  void SetSearchInternal(const nsACString& aSearch);

  void CreateSearchParamsIfNeeded();

  nsCOMPtr<nsISupports> mParent;
  RefPtr<URLSearchParams> mSearchParams;
  nsCOMPtr<nsIURI> mURI;
};

bool IsChromeURI(nsIURI* aURI);

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_URL_h */
