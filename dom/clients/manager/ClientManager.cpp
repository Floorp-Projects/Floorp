/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientManager.h"

#include "ClientHandle.h"
#include "ClientManagerChild.h"
#include "ClientManagerOpChild.h"
#include "ClientSource.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ClearOnShutdown.h"  // PastShutdownPhase
#include "mozilla/StaticPrefs_dom.h"
#include "prthread.h"

namespace mozilla::dom {

using mozilla::ipc::BackgroundChild;
using mozilla::ipc::PBackgroundChild;
using mozilla::ipc::PrincipalInfo;

namespace {

const uint32_t kBadThreadLocalIndex = -1;
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
const uint32_t kThreadLocalMagic1 = 0x8d57eea6;
const uint32_t kThreadLocalMagic2 = 0x59f375c9;
#endif

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
uint32_t sClientManagerThreadLocalMagic1 = kThreadLocalMagic1;
#endif

uint32_t sClientManagerThreadLocalIndex = kBadThreadLocalIndex;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
uint32_t sClientManagerThreadLocalMagic2 = kThreadLocalMagic2;
uint32_t sClientManagerThreadLocalIndexDuplicate = kBadThreadLocalIndex;
#endif

}  // anonymous namespace

ClientManager::ClientManager() {
  PBackgroundChild* parentActor =
      BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!parentActor)) {
    Shutdown();
    return;
  }

  ClientManagerChild* actor = ClientManagerChild::Create();
  if (NS_WARN_IF(!actor)) {
    Shutdown();
    return;
  }

  PClientManagerChild* sentActor =
      parentActor->SendPClientManagerConstructor(actor);
  if (NS_WARN_IF(!sentActor)) {
    Shutdown();
    return;
  }
  MOZ_DIAGNOSTIC_ASSERT(sentActor == actor);

  ActivateThing(actor);
}

ClientManager::~ClientManager() {
  NS_ASSERT_OWNINGTHREAD(ClientManager);

  Shutdown();

  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalMagic1 == kThreadLocalMagic1);
  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalMagic2 == kThreadLocalMagic2);
  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalIndex != kBadThreadLocalIndex);
  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalIndex ==
                        sClientManagerThreadLocalIndexDuplicate);
  MOZ_DIAGNOSTIC_ASSERT(this ==
                        PR_GetThreadPrivate(sClientManagerThreadLocalIndex));

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  PRStatus status =
#endif
      PR_SetThreadPrivate(sClientManagerThreadLocalIndex, nullptr);
  MOZ_DIAGNOSTIC_ASSERT(status == PR_SUCCESS);
}

void ClientManager::Shutdown() {
  NS_ASSERT_OWNINGTHREAD(ClientManager);

  if (IsShutdown()) {
    return;
  }

  ShutdownThing();
}

UniquePtr<ClientSource> ClientManager::CreateSourceInternal(
    ClientType aType, nsISerialEventTarget* aEventTarget,
    const PrincipalInfo& aPrincipal) {
  NS_ASSERT_OWNINGTHREAD(ClientManager);

  nsID id;
  nsresult rv = nsID::GenerateUUIDInPlace(id);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If we can't even get a UUID, at least make sure not to use a garbage
    // value.  Instead return a shutdown ClientSource with a zero'd id.
    // This should be exceptionally rare, if it happens at all.
    id.Clear();
    ClientSourceConstructorArgs args(id, aType, aPrincipal, TimeStamp::Now());
    UniquePtr<ClientSource> source(new ClientSource(this, aEventTarget, args));
    source->Shutdown();
    return source;
  }

  ClientSourceConstructorArgs args(id, aType, aPrincipal, TimeStamp::Now());
  UniquePtr<ClientSource> source(new ClientSource(this, aEventTarget, args));

  if (IsShutdown()) {
    source->Shutdown();
    return source;
  }

  source->Activate(GetActor());

  return source;
}

UniquePtr<ClientSource> ClientManager::CreateSourceInternal(
    const ClientInfo& aClientInfo, nsISerialEventTarget* aEventTarget) {
  NS_ASSERT_OWNINGTHREAD(ClientManager);

  ClientSourceConstructorArgs args(aClientInfo.Id(), aClientInfo.Type(),
                                   aClientInfo.PrincipalInfo(),
                                   aClientInfo.CreationTime());
  UniquePtr<ClientSource> source(new ClientSource(this, aEventTarget, args));

  if (IsShutdown()) {
    source->Shutdown();
    return source;
  }

  source->Activate(GetActor());

  return source;
}

