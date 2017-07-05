/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URLWorker_h
#define mozilla_dom_URLWorker_h

#include "URL.h"
#include "URLMainThread.h"

namespace mozilla {
namespace dom {

namespace workers {
class WorkerPrivate;
}

// URLWorker implements the URL object in workers.
class URLWorker final : public URL
{
public:
  class URLProxy;

  static already_AddRefed<URLWorker>
  Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
              const Optional<nsAString>& aBase, ErrorResult& aRv);

  static already_AddRefed<URLWorker>
  Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
              const nsAString& aBase, ErrorResult& aRv);

  static void
  CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                  nsAString& aResult, mozilla::ErrorResult& aRv);

  static void
  RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aUrl,
                  ErrorResult& aRv);

  static bool
  IsValidURL(const GlobalObject& aGlobal, const nsAString& aUrl,
             ErrorResult& aRv);

  URLWorker(workers::WorkerPrivate* aWorkerPrivate, URLProxy* aURLProxy);

  virtual void
  GetHref(nsAString& aHref, ErrorResult& aRv) const override;

  virtual void
  SetHref(const nsAString& aHref, ErrorResult& aRv) override;

  virtual void
  GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const override;

  virtual void
  GetProtocol(nsAString& aProtocol, ErrorResult& aRv) const override;

  virtual void
  SetProtocol(const nsAString& aProtocol, ErrorResult& aRv) override;

  virtual void
  GetUsername(nsAString& aUsername, ErrorResult& aRv) const override;

  virtual void
  SetUsername(const nsAString& aUsername, ErrorResult& aRv) override;

  virtual void
  GetPassword(nsAString& aPassword, ErrorResult& aRv) const override;

  virtual void
  SetPassword(const nsAString& aPassword, ErrorResult& aRv) override;

  virtual void
  GetHost(nsAString& aHost, ErrorResult& aRv) const override;

  virtual void
  SetHost(const nsAString& aHost, ErrorResult& aRv) override;

  virtual void
  GetHostname(nsAString& aHostname, ErrorResult& aRv) const override;

  virtual void
  SetHostname(const nsAString& aHostname, ErrorResult& aRv) override;

  virtual void
  GetPort(nsAString& aPort, ErrorResult& aRv) const override;

  virtual void
  SetPort(const nsAString& aPort, ErrorResult& aRv) override;

  virtual void
  GetPathname(nsAString& aPathname, ErrorResult& aRv) const override;

  virtual void
  SetPathname(const nsAString& aPathname, ErrorResult& aRv) override;

  virtual void
  GetSearch(nsAString& aSearch, ErrorResult& aRv) const override;

  virtual void
  GetHash(nsAString& aHost, ErrorResult& aRv) const override;

  virtual void
  SetHash(const nsAString& aHash, ErrorResult& aRv) override;

  virtual void UpdateURLSearchParams() override;

  virtual void
  SetSearchInternal(const nsAString& aSearch, ErrorResult& aRv) override;

  URLProxy*
  GetURLProxy() const;

private:
  ~URLWorker();

  workers::WorkerPrivate* mWorkerPrivate;
  RefPtr<URLProxy> mURLProxy;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_URLWorker_h
