/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URLMainThread_h
#define mozilla_dom_URLMainThread_h

#include "URL.h"

namespace mozilla {
namespace dom {

// The URL implementation for the main-thread
class URLMainThread final : public URL
{
public:
  static already_AddRefed<URLMainThread>
  Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
              const Optional<nsAString>& aBase, ErrorResult& aRv);

  static already_AddRefed<URLMainThread>
  Constructor(nsISupports* aParent, const nsAString& aURL,
              const nsAString& aBase, ErrorResult& aRv);

  static already_AddRefed<URLMainThread>
  Constructor(nsISupports* aParent, const nsAString& aURL, nsIURI* aBase,
              ErrorResult& aRv);

  static void
  CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                  nsAString& aResult, ErrorResult& aRv);

  static void
  CreateObjectURL(const GlobalObject& aGlobal, DOMMediaStream& aStream,
                  nsAString& aResult, ErrorResult& aRv);

  static void
  CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                  nsAString& aResult, ErrorResult& aRv);

  static void
  RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aURL,
                  ErrorResult& aRv);

  static bool
  IsValidURL(const GlobalObject& aGlobal, const nsAString& aURL,
             ErrorResult& aRv);

  URLMainThread(nsISupports* aParent, already_AddRefed<nsIURI> aURI);

  virtual void
  GetHref(nsAString& aHref) const override;

  virtual void
  SetHref(const nsAString& aHref, ErrorResult& aRv) override;

  virtual void
  GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const override;

  virtual void
  GetProtocol(nsAString& aProtocol) const override;

  virtual void
  SetProtocol(const nsAString& aProtocol, ErrorResult& aRv) override;

  virtual void
  GetUsername(nsAString& aUsername) const override;

  virtual void
  SetUsername(const nsAString& aUsername) override;

  virtual void
  GetPassword(nsAString& aPassword) const override;

  virtual void
  SetPassword(const nsAString& aPassword) override;

  virtual void
  GetHost(nsAString& aHost) const override;

  virtual void
  SetHost(const nsAString& aHost) override;

  virtual void
  GetHostname(nsAString& aHostname) const override;

  virtual void
  SetHostname(const nsAString& aHostname) override;

  virtual void
  GetPort(nsAString& aPort) const override;

  virtual void
  SetPort(const nsAString& aPort) override;

  virtual void
  GetPathname(nsAString& aPathname) const override;

  virtual void
  SetPathname(const nsAString& aPathname) override;

  virtual void
  GetSearch(nsAString& aSearch) const override;

  virtual void
  GetHash(nsAString& aHost) const override;

  virtual void
  SetHash(const nsAString& aHash) override;

  virtual void UpdateURLSearchParams() override;

  virtual void
  SetSearchInternal(const nsAString& aSearch) override;

  nsIURI*
  GetURI() const;

private:
  ~URLMainThread();

  nsCOMPtr<nsIURI> mURI;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_URLMainThread_h