already_AddRefed<ClientHandle> ClientManager::CreateHandleInternal(
    const ClientInfo& aClientInfo, nsISerialEventTarget* aSerialEventTarget) {
  NS_ASSERT_OWNINGTHREAD(ClientManager);
  MOZ_DIAGNOSTIC_ASSERT(aSerialEventTarget);

  RefPtr<ClientHandle> handle =
      new ClientHandle(this, aSerialEventTarget, aClientInfo);

  if (IsShutdown()) {
    handle->Shutdown();
    return handle.forget();
  }

  handle->Activate(GetActor());

  return handle.forget();
}

RefPtr<ClientOpPromise> ClientManager::StartOp(
    const ClientOpConstructorArgs& aArgs,
    nsISerialEventTarget* aSerialEventTarget) {
  RefPtr<ClientOpPromise::Private> promise =
      new ClientOpPromise::Private(__func__);

  // Hold a ref to the client until the remote operation completes.  Otherwise
  // the ClientHandle might get de-refed and teardown the actor before we
  // get an answer.
  RefPtr<ClientManager> kungFuGrip = this;

  MaybeExecute(
      [&aArgs, promise, kungFuGrip](ClientManagerChild* aActor) {
        ClientManagerOpChild* actor =
            new ClientManagerOpChild(kungFuGrip, aArgs, promise);
        if (!aActor->SendPClientManagerOpConstructor(actor, aArgs)) {
          // Constructor failure will reject promise via ActorDestroy()
          return;
        }
      },
      [promise] {
        CopyableErrorResult rv;
        rv.ThrowInvalidStateError("Client has been destroyed");
        promise->Reject(rv, __func__);
      });

  return promise;
}

// static
already_AddRefed<ClientManager> ClientManager::GetOrCreateForCurrentThread() {
  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalMagic1 == kThreadLocalMagic1);
  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalMagic2 == kThreadLocalMagic2);
  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalIndex != kBadThreadLocalIndex);
  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalIndex ==
                        sClientManagerThreadLocalIndexDuplicate);
  RefPtr<ClientManager> cm = static_cast<ClientManager*>(
      PR_GetThreadPrivate(sClientManagerThreadLocalIndex));

  if (!cm) {
    cm = new ClientManager();

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    PRStatus status =
#endif
        PR_SetThreadPrivate(sClientManagerThreadLocalIndex, cm.get());
    MOZ_DIAGNOSTIC_ASSERT(status == PR_SUCCESS);
  }

  MOZ_DIAGNOSTIC_ASSERT(cm);

  if (StaticPrefs::dom_workers_testing_enabled()) {
    // Check that the ClientManager instance associated to the current thread
    // has not been kept alive when it was expected to have been already
    // deallocated (e.g. due to a leak ClientManager's mShutdown can have ben
    // set to true from its RevokeActor method but never fully deallocated and
    // unset from the thread locals).
    MOZ_DIAGNOSTIC_ASSERT(!cm->IsShutdown());
  }
  return cm.forget();
}

WorkerPrivate* ClientManager::GetWorkerPrivate() const {
  NS_ASSERT_OWNINGTHREAD(ClientManager);
  MOZ_DIAGNOSTIC_ASSERT(GetActor());
  return GetActor()->GetWorkerPrivate();
}

// Used to share logic between ExpectFutureSource and ForgetFutureSource.
/* static */ bool ClientManager::ExpectOrForgetFutureSource(
    const ClientInfo& aClientInfo,
    bool (PClientManagerChild::*aMethod)(const IPCClientInfo&)) {
  // Return earlier if called late in the XPCOM shutdown path,
  // ClientManager would be already shutdown at the point.
  if (NS_WARN_IF(PastShutdownPhase(ShutdownPhase::XPCOMShutdown))) {
    return false;
  }

  bool rv = true;

  RefPtr<ClientManager> mgr = ClientManager::GetOrCreateForCurrentThread();
  mgr->MaybeExecute(
      [&](ClientManagerChild* aActor) {
        if (!(aActor->*aMethod)(aClientInfo.ToIPC())) {
          rv = false;
        }
      },
      [&] { rv = false; });

  return rv;
}

