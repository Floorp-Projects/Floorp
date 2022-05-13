/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerUnregisterCallback_h
#define mozilla_dom_ServiceWorkerUnregisterCallback_h

#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "nsIServiceWorkerManager.h"

namespace mozilla::dom {

class UnregisterCallback final : public nsIServiceWorkerUnregisterCallback {
 public:
  NS_DECL_ISUPPORTS

  UnregisterCallback();

  explicit UnregisterCallback(GenericPromise::Private* aPromise);

  // nsIServiceWorkerUnregisterCallback implementation
  NS_IMETHOD
  UnregisterSucceeded(bool aState) override;

  NS_IMETHOD
  UnregisterFailed() override;

  RefPtr<GenericPromise> Promise() const;

 private:
  ~UnregisterCallback() = default;

  RefPtr<GenericPromise::Private> mPromise;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ServiceWorkerUnregisterCallback_h
