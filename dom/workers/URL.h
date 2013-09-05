/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * url, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_url_h__
#define mozilla_dom_workers_url_h__

#include "mozilla/dom/workers/bindings/DOMBindingBase.h"
#include "mozilla/dom/URLBinding.h"

#include "EventTarget.h"

BEGIN_WORKERS_NAMESPACE

class URLProxy;

class URL MOZ_FINAL : public DOMBindingBase
{
public:

  URL(WorkerPrivate* aWorkerPrivate, URLProxy* aURLProxy);
  ~URL();

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

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

  void GetSearch(nsString& aSearch) const;

  void SetSearch(const nsAString& aSearch);

  void GetHash(nsString& aHost) const;

  void SetHash(const nsAString& aHash);

private:
  URLProxy* GetURLProxy() const
  {
    return mURLProxy;
  }

  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<URLProxy> mURLProxy;
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_url_h__ */