/* static */ bool ClientManager::ExpectFutureSource(
    const ClientInfo& aClientInfo) {
  return ExpectOrForgetFutureSource(
      aClientInfo, &PClientManagerChild::SendExpectFutureClientSource);
}

/* static */ bool ClientManager::ForgetFutureSource(
    const ClientInfo& aClientInfo) {
  return ExpectOrForgetFutureSource(
      aClientInfo, &PClientManagerChild::SendForgetFutureClientSource);
}

// static
void ClientManager::Startup() {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalMagic1 == kThreadLocalMagic1);
  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalMagic2 == kThreadLocalMagic2);
  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalIndex == kBadThreadLocalIndex);
  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalIndex ==
                        sClientManagerThreadLocalIndexDuplicate);

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  PRStatus status =
#endif
      PR_NewThreadPrivateIndex(&sClientManagerThreadLocalIndex, nullptr);
  MOZ_DIAGNOSTIC_ASSERT(status == PR_SUCCESS);

  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalIndex != kBadThreadLocalIndex);
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  sClientManagerThreadLocalIndexDuplicate = sClientManagerThreadLocalIndex;
#endif
}

// static
UniquePtr<ClientSource> ClientManager::CreateSource(
    ClientType aType, nsISerialEventTarget* aEventTarget,
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  PrincipalInfo principalInfo;
  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_CRASH("ClientManager::CreateSource() cannot serialize bad principal");
  }

  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->CreateSourceInternal(aType, aEventTarget, principalInfo);
}

// static
UniquePtr<ClientSource> ClientManager::CreateSource(
    ClientType aType, nsISerialEventTarget* aEventTarget,
    const PrincipalInfo& aPrincipal) {
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->CreateSourceInternal(aType, aEventTarget, aPrincipal);
}

// static
UniquePtr<ClientSource> ClientManager::CreateSourceFromInfo(
    const ClientInfo& aClientInfo, nsISerialEventTarget* aEventTarget) {
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->CreateSourceInternal(aClientInfo, aEventTarget);
}

Maybe<ClientInfo> ClientManager::CreateInfo(ClientType aType,
                                            nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  PrincipalInfo principalInfo;
  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_CRASH("ClientManager::CreateSource() cannot serialize bad principal");
  }

  nsID id;
  rv = nsID::GenerateUUIDInPlace(id);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return Nothing();
  }

  return Some(ClientInfo(id, aType, principalInfo, TimeStamp::Now()));
}

// static
already_AddRefed<ClientHandle> ClientManager::CreateHandle(
    const ClientInfo& aClientInfo, nsISerialEventTarget* aSerialEventTarget) {
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->CreateHandleInternal(aClientInfo, aSerialEventTarget);
}

// static
RefPtr<ClientOpPromise> ClientManager::MatchAll(
    const ClientMatchAllArgs& aArgs, nsISerialEventTarget* aSerialEventTarget) {
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->StartOp(aArgs, aSerialEventTarget);
}

// static
RefPtr<ClientOpPromise> ClientManager::Claim(
    const ClientClaimArgs& aArgs, nsISerialEventTarget* aSerialEventTarget) {
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->StartOp(aArgs, aSerialEventTarget);
}

// static
RefPtr<ClientOpPromise> ClientManager::GetInfoAndState(
    const ClientGetInfoAndStateArgs& aArgs,
    nsISerialEventTarget* aSerialEventTarget) {
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->StartOp(aArgs, aSerialEventTarget);
}

// static
RefPtr<ClientOpPromise> ClientManager::Navigate(
    const ClientNavigateArgs& aArgs, nsISerialEventTarget* aSerialEventTarget) {
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->StartOp(aArgs, aSerialEventTarget);
}

// static
RefPtr<ClientOpPromise> ClientManager::OpenWindow(
    const ClientOpenWindowArgs& aArgs,
    nsISerialEventTarget* aSerialEventTarget) {
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->StartOp(aArgs, aSerialEventTarget);
}

}  // namespace mozilla::dom
