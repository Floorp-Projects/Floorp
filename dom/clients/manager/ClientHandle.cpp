/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientHandle.h"

#include "ClientHandleChild.h"
#include "ClientHandleOpChild.h"
#include "ClientManager.h"
#include "mozilla/dom/PClientManagerChild.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"

namespace mozilla {
namespace dom {

ClientHandle::~ClientHandle()
{
  Shutdown();
}

void
ClientHandle::Shutdown()
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (IsShutdown()) {
    return;
  }

  ShutdownThing();

  mManager = nullptr;
}

already_AddRefed<ClientOpPromise>
ClientHandle::StartOp(const ClientOpConstructorArgs& aArgs)
{
  RefPtr<ClientOpPromise::Private> promise =
    new ClientOpPromise::Private(__func__);

  // Hold a ref to the client until the remote operation completes.  Otherwise
  // the ClientHandle might get de-refed and teardown the actor before we
  // get an answer.
  RefPtr<ClientHandle> kungFuGrip = this;
  promise->Then(mSerialEventTarget, __func__,
                [kungFuGrip] (const ClientOpResult &) { },
                [kungFuGrip] (nsresult) { });

  MaybeExecute([aArgs, promise] (ClientHandleChild* aActor) {
    ClientHandleOpChild* actor = new ClientHandleOpChild(aArgs, promise);
    if (!aActor->SendPClientHandleOpConstructor(actor, aArgs)) {
      // Constructor failure will reject promise via ActorDestroy()
      return;
    }
  });

  RefPtr<ClientOpPromise> ref = promise.get();
  return ref.forget();
}

ClientHandle::ClientHandle(ClientManager* aManager,
                           nsISerialEventTarget* aSerialEventTarget,
                           const ClientInfo& aClientInfo)
  : mManager(aManager)
  , mSerialEventTarget(aSerialEventTarget)
  , mClientInfo(aClientInfo)
{
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  MOZ_DIAGNOSTIC_ASSERT(mSerialEventTarget);
  MOZ_ASSERT(mSerialEventTarget->IsOnCurrentThread());
}

void
ClientHandle::Activate(PClientManagerChild* aActor)
{
  NS_ASSERT_OWNINGTHREAD(ClientHandle);

  if (IsShutdown()) {
    return;
  }

  PClientHandleChild* actor =
    aActor->SendPClientHandleConstructor(mClientInfo.ToIPC());
  if (!actor) {
    Shutdown();
    return;
  }

  ActivateThing(static_cast<ClientHandleChild*>(actor));
}

void
ClientHandle::ExecutionReady(const ClientInfo& aClientInfo)
{
  mClientInfo = aClientInfo;
}

const ClientInfo&
ClientHandle::Info() const
{
  return mClientInfo;
}

RefPtr<GenericPromise>
ClientHandle::Control(const ServiceWorkerDescriptor& aServiceWorker)
{
  RefPtr<GenericPromise::Private> outerPromise =
    new GenericPromise::Private(__func__);

  RefPtr<ClientOpPromise> innerPromise =
    StartOp(ClientControlledArgs(aServiceWorker.ToIPC()));

  innerPromise->Then(mSerialEventTarget, __func__,
    [outerPromise](const ClientOpResult& aResult) {
      outerPromise->Resolve(true, __func__);
    },
    [outerPromise](const ClientOpResult& aResult) {
      outerPromise->Reject(aResult.get_nsresult(), __func__);
    });

  return outerPromise.forget();
}

} // namespace dom
} // namespace mozilla
