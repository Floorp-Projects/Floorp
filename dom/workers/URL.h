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
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {
class Blob;
struct objectURLOptions;
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class URLProxy;
class ConstructorRunnable;

class URL final : public mozilla::dom::URLSearchParamsObserver
                , public nsWrapperCache
{
  typedef mozilla::dom::URLSearchParams URLSearchParams;

  ~URL();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(URL)

  URL(WorkerPrivate* aWorkerPrivate, URLProxy* aURLProxy);

  nsISupports*
  GetParentObject() const
  {
    // There's only one global on a worker, so we don't need to specify.
    return nullptr;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

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
                  Blob& aArg, const objectURLOptions& aOptions,
                  nsAString& aResult, ErrorResult& aRv);

  static void
  RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aUrl,
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

  void GetSearch(nsAString& aSearch) const;

  void SetSearch(const nsAString& aSearch);

  URLSearchParams* SearchParams();

  void GetHash(nsAString& aHost) const;

  void SetHash(const nsAString& aHash);

  void Stringify(nsAString& aRetval) const
  {
    GetHref(aRetval);
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
  RefPtr<URLProxy> mURLProxy;
  RefPtr<URLSearchParams> mSearchParams;
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_url_h__ */
