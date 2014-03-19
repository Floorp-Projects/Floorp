/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef URL_h___
#define URL_h___

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/URLSearchParams.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsString.h"

class nsIDOMBlob;
class nsISupports;
class nsIURI;

namespace mozilla {

class ErrorResult;
class DOMMediaStream;

namespace dom {

class MediaSource;
class GlobalObject;
struct objectURLOptions;

namespace workers {
class URLProxy;
}

class URL MOZ_FINAL : public URLSearchParamsObserver
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(URL)

  URL(nsIURI* aURI);

  // WebIDL methods
  nsISupports* GetParentObject() const
  {
    return nullptr;
  }

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope);

  static already_AddRefed<URL>
  Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
              URL& aBase, ErrorResult& aRv);
  static already_AddRefed<URL>
  Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
              const nsAString& aBase, ErrorResult& aRv);

  static void CreateObjectURL(const GlobalObject& aGlobal,
                              nsIDOMBlob* aBlob,
                              const objectURLOptions& aOptions,
                              nsString& aResult,
                              ErrorResult& aError);
  static void CreateObjectURL(const GlobalObject& aGlobal,
                              DOMMediaStream& aStream,
                              const objectURLOptions& aOptions,
                              nsString& aResult,
                              ErrorResult& aError);
  static void CreateObjectURL(const GlobalObject& aGlobal,
                              MediaSource& aSource,
                              const objectURLOptions& aOptions,
                              nsString& aResult,
                              ErrorResult& aError);
  static void RevokeObjectURL(const GlobalObject& aGlobal,
                              const nsAString& aURL);

  void GetHref(nsString& aHref) const;

  void SetHref(const nsAString& aHref, ErrorResult& aRv);

  void GetOrigin(nsString& aOrigin) const;

  void GetProtocol(nsString& aProtocol) const;

  void SetProtocol(const nsAString& aProtocol);

  void GetUsername(nsString& aUsername) const;

  void SetUsername(const nsAString& aUsername);

  void GetPassword(nsString& aPassword) const;

  void SetPassword(const nsAString& aPassword);

  void GetHost(nsString& aHost) const;

  void SetHost(const nsAString& aHost);

  void GetHostname(nsString& aHostname) const;

  void SetHostname(const nsAString& aHostname);

  void GetPort(nsString& aPort) const;

  void SetPort(const nsAString& aPort);

  void GetPathname(nsString& aPathname) const;

  void SetPathname(const nsAString& aPathname);

  void GetSearch(nsString& aRetval) const;

  void SetSearch(const nsAString& aArg);

  URLSearchParams* SearchParams();

  void SetSearchParams(URLSearchParams& aSearchParams);

  void GetHash(nsString& aRetval) const;

  void SetHash(const nsAString& aArg);

  void Stringify(nsString& aRetval) const
  {
    GetHref(aRetval);
  }

  // URLSearchParamsObserver
  void URLSearchParamsUpdated() MOZ_OVERRIDE;

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
                                      nsString& aResult,
                                      ErrorResult& aError);

  nsCOMPtr<nsIURI> mURI;
  nsRefPtr<URLSearchParams> mSearchParams;

  friend class mozilla::dom::workers::URLProxy;
};

bool IsChromeURI(nsIURI* aURI);

}
}

#endif /* URL_h___ */
