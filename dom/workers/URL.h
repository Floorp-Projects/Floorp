/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
class File;
struct objectURLOptions;
}
}

BEGIN_WORKERS_NAMESPACE

class URLProxy;
class ConstructorRunnable;

class URL final : public mozilla::dom::URLSearchParamsObserver
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

  bool
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector);

  // Methods for WebIDL

  static already_AddRefed<URL>
  Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
              URL& aBase, ErrorResult& aRv);
  static already_AddRefed<URL>
  Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
              const Optional<nsAString>& aBase, ErrorResult& aRv);
  static already_AddRefed<URL>
  Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
              const nsAString& aBase, ErrorResult& aRv);

  static void
  CreateObjectURL(const GlobalObject& aGlobal,
                  File& aArg, const objectURLOptions& aOptions,
                  nsAString& aResult, ErrorResult& aRv);

  static void
  RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aUrl);

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

  void GetSearch(nsAString& aSearch, ErrorResult& aRv) const;

  void SetSearch(const nsAString& aSearch, ErrorResult& aRv);

  URLSearchParams* SearchParams();

  void SetSearchParams(URLSearchParams& aSearchParams);

  void GetHash(nsAString& aHost, ErrorResult& aRv) const;

  void SetHash(const nsAString& aHash, ErrorResult& aRv);

  void Stringify(nsAString& aRetval, ErrorResult& aRv) const
  {
    GetHref(aRetval, aRv);
  }

  // IURLSearchParamsObserver
  void URLSearchParamsUpdated(URLSearchParams* aSearchParams) override;

private:
  URLProxy* GetURLProxy() const
  {
    return mURLProxy;
  }

  static already_AddRefed<URL>
  FinishConstructor(JSContext* aCx, WorkerPrivate* aPrivate,
                    ConstructorRunnable* aRunnable, ErrorResult& aRv);

  void CreateSearchParamsIfNeeded();

  void SetSearchInternal(const nsAString& aSearch);

  void UpdateURLSearchParams();

  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<URLProxy> mURLProxy;
  nsRefPtr<URLSearchParams> mSearchParams;
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_url_h__ */
