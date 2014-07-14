/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * url, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_url_h__
#define mozilla_dom_workers_url_h__

#include "Workers.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/URLSearchParams.h"

namespace mozilla {
namespace dom {
struct objectURLOptions;
}
}

BEGIN_WORKERS_NAMESPACE

class URLProxy;

class URL MOZ_FINAL : public mozilla::dom::URLSearchParamsObserver
{
  typedef mozilla::dom::URLSearchParams URLSearchParams;

  ~URL();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(URL)

  URL(WorkerPrivate* aWorkerPrivate, URLProxy* aURLProxy);

  nsISupports*
  GetParentObject() const
  {
    // There's only one global on a worker, so we don't need to specify.
    return nullptr;
  }

  JSObject*
  WrapObject(JSContext* aCx);

  // Methods for WebIDL

  static URL*
  Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
              URL& aBase, ErrorResult& aRv);
  static URL*
  Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
              const nsAString& aBase, ErrorResult& aRv);

  static void
  CreateObjectURL(const GlobalObject& aGlobal,
                  JSObject* aArg, const objectURLOptions& aOptions,
                  nsString& aResult, ErrorResult& aRv);

  static void
  CreateObjectURL(const GlobalObject& aGlobal,
                  JSObject& aArg, const objectURLOptions& aOptions,
                  nsString& aResult, ErrorResult& aRv);

  static void
  RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aUrl);

  void GetHref(nsString& aHref, ErrorResult& aRv) const;

  void SetHref(const nsAString& aHref, ErrorResult& aRv);

  void GetOrigin(nsString& aOrigin, ErrorResult& aRv) const;

  void GetProtocol(nsString& aProtocol, ErrorResult& aRv) const;

  void SetProtocol(const nsAString& aProtocol, ErrorResult& aRv);

  void GetUsername(nsString& aUsername, ErrorResult& aRv) const;

  void SetUsername(const nsAString& aUsername, ErrorResult& aRv);

  void GetPassword(nsString& aPassword, ErrorResult& aRv) const;

  void SetPassword(const nsAString& aPassword, ErrorResult& aRv);

  void GetHost(nsString& aHost, ErrorResult& aRv) const;

  void SetHost(const nsAString& aHost, ErrorResult& aRv);

  void GetHostname(nsString& aHostname, ErrorResult& aRv) const;

  void SetHostname(const nsAString& aHostname, ErrorResult& aRv);

  void GetPort(nsString& aPort, ErrorResult& aRv) const;

  void SetPort(const nsAString& aPort, ErrorResult& aRv);

  void GetPathname(nsString& aPathname, ErrorResult& aRv) const;

  void SetPathname(const nsAString& aPathname, ErrorResult& aRv);

  void GetSearch(nsString& aSearch, ErrorResult& aRv) const;

  void SetSearch(const nsAString& aSearch, ErrorResult& aRv);

  URLSearchParams* SearchParams();

  void SetSearchParams(URLSearchParams& aSearchParams);

  void GetHash(nsString& aHost, ErrorResult& aRv) const;

  void SetHash(const nsAString& aHash, ErrorResult& aRv);

  void Stringify(nsString& aRetval, ErrorResult& aRv) const
  {
    GetHref(aRetval, aRv);
  }

  // IURLSearchParamsObserver
  void URLSearchParamsUpdated() MOZ_OVERRIDE;

private:
  URLProxy* GetURLProxy() const
  {
    return mURLProxy;
  }

  void CreateSearchParamsIfNeeded();

  void SetSearchInternal(const nsAString& aSearch);

  void UpdateURLSearchParams();

  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<URLProxy> mURLProxy;
  nsRefPtr<URLSearchParams> mSearchParams;
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_url_h__ */
