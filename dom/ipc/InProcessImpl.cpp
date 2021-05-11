/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/InProcessParent.h"
#include "mozilla/dom/InProcessChild.h"
#include "mozilla/dom/JSProcessActorBinding.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"

using namespace mozilla::ipc;

// This file contains the implementation of core InProcess lifecycle management
// facilities.

namespace mozilla::dom {

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

  // Stash global references to fetch the other side of the reference.
  InProcessParent::sSingleton = std::move(parent);
  InProcessChild::sSingleton = std::move(child);
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
  JSActorDidDestroy();
  InProcessParent::Shutdown();
}

void InProcessChild::ActorDestroy(ActorDestroyReason aWhy) {
  JSActorDidDestroy();
  InProcessParent::Shutdown();
}

/////////////////////////
// nsIDOMProcessParent //
/////////////////////////

NS_IMETHODIMP
InProcessParent::GetChildID(uint64_t* aChildID) {
  *aChildID = 0;
  return NS_OK;
}

NS_IMETHODIMP
InProcessParent::GetOsPid(int32_t* aOsPid) {
  // InProcessParent always run in the parent process,
  // so we can return the current process id.
  *aOsPid = base::GetCurrentProcId();
  return NS_OK;
}

NS_IMETHODIMP InProcessParent::GetRemoteType(nsACString& aRemoteType) {
  aRemoteType = NOT_REMOTE_TYPE;
  return NS_OK;
}

NS_IMETHODIMP
InProcessParent::GetActor(const nsACString& aName, JSContext* aCx,
                          JSProcessActorParent** aActor) {
  ErrorResult error;
  RefPtr<JSProcessActorParent> actor =
      JSActorManager::GetActor(aCx, aName, error)
          .downcast<JSProcessActorParent>();
  if (error.MaybeSetPendingException(aCx)) {
    return NS_ERROR_FAILURE;
  }
  actor.forget(aActor);
  return NS_OK;
}

NS_IMETHODIMP
InProcessParent::GetExistingActor(const nsACString& aName,
                                  JSProcessActorParent** aActor) {
  RefPtr<JSProcessActorParent> actor =
      JSActorManager::GetExistingActor(aName).downcast<JSProcessActorParent>();
  actor.forget(aActor);
  return NS_OK;
}

already_AddRefed<JSActor> InProcessParent::InitJSActor(
    JS::HandleObject aMaybeActor, const nsACString& aName, ErrorResult& aRv) {
  RefPtr<JSProcessActorParent> actor;
  if (aMaybeActor.get()) {
    aRv = UNWRAP_OBJECT(JSProcessActorParent, aMaybeActor.get(), actor);
    if (aRv.Failed()) {
      return nullptr;
    }
  } else {
    actor = new JSProcessActorParent();
  }

  MOZ_RELEASE_ASSERT(!actor->Manager(),
                     "mManager was already initialized once!");
  actor->Init(aName, this);
  return actor.forget();
}

NS_IMETHODIMP
InProcessParent::GetCanSend(bool* aCanSend) {
  *aCanSend = CanSend();
  return NS_OK;
}

ContentParent* InProcessParent::AsContentParent() { return nullptr; }

JSActorManager* InProcessParent::AsJSActorManager() { return this; }

////////////////////////
// nsIDOMProcessChild //
////////////////////////

NS_IMETHODIMP
InProcessChild::GetChildID(uint64_t* aChildID) {
  *aChildID = 0;
  return NS_OK;
}

NS_IMETHODIMP
InProcessChild::GetActor(const nsACString& aName, JSContext* aCx,
                         JSProcessActorChild** aActor) {
  ErrorResult error;
  RefPtr<JSProcessActorChild> actor =
      JSActorManager::GetActor(aCx, aName, error)
          .downcast<JSProcessActorChild>();
  if (error.MaybeSetPendingException(aCx)) {
    return NS_ERROR_FAILURE;
  }
  actor.forget(aActor);
  return NS_OK;
}

NS_IMETHODIMP
InProcessChild::GetExistingActor(const nsACString& aName,
                                 JSProcessActorChild** aActor) {
  RefPtr<JSProcessActorChild> actor =
      JSActorManager::GetExistingActor(aName).downcast<JSProcessActorChild>();
  actor.forget(aActor);
  return NS_OK;
}

already_AddRefed<JSActor> InProcessChild::InitJSActor(
    JS::HandleObject aMaybeActor, const nsACString& aName, ErrorResult& aRv) {
  RefPtr<JSProcessActorChild> actor;
  if (aMaybeActor.get()) {
    aRv = UNWRAP_OBJECT(JSProcessActorChild, aMaybeActor.get(), actor);
    if (aRv.Failed()) {
      return nullptr;
    }
  } else {
    actor = new JSProcessActorChild();
  }

  MOZ_RELEASE_ASSERT(!actor->Manager(),
                     "mManager was already initialized once!");
  actor->Init(aName, this);
  return actor.forget();
}

NS_IMETHODIMP
InProcessChild::GetCanSend(bool* aCanSend) {
  *aCanSend = CanSend();
  return NS_OK;
}

ContentChild* InProcessChild::AsContentChild() { return nullptr; }

JSActorManager* InProcessChild::AsJSActorManager() { return this; }

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

NS_IMPL_ISUPPORTS(InProcessParent, nsIDOMProcessParent, nsIObserver)
NS_IMPL_ISUPPORTS(InProcessChild, nsIDOMProcessChild)

}  // namespace mozilla::dom
