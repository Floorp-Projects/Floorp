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

namespace mozilla {
namespace dom {

PServiceWorkerChild*
AllocServiceWorkerChild()
{
  MOZ_CRASH("should not be called");
}

bool
DeallocServiceWorkerChild(PServiceWorkerChild* aActor)
{
  auto actor = static_cast<ServiceWorkerChild*>(aActor);
  delete actor;
  return true;
}

PServiceWorkerParent*
AllocServiceWorkerParent()
{
  return new ServiceWorkerParent();
}

bool
DeallocServiceWorkerParent(PServiceWorkerParent* aActor)
{
  auto actor = static_cast<ServiceWorkerParent*>(aActor);
  delete actor;
  return true;
}

void
InitServiceWorkerParent(PServiceWorkerParent* aActor,
                        const IPCServiceWorkerDescriptor& aDescriptor)
{
  auto actor = static_cast<ServiceWorkerParent*>(aActor);
  actor->Init(aDescriptor);
}

PServiceWorkerContainerChild*
AllocServiceWorkerContainerChild()
{
  MOZ_CRASH("should not be called");
}

bool
DeallocServiceWorkerContainerChild(PServiceWorkerContainerChild* aActor)
{
  auto actor = static_cast<ServiceWorkerContainerChild*>(aActor);
  delete actor;
  return true;
}

PServiceWorkerContainerParent*
AllocServiceWorkerContainerParent()
{
  return new ServiceWorkerContainerParent();
}

bool
DeallocServiceWorkerContainerParent(PServiceWorkerContainerParent* aActor)
{
  auto actor = static_cast<ServiceWorkerContainerParent*>(aActor);
  delete actor;
  return true;
}

void
InitServiceWorkerContainerParent(PServiceWorkerContainerParent* aActor)
{
  auto actor = static_cast<ServiceWorkerContainerParent*>(aActor);
  actor->Init();
}

PServiceWorkerRegistrationChild*
AllocServiceWorkerRegistrationChild()
{
  MOZ_CRASH("should not be called");
}

bool
DeallocServiceWorkerRegistrationChild(PServiceWorkerRegistrationChild* aActor)
{
  auto actor = static_cast<ServiceWorkerRegistrationChild*>(aActor);
  delete actor;
  return true;
}

PServiceWorkerRegistrationParent*
AllocServiceWorkerRegistrationParent()
{
  return new ServiceWorkerRegistrationParent();
}

bool
DeallocServiceWorkerRegistrationParent(PServiceWorkerRegistrationParent* aActor)
{
  auto actor = static_cast<ServiceWorkerRegistrationParent*>(aActor);
  delete actor;
  return true;
}

void
InitServiceWorkerRegistrationParent(PServiceWorkerRegistrationParent* aActor,
                                    const IPCServiceWorkerRegistrationDescriptor& aDescriptor)
{
  auto actor = static_cast<ServiceWorkerRegistrationParent*>(aActor);
  actor->Init(aDescriptor);
}

} // namespace dom
} // namespace mozilla
