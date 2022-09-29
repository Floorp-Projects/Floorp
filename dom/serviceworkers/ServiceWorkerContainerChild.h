/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkercontainerchild_h__
#define mozilla_dom_serviceworkercontainerchild_h__

#include "mozilla/dom/PServiceWorkerContainerChild.h"

// XXX Avoid including this here by moving function bodies to the cpp file
#include "mozilla/dom/WorkerRef.h"

namespace mozilla::dom {

class ServiceWorkerContainer;

class IPCWorkerRef;

class ServiceWorkerContainerChild final : public PServiceWorkerContainerChild {
  RefPtr<IPCWorkerRef> mIPCWorkerRef;
  ServiceWorkerContainer* mOwner;
  bool mTeardownStarted;

  ServiceWorkerContainerChild();

  ~ServiceWorkerContainerChild() = default;

  // PServiceWorkerContainerChild
  void ActorDestroy(ActorDestroyReason aReason) override;

 public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerContainerChild, override);

  static already_AddRefed<ServiceWorkerContainerChild> Create();

  void SetOwner(ServiceWorkerContainer* aOwner);

  void RevokeOwner(ServiceWorkerContainer* aOwner);

  void MaybeStartTeardown();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_serviceworkercontainerchild_h__
