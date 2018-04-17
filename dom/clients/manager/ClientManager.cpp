/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientManager.h"

#include "ClientHandle.h"
#include "ClientManagerChild.h"
#include "ClientManagerOpChild.h"
#include "ClientPrefs.h"
#include "ClientSource.h"
#include "mozilla/dom/WorkerHolderToken.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsContentUtils.h"
#include "prthread.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::BackgroundChild;
using mozilla::ipc::PBackgroundChild;
using mozilla::ipc::PrincipalInfo;

namespace {

uint32_t kBadThreadLocalIndex = -1;
uint32_t sClientManagerThreadLocalIndex = kBadThreadLocalIndex;

} // anonymous namespace

ClientManager::ClientManager()
{
  PBackgroundChild* parentActor = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!parentActor)) {
    Shutdown();
    return;
  }

  RefPtr<WorkerHolderToken> workerHolderToken;
  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_DIAGNOSTIC_ASSERT(workerPrivate);

    workerHolderToken =
      WorkerHolderToken::Create(workerPrivate, Terminating,
                                WorkerHolderToken::AllowIdleShutdownStart);
    if (NS_WARN_IF(!workerHolderToken)) {
      Shutdown();
      return;
    }
  }

  ClientManagerChild* actor = new ClientManagerChild(workerHolderToken);
  PClientManagerChild *sentActor =
    parentActor->SendPClientManagerConstructor(actor);
  if (NS_WARN_IF(!sentActor)) {
    Shutdown();
    return;
  }
  MOZ_DIAGNOSTIC_ASSERT(sentActor == actor);

  ActivateThing(actor);
}

ClientManager::~ClientManager()
{
  NS_ASSERT_OWNINGTHREAD(ClientManager);

  Shutdown();

  MOZ_DIAGNOSTIC_ASSERT(this == PR_GetThreadPrivate(sClientManagerThreadLocalIndex));

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  PRStatus status =
#endif
    PR_SetThreadPrivate(sClientManagerThreadLocalIndex, nullptr);
  MOZ_DIAGNOSTIC_ASSERT(status == PR_SUCCESS);
}

void
ClientManager::Shutdown()
{
  NS_ASSERT_OWNINGTHREAD(ClientManager);

  if (IsShutdown()) {
    return;
  }

  ShutdownThing();
}

UniquePtr<ClientSource>
ClientManager::CreateSourceInternal(ClientType aType,
                                    nsISerialEventTarget* aEventTarget,
                                    const PrincipalInfo& aPrincipal)
{
  NS_ASSERT_OWNINGTHREAD(ClientManager);

  nsID id;
  nsresult rv = nsContentUtils::GenerateUUIDInPlace(id);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If we can't even get a UUID, at least make sure not to use a garbage
    // value.  Instead return a shutdown ClientSource with a zero'd id.
    // This should be exceptionally rare, if it happens at all.
    id.Clear();
    ClientSourceConstructorArgs args(id, aType, aPrincipal, TimeStamp::Now());
    UniquePtr<ClientSource> source(new ClientSource(this, aEventTarget, args));
    source->Shutdown();
    return Move(source);
  }

  ClientSourceConstructorArgs args(id, aType, aPrincipal, TimeStamp::Now());
  UniquePtr<ClientSource> source(new ClientSource(this, aEventTarget, args));

  if (IsShutdown()) {
    source->Shutdown();
    return Move(source);
  }

  source->Activate(GetActor());

  return Move(source);
}

already_AddRefed<ClientHandle>
ClientManager::CreateHandleInternal(const ClientInfo& aClientInfo,
                                    nsISerialEventTarget* aSerialEventTarget)
{
  NS_ASSERT_OWNINGTHREAD(ClientManager);
  MOZ_DIAGNOSTIC_ASSERT(aSerialEventTarget);

  RefPtr<ClientHandle> handle = new ClientHandle(this, aSerialEventTarget,
                                                 aClientInfo);

  if (IsShutdown()) {
    handle->Shutdown();
    return handle.forget();
  }

  handle->Activate(GetActor());

  return handle.forget();
}

