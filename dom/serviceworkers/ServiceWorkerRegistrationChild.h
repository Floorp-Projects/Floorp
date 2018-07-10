/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerregistrationchild_h__
#define mozilla_dom_serviceworkerregistrationchild_h__

#include "mozilla/dom/PServiceWorkerRegistrationChild.h"
#include "mozilla/dom/WorkerHolderToken.h"

namespace mozilla {
namespace dom {

class RemoteServiceWorkerRegistrationImpl;

class ServiceWorkerRegistrationChild final : public PServiceWorkerRegistrationChild
                                           , public WorkerHolderToken::Listener
{
  RefPtr<WorkerHolderToken> mWorkerHolderToken;
  RemoteServiceWorkerRegistrationImpl* mOwner;
  bool mTeardownStarted;

  // PServiceWorkerRegistrationChild
  void
  ActorDestroy(ActorDestroyReason aReason) override;

  mozilla::ipc::IPCResult
  RecvUpdateState(const IPCServiceWorkerRegistrationDescriptor& aDescriptor) override;

  // WorkerHolderToken::Listener
  void
  WorkerShuttingDown() override;

public:
  explicit ServiceWorkerRegistrationChild(WorkerHolderToken* aWorkerHolderToken);
  ~ServiceWorkerRegistrationChild() = default;

  void
  SetOwner(RemoteServiceWorkerRegistrationImpl* aOwner);

  void
  RevokeOwner(RemoteServiceWorkerRegistrationImpl* aOwner);

  void
  MaybeStartTeardown();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_serviceworkerregistrationchild_h__
