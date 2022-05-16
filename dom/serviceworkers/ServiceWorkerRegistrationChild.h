/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerregistrationchild_h__
#define mozilla_dom_serviceworkerregistrationchild_h__

#include "mozilla/dom/PServiceWorkerRegistrationChild.h"

// XXX Avoid including this here by moving function bodies to the cpp file
#include "mozilla/dom/WorkerRef.h"

namespace mozilla::dom {

class IPCWorkerRef;
class ServiceWorkerRegistration;

class ServiceWorkerRegistrationChild final
    : public PServiceWorkerRegistrationChild {
  RefPtr<IPCWorkerRef> mIPCWorkerRef;
  ServiceWorkerRegistration* mOwner;
  bool mTeardownStarted;

  ServiceWorkerRegistrationChild();

  ~ServiceWorkerRegistrationChild() = default;

  // PServiceWorkerRegistrationChild
  void ActorDestroy(ActorDestroyReason aReason) override;

  mozilla::ipc::IPCResult RecvUpdateState(
      const IPCServiceWorkerRegistrationDescriptor& aDescriptor) override;

  mozilla::ipc::IPCResult RecvFireUpdateFound() override;

 public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerRegistrationChild, override);

  static RefPtr<ServiceWorkerRegistrationChild> Create();

  void SetOwner(ServiceWorkerRegistration* aOwner);

  void RevokeOwner(ServiceWorkerRegistration* aOwner);

  void MaybeStartTeardown();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_serviceworkerregistrationchild_h__
