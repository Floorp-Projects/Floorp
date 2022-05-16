/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerchild_h__
#define mozilla_dom_serviceworkerchild_h__

#include "mozilla/dom/PServiceWorkerChild.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla::dom {

class IPCWorkerRef;
class ServiceWorker;

class ServiceWorkerChild final : public PServiceWorkerChild {
  RefPtr<IPCWorkerRef> mIPCWorkerRef;
  ServiceWorker* mOwner;
  bool mTeardownStarted;

  ServiceWorkerChild();

  ~ServiceWorkerChild() = default;

  // PServiceWorkerChild
  void ActorDestroy(ActorDestroyReason aReason) override;

 public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerChild, override);

  static RefPtr<ServiceWorkerChild> Create();

  void SetOwner(ServiceWorker* aOwner);

  void RevokeOwner(ServiceWorker* aOwner);

  void MaybeStartTeardown();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_serviceworkerchild_h__
