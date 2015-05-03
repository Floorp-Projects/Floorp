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
#include "nsAutoPtr.h"
#include "nsString.h"

class nsISupports;
class nsIURI;

namespace mozilla {

class ErrorResult;
class DOMMediaStream;

namespace dom {

class File;
class MediaSource;
class GlobalObject;
struct objectURLOptions;

namespace workers {
class URLProxy;
}

class URL final : public URLSearchParamsObserver
{
  ~URL() {}

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(URL)

  explicit URL(nsIURI* aURI);

  // WebIDL methods
  bool
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector);

  static already_AddRefed<URL>
  Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
              URL& aBase, ErrorResult& aRv);
  static already_AddRefed<URL>
  Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
              const nsAString& aBase, ErrorResult& aRv);

  static void CreateObjectURL(const GlobalObject& aGlobal,
                              File& aBlob,
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
                              const nsAString& aURL);

  void GetHref(nsAString& aHref, ErrorResult& aRv) const;

  void SetHref(const nsAString& aHref, ErrorResult& aRv);

  void GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const;

  void GetProtocol(nsAString& aProtocol, ErrorResult& aRv) const;

  void SetProtocol(const nsAString& aProtocol, ErrorResult& aRv);

  void GetUsername(nsAString& aUsername, ErrorResult& aRv) const;

  void SetUsername(const nsAString& aUsername, ErrorResult& aRv);

  void GetPassword(nsAString& aPassword, ErrorResult& aRv) const;

  void SetPassword(const nsAString& aPassword, ErrorResult& aRv);

  void GetHost(nsAString& aHost, ErrorResult& aRv) const;

  void SetHost(const nsAString& aHost, ErrorResult& aRv);

  void GetHostname(nsAString& aHostname, ErrorResult& aRv) const;

  void SetHostname(const nsAString& aHostname, ErrorResult& aRv);

  void GetPort(nsAString& aPort, ErrorResult& aRv) const;

  void SetPort(const nsAString& aPort, ErrorResult& aRv);

  void GetPathname(nsAString& aPathname, ErrorResult& aRv) const;

  void SetPathname(const nsAString& aPathname, ErrorResult& aRv);

  void GetSearch(nsAString& aRetval, ErrorResult& aRv) const;

  void SetSearch(const nsAString& aArg, ErrorResult& aRv);

  URLSearchParams* SearchParams();

  void SetSearchParams(URLSearchParams& aSearchParams);

  void GetHash(nsAString& aRetval, ErrorResult& aRv) const;

  void SetHash(const nsAString& aArg, ErrorResult& aRv);

  void Stringify(nsAString& aRetval, ErrorResult& aRv) const
  {
    GetHref(aRetval, aRv);
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

  nsCOMPtr<nsIURI> mURI;
  nsRefPtr<URLSearchParams> mSearchParams;

  friend class mozilla::dom::workers::URLProxy;
};

bool IsChromeURI(nsIURI* aURI);

}
}

#endif /* URL_h___ */
