/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef URL_h___
#define URL_h___

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/URLSearchParams.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "nsWrapperCache.h"

class nsISupports;
class nsIURI;

namespace mozilla {

class ErrorResult;
class DOMMediaStream;

namespace dom {

class Blob;
class MediaSource;
class GlobalObject;
struct objectURLOptions;

namespace workers {
class URLProxy;
} // namespace workers

class URL final : public URLSearchParamsObserver
                , public nsWrapperCache
{
  ~URL() {}

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(URL)

  URL(nsISupports* aParent, already_AddRefed<nsIURI> aURI);

  // WebIDL methods
  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<URL>
  Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
              URL& aBase, ErrorResult& aRv);
  static already_AddRefed<URL>
  Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
              const Optional<nsAString>& aBase, ErrorResult& aRv);
  // Versions of Constructor that we can share with workers and other code.
  static already_AddRefed<URL>
  Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
              const nsAString& aBase, ErrorResult& aRv);
  static already_AddRefed<URL>
  Constructor(nsISupports* aParent, const nsAString& aUrl,
              const nsAString& aBase, ErrorResult& aRv);
  static already_AddRefed<URL>
  Constructor(nsISupports* aParent, const nsAString& aUrl,
              nsIURI* aBase, ErrorResult& aRv);

  static void CreateObjectURL(const GlobalObject& aGlobal,
                              Blob& aBlob,
                              const objectURLOptions& aOptions,
                              nsAString& aResult,
                              ErrorResult& aError);
  static void CreateObjectURL(const GlobalObject& aGlobal,
                              DOMMediaStream& aStream,
                              const objectURLOptions& aOptions,
                              nsAString& aResult,
                              ErrorResult& aError);
  static void CreateObjectURL(const GlobalObject& aGlobal,
                              MediaSource& aSource,
                              const objectURLOptions& aOptions,
                              nsAString& aResult,
                              ErrorResult& aError);
  static void RevokeObjectURL(const GlobalObject& aGlobal,
                              const nsAString& aURL,
                              ErrorResult& aRv);

  void GetHref(nsAString& aHref) const;

  void SetHref(const nsAString& aHref, ErrorResult& aRv);

  void GetOrigin(nsAString& aOrigin) const;

  void GetProtocol(nsAString& aProtocol) const;

  void SetProtocol(const nsAString& aProtocol);

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

  void GetSearch(nsAString& aRetval) const;

  void SetSearch(const nsAString& aArg);

  URLSearchParams* SearchParams();

  void GetHash(nsAString& aRetval) const;

  void SetHash(const nsAString& aArg);

  void Stringify(nsAString& aRetval) const
  {
    GetHref(aRetval);
  }

  // URLSearchParamsObserver
  void URLSearchParamsUpdated(URLSearchParams* aSearchParams) override;

private:
  nsIURI* GetURI() const
  {
    return mURI;
  }

  void CreateSearchParamsIfNeeded();

  void SetSearchInternal(const nsAString& aSearch);

  void UpdateURLSearchParams();

  static void CreateObjectURLInternal(const GlobalObject& aGlobal,
                                      nsISupports* aObject,
                                      const nsACString& aScheme,
                                      const objectURLOptions& aOptions,
                                      nsAString& aResult,
                                      ErrorResult& aError);

  nsCOMPtr<nsISupports> mParent;
  nsCOMPtr<nsIURI> mURI;
  RefPtr<URLSearchParams> mSearchParams;

  friend class mozilla::dom::workers::URLProxy;
};

bool IsChromeURI(nsIURI* aURI);

} // namespace dom
} // namespace mozilla

#endif /* URL_h___ */
