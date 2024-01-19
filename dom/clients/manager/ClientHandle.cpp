/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientHandle.h"

#include "ClientHandleChild.h"
#include "ClientHandleOpChild.h"
#include "ClientManager.h"
#include "ClientPrincipalUtils.h"
#include "ClientState.h"
#include "mozilla/dom/PClientManagerChild.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"

namespace mozilla::dom {

using mozilla::dom::ipc::StructuredCloneData;

ClientHandle::~ClientHandle() { Shutdown(); }

void ClientHandle::Shutdown() {
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (IsShutdown()) {
    return;
  }

  ShutdownThing();

  mManager = nullptr;
}

void ClientHandle::StartOp(const ClientOpConstructorArgs& aArgs,
                           const ClientOpCallback&& aResolveCallback,
                           const ClientOpCallback&& aRejectCallback) {
  // Hold a ref to the client until the remote operation completes.  Otherwise
  // the ClientHandle might get de-refed and teardown the actor before we
  // get an answer.
  RefPtr<ClientHandle> kungFuGrip = this;

  MaybeExecute(
      [&aArgs, kungFuGrip, aRejectCallback,
       resolve = std::move(aResolveCallback)](ClientHandleChild* aActor) {
        MOZ_DIAGNOSTIC_ASSERT(aActor);
        ClientHandleOpChild* actor = new ClientHandleOpChild(
            kungFuGrip, aArgs, std::move(resolve), std::move(aRejectCallback));
        if (!aActor->SendPClientHandleOpConstructor(actor, aArgs)) {
          // Constructor failure will call reject callback via ActorDestroy()
          return;
        }
      },
      [aRejectCallback] {
        MOZ_DIAGNOSTIC_ASSERT(aRejectCallback);
        CopyableErrorResult rv;
        rv.ThrowInvalidStateError("Client has been destroyed");
        aRejectCallback(rv);
      });
}

void ClientHandle::OnShutdownThing() {
  NS_ASSERT_OWNINGTHREAD(ClientHandle);
  if (!mDetachPromise) {
    return;
  }
  mDetachPromise->Resolve(true, __func__);
}

ClientHandle::ClientHandle(ClientManager* aManager,
                           nsISerialEventTarget* aSerialEventTarget,
                           const ClientInfo& aClientInfo)
    : mManager(aManager),
      mSerialEventTarget(aSerialEventTarget),
      mClientInfo(aClientInfo) {
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  MOZ_DIAGNOSTIC_ASSERT(mSerialEventTarget);
  MOZ_ASSERT(mSerialEventTarget->IsOnCurrentThread());
}

void ClientHandle::Activate(PClientManagerChild* aActor) {
  NS_ASSERT_OWNINGTHREAD(ClientHandle);

  if (IsShutdown()) {
    return;
  }

  RefPtr<ClientHandleChild> actor = new ClientHandleChild();
  if (!aActor->SendPClientHandleConstructor(actor, mClientInfo.ToIPC())) {
    Shutdown();
    return;
  }

  ActivateThing(actor);
}

void ClientHandle::ExecutionReady(const ClientInfo& aClientInfo) {
  mClientInfo = aClientInfo;
}

const ClientInfo& ClientHandle::Info() const { return mClientInfo; }

RefPtr<GenericErrorResultPromise> ClientHandle::Control(
    const ServiceWorkerDescriptor& aServiceWorker) {
  RefPtr<GenericErrorResultPromise::Private> outerPromise =
      new GenericErrorResultPromise::Private(__func__);

  // We should never have a cross-origin controller.  Since this would be
  // same-origin policy violation we do a full release assertion here.
  MOZ_RELEASE_ASSERT(ClientMatchPrincipalInfo(mClientInfo.PrincipalInfo(),
                                              aServiceWorker.PrincipalInfo()));

  StartOp(
      ClientControlledArgs(aServiceWorker.ToIPC()),
      [outerPromise](const ClientOpResult& aResult) {
        outerPromise->Resolve(true, __func__);
      },
      [outerPromise](const ClientOpResult& aResult) {
        outerPromise->Reject(aResult.get_CopyableErrorResult(), __func__);
      });

  return outerPromise;
}

RefPtr<ClientStatePromise> ClientHandle::Focus(CallerType aCallerType) {
  RefPtr<ClientStatePromise::Private> outerPromise =
      new ClientStatePromise::Private(__func__);

  StartOp(
      ClientFocusArgs(aCallerType),
      [outerPromise](const ClientOpResult& aResult) {
        outerPromise->Resolve(
            ClientState::FromIPC(aResult.get_IPCClientState()), __func__);
      },
      [outerPromise](const ClientOpResult& aResult) {
        outerPromise->Reject(aResult.get_CopyableErrorResult(), __func__);
      });

  return outerPromise;
}

RefPtr<GenericErrorResultPromise> ClientHandle::PostMessage(
    StructuredCloneData& aData, const ServiceWorkerDescriptor& aSource) {
  if (IsShutdown()) {
    CopyableErrorResult rv;
    rv.ThrowInvalidStateError("Client has been destroyed");
    return GenericErrorResultPromise::CreateAndReject(rv, __func__);
  }

  ClientPostMessageArgs args;
  args.serviceWorker() = aSource.ToIPC();

  if (!aData.BuildClonedMessageData(args.clonedData())) {
    CopyableErrorResult rv;
    rv.ThrowInvalidStateError("Failed to clone data");
    return GenericErrorResultPromise::CreateAndReject(rv, __func__);
  }

  RefPtr<GenericErrorResultPromise::Private> outerPromise =
      new GenericErrorResultPromise::Private(__func__);

  StartOp(
      std::move(args),
      [outerPromise](const ClientOpResult& aResult) {
        outerPromise->Resolve(true, __func__);
      },
      [outerPromise](const ClientOpResult& aResult) {
        outerPromise->Reject(aResult.get_CopyableErrorResult(), __func__);
      });

  return outerPromise;
}

RefPtr<GenericPromise> ClientHandle::OnDetach() {
  NS_ASSERT_OWNINGTHREAD(ClientSource);

  if (!mDetachPromise) {
    mDetachPromise = new GenericPromise::Private(__func__);
    if (IsShutdown()) {
      mDetachPromise->Resolve(true, __func__);
    }
  }

  return mDetachPromise;
}

void ClientHandle::EvictFromBFCache() {
  ClientEvictBFCacheArgs args;
  StartOp(
      std::move(args), [](const ClientOpResult& aResult) {},
      [](const ClientOpResult& aResult) {});
}

}  // namespace mozilla::dom
