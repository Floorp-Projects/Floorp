/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/InProcessParent.h"
#include "mozilla/ipc/InProcessChild.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"

// This file contains the implementation of core InProcess lifecycle management
// facilities.

namespace mozilla {
namespace ipc {

StaticRefPtr<InProcessParent> InProcessParent::sSingleton;
StaticRefPtr<InProcessChild> InProcessChild::sSingleton;
bool InProcessParent::sShutdown = false;

//////////////////////////////////////////
// InProcess actor lifecycle management //
//////////////////////////////////////////

/* static */
InProcessChild* InProcessChild::Singleton() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton) {
    InProcessParent::Startup();
  }
  return sSingleton;
}

/* static */
InProcessParent* InProcessParent::Singleton() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton) {
    InProcessParent::Startup();
  }
  return sSingleton;
}

/* static */
void InProcessParent::Startup() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sShutdown) {
    NS_WARNING("Could not get in-process actor while shutting down!");
    return;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    sShutdown = true;
    NS_WARNING("Failed to get nsIObserverService for in-process actor");
    return;
  }

  RefPtr<InProcessParent> parent = new InProcessParent();
  RefPtr<InProcessChild> child = new InProcessChild();

  // Observe the shutdown event to close & clean up after ourselves.
  nsresult rv = obs->AddObserver(parent, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Link the two actors
  if (!child->OpenOnSameThread(parent->GetIPCChannel(), ChildSide)) {
    MOZ_CRASH("Failed to open InProcessChild!");
  }

  parent->SetOtherProcessId(base::GetCurrentProcId());

  // Create references held by the IPC layer which will be freed in
  // DeallocPInProcess{Parent,Child}.
  parent.get()->AddRef();
  child.get()->AddRef();

  // Stash global references to fetch the other side of the reference.
  InProcessParent::sSingleton = parent.forget();
  InProcessChild::sSingleton = child.forget();
}

/* static */
void InProcessParent::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton || sShutdown) {
    return;
  }

  sShutdown = true;

  RefPtr<InProcessParent> parent = sSingleton;
  InProcessParent::sSingleton = nullptr;
  InProcessChild::sSingleton = nullptr;

  // Calling `Close` on the actor will cause the `Dealloc` methods to be called,
  // freeing the remaining references.
  parent->Close();
}

NS_IMETHODIMP
InProcessParent::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  MOZ_ASSERT(!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID));
  InProcessParent::Shutdown();
  return NS_OK;
}

void InProcessParent::ActorDestroy(ActorDestroyReason aWhy) {
  InProcessParent::Shutdown();
}

void InProcessChild::ActorDestroy(ActorDestroyReason aWhy) {
  InProcessParent::Shutdown();
}

void InProcessParent::ActorDealloc() {
  MOZ_ASSERT(!InProcessParent::sSingleton);
  Release();  // Release the reference taken in InProcessParent::Startup.
}

void InProcessChild::ActorDealloc() {
  MOZ_ASSERT(!InProcessChild::sSingleton);
  Release();  // Release the reference taken in InProcessParent::Startup.
}

////////////////////////////////
// In-Process Actor Utilities //
////////////////////////////////

// Helper method for implementing ParentActorFor and ChildActorFor.
static IProtocol* GetOtherInProcessActor(IProtocol* aActor) {
  MOZ_ASSERT(aActor->GetSide() != UnknownSide, "bad unknown side");

  // Discover the manager of aActor which is PInProcess.
  IProtocol* current = aActor;
  while (current) {
    if (current->GetProtocolId() == PInProcessMsgStart) {
      break;  // Found the correct actor.
    }
    current = current->Manager();
  }
  if (!current) {
    return nullptr;  // Not a PInProcess actor, return |nullptr|
  }

  MOZ_ASSERT(current->GetSide() == aActor->GetSide(), "side changed?");
  MOZ_ASSERT_IF(aActor->GetSide() == ParentSide,
                current == InProcessParent::Singleton());
  MOZ_ASSERT_IF(aActor->GetSide() == ChildSide,
                current == InProcessChild::Singleton());

  // Check whether this is InProcessParent or InProcessChild, and get the other
  // side's toplevel actor.
  IProtocol* otherRoot = nullptr;
  if (aActor->GetSide() == ParentSide) {
    otherRoot = InProcessChild::Singleton();
  } else {
    otherRoot = InProcessParent::Singleton();
  }
  if (NS_WARN_IF(!otherRoot)) {
    return nullptr;
  }

  // Look up the actor on the other side, and return it.
  IProtocol* otherActor = otherRoot->Lookup(aActor->Id());
  if (otherActor) {
    MOZ_ASSERT(otherActor->GetSide() != UnknownSide, "bad unknown side");
    MOZ_ASSERT(otherActor->GetSide() != aActor->GetSide(), "Wrong side!");
    MOZ_ASSERT(otherActor->GetProtocolId() == aActor->GetProtocolId(),
               "Wrong type of protocol!");
  }

  return otherActor;
}

/* static */
IProtocol* InProcessParent::ChildActorFor(IProtocol* aActor) {
  MOZ_ASSERT(aActor && aActor->GetSide() == ParentSide);
  return GetOtherInProcessActor(aActor);
}

/* static */
IProtocol* InProcessChild::ParentActorFor(IProtocol* aActor) {
  MOZ_ASSERT(aActor && aActor->GetSide() == ChildSide);
  return GetOtherInProcessActor(aActor);
}

NS_IMPL_ISUPPORTS(InProcessParent, nsIObserver)

}  // namespace ipc
}  // namespace mozilla
