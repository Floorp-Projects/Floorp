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
  CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                  nsAString& aResult, ErrorResult& aRv);

  static void
  RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aURL,
                  ErrorResult& aRv);

  static bool
  IsValidURL(const GlobalObject& aGlobal, const nsAString& aURL,
             ErrorResult& aRv);

  explicit URLMainThread(nsISupports* aParent);

  virtual void
  SetHref(const nsAString& aHref, ErrorResult& aRv) override;

  virtual void
  GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const override;

  virtual void
  SetProtocol(const nsAString& aProtocol, ErrorResult& aRv) override;

private:
  ~URLMainThread();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_URLMainThread_h
