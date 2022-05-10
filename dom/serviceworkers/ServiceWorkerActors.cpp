/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerActors.h"

#include "ServiceWorkerChild.h"
#include "ServiceWorkerContainerChild.h"
#include "ServiceWorkerContainerParent.h"
#include "ServiceWorkerParent.h"
#include "ServiceWorkerRegistrationChild.h"
#include "ServiceWorkerRegistrationParent.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla::dom {

void InitServiceWorkerParent(PServiceWorkerParent* aActor,
                             const IPCServiceWorkerDescriptor& aDescriptor) {
  auto actor = static_cast<ServiceWorkerParent*>(aActor);
  actor->Init(aDescriptor);
}

void InitServiceWorkerContainerParent(PServiceWorkerContainerParent* aActor) {
  auto actor = static_cast<ServiceWorkerContainerParent*>(aActor);
  actor->Init();
}

void InitServiceWorkerRegistrationParent(
    PServiceWorkerRegistrationParent* aActor,
    const IPCServiceWorkerRegistrationDescriptor& aDescriptor) {
  auto actor = static_cast<ServiceWorkerRegistrationParent*>(aActor);
  actor->Init(aDescriptor);
}

}  // namespace mozilla::dom