already_AddRefed<ClientOpPromise>
ClientManager::StartOp(const ClientOpConstructorArgs& aArgs,
                       nsISerialEventTarget* aSerialEventTarget)
{
  RefPtr<ClientOpPromise::Private> promise =
    new ClientOpPromise::Private(__func__);

  // Hold a ref to the client until the remote operation completes.  Otherwise
  // the ClientHandle might get de-refed and teardown the actor before we
  // get an answer.
  RefPtr<ClientManager> kungFuGrip = this;
  promise->Then(aSerialEventTarget, __func__,
                [kungFuGrip] (const ClientOpResult&) { },
                [kungFuGrip] (nsresult) { });

  MaybeExecute([aArgs, promise] (ClientManagerChild* aActor) {
    ClientManagerOpChild* actor = new ClientManagerOpChild(aArgs, promise);
    if (!aActor->SendPClientManagerOpConstructor(actor, aArgs)) {
      // Constructor failure will reject promise via ActorDestroy()
      return;
    }
  }, [promise] {
    promise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
  });

  RefPtr<ClientOpPromise> ref = promise.get();
  return ref.forget();
}

// static
already_AddRefed<ClientManager>
ClientManager::GetOrCreateForCurrentThread()
{
  MOZ_DIAGNOSTIC_ASSERT(sClientManagerThreadLocalIndex != kBadThreadLocalIndex);
  RefPtr<ClientManager> cm =
    static_cast<ClientManager*>(PR_GetThreadPrivate(sClientManagerThreadLocalIndex));

  if (!cm) {
    cm = new ClientManager();

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    PRStatus status =
#endif
      PR_SetThreadPrivate(sClientManagerThreadLocalIndex, cm.get());
    MOZ_DIAGNOSTIC_ASSERT(status == PR_SUCCESS);
  }

  MOZ_ASSERT(cm);
  return cm.forget();
}

WorkerPrivate*
ClientManager::GetWorkerPrivate() const
{
  NS_ASSERT_OWNINGTHREAD(ClientManager);
  MOZ_DIAGNOSTIC_ASSERT(GetActor());
  return GetActor()->GetWorkerPrivate();
}

// static
void
ClientManager::Startup()
{
  MOZ_ASSERT(NS_IsMainThread());
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  PRStatus status =
#endif
    PR_NewThreadPrivateIndex(&sClientManagerThreadLocalIndex, nullptr);
  MOZ_DIAGNOSTIC_ASSERT(status == PR_SUCCESS);

  ClientPrefsInit();
}

// static
UniquePtr<ClientSource>
ClientManager::CreateSource(ClientType aType, nsISerialEventTarget* aEventTarget,
                            nsIPrincipal* aPrincipal)
{
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
UniquePtr<ClientSource>
ClientManager::CreateSource(ClientType aType, nsISerialEventTarget* aEventTarget,
                            const PrincipalInfo& aPrincipal)
{
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->CreateSourceInternal(aType, aEventTarget, aPrincipal);
}

// static
already_AddRefed<ClientHandle>
ClientManager::CreateHandle(const ClientInfo& aClientInfo,
                            nsISerialEventTarget* aSerialEventTarget)
{
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->CreateHandleInternal(aClientInfo, aSerialEventTarget);
}

// static
RefPtr<ClientOpPromise>
ClientManager::MatchAll(const ClientMatchAllArgs& aArgs,
                        nsISerialEventTarget* aSerialEventTarget)
{
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->StartOp(aArgs, aSerialEventTarget);
}

// static
RefPtr<ClientOpPromise>
ClientManager::Claim(const ClientClaimArgs& aArgs,
                     nsISerialEventTarget* aSerialEventTarget)
{
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->StartOp(aArgs, aSerialEventTarget);
}

// static
RefPtr<ClientOpPromise>
ClientManager::GetInfoAndState(const ClientGetInfoAndStateArgs& aArgs,
                               nsISerialEventTarget* aSerialEventTarget)
{
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->StartOp(aArgs, aSerialEventTarget);
}

// static
RefPtr<ClientOpPromise>
ClientManager::Navigate(const ClientNavigateArgs& aArgs,
                        nsISerialEventTarget* aSerialEventTarget)
{
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->StartOp(aArgs, aSerialEventTarget);
}

// static
RefPtr<ClientOpPromise>
ClientManager::OpenWindow(const ClientOpenWindowArgs& aArgs,
                          nsISerialEventTarget* aSerialEventTarget)
{
  RefPtr<ClientManager> mgr = GetOrCreateForCurrentThread();
  return mgr->StartOp(aArgs, aSerialEventTarget);
}

} // namespace dom
} // namespace mozilla
