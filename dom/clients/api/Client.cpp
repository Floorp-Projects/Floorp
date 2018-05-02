/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Client.h"

#include "ClientDOMUtil.h"
#include "mozilla/dom/ClientHandle.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/ClientManager.h"
#include "mozilla/dom/ClientState.h"
#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace dom {

using mozilla::dom::ipc::StructuredCloneData;

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozilla::dom::Client);
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozilla::dom::Client);
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(mozilla::dom::Client, mGlobal);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(mozilla::dom::Client)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

void
Client::EnsureHandle()
{
  NS_ASSERT_OWNINGTHREAD(mozilla::dom::Client);
  if (!mHandle) {
    mHandle = ClientManager::CreateHandle(ClientInfo(mData->info()),
                                          mGlobal->EventTargetFor(TaskCategory::Other));
  }
}

Client::Client(nsIGlobalObject* aGlobal, const ClientInfoAndState& aData)
  : mGlobal(aGlobal)
  , mData(MakeUnique<ClientInfoAndState>(aData))
{
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
}

TimeStamp
Client::CreationTime() const
{
  return mData->info().creationTime();
}

TimeStamp
Client::LastFocusTime() const
{
  if (mData->info().type() != ClientType::Window) {
    return TimeStamp();
  }
  return mData->state().get_IPCClientWindowState().lastFocusTime();
}

nsContentUtils::StorageAccess
Client::GetStorageAccess() const
{
  ClientState state(ClientState::FromIPC(mData->state()));
  return state.GetStorageAccess();
}

JSObject*
Client::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  if (mData->info().type() == ClientType::Window) {
    return WindowClientBinding::Wrap(aCx, this, aGivenProto);
  }
  return ClientBinding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject*
Client::GetParentObject() const
{
  return mGlobal;
}

void
Client::GetUrl(nsAString& aUrlOut) const
{
  CopyUTF8toUTF16(mData->info().url(), aUrlOut);
}

void
Client::GetId(nsAString& aIdOut) const
{
  char buf[NSID_LENGTH];
  mData->info().id().ToProvidedString(buf);
  NS_ConvertASCIItoUTF16 uuid(buf);

  // Remove {} and the null terminator
  aIdOut.Assign(Substring(uuid, 1, NSID_LENGTH - 3));
}

ClientType
Client::Type() const
{
  return mData->info().type();
}

FrameType
Client::GetFrameType() const
{
  return mData->info().frameType();
}

void
Client::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                    const Sequence<JSObject*>& aTransferable,
                    ErrorResult& aRv)
{
  MOZ_ASSERT(!NS_IsMainThread());
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate->IsServiceWorker());
  workerPrivate->AssertIsOnWorkerThread();

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());
  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransferable,
                                                          &transferable);
  if (aRv.Failed()) {
    return;
  }

  StructuredCloneData data;
  data.Write(aCx, aMessage, transferable, aRv);
  if (aRv.Failed()) {
    return;
  }

  EnsureHandle();
  mHandle->PostMessage(data, workerPrivate->GetServiceWorkerDescriptor());
}

VisibilityState
Client::GetVisibilityState() const
{
  return mData->state().get_IPCClientWindowState().visibilityState();
}

bool
Client::Focused() const
{
  return mData->state().get_IPCClientWindowState().focused();
}

already_AddRefed<Promise>
Client::Focus(ErrorResult& aRv)
{
  MOZ_ASSERT(!NS_IsMainThread());
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate->IsServiceWorker());
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<Promise> outerPromise = Promise::Create(mGlobal, aRv);
  if (aRv.Failed()) {
    return outerPromise.forget();
  }

  if (!workerPrivate->GlobalScope()->WindowInteractionAllowed()) {
    outerPromise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return outerPromise.forget();
  }

  EnsureHandle();

  IPCClientInfo ipcClientInfo(mData->info());
  auto holder = MakeRefPtr<DOMMozPromiseRequestHolder<ClientStatePromise>>(mGlobal);

  mHandle->Focus()->Then(mGlobal->EventTargetFor(TaskCategory::Other), __func__,
    [ipcClientInfo, holder, outerPromise] (const ClientState& aResult) {
      holder->Complete();
      NS_ENSURE_TRUE_VOID(holder->GetParentObject());
      RefPtr<Client> newClient = new Client(holder->GetParentObject(),
                                            ClientInfoAndState(ipcClientInfo,
                                                               aResult.ToIPC()));
      outerPromise->MaybeResolve(newClient);
    }, [holder, outerPromise] (nsresult aResult) {
      holder->Complete();
      outerPromise->MaybeReject(aResult);
    })->Track(*holder);

  return outerPromise.forget();
}

already_AddRefed<Promise>
Client::Navigate(const nsAString& aURL, ErrorResult& aRv)
{
  MOZ_ASSERT(!NS_IsMainThread());
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate->IsServiceWorker());
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<Promise> outerPromise = Promise::Create(mGlobal, aRv);
  if (aRv.Failed()) {
    return outerPromise.forget();
  }

  ClientNavigateArgs args(mData->info(), NS_ConvertUTF16toUTF8(aURL),
                          workerPrivate->GetLocationInfo().mHref);
  RefPtr<Client> self = this;

  StartClientManagerOp(&ClientManager::Navigate, args, mGlobal,
    [self, outerPromise] (const ClientOpResult& aResult) {
      if (aResult.type() != ClientOpResult::TClientInfoAndState) {
        outerPromise->MaybeResolve(JS::NullHandleValue);
        return;
      }
      RefPtr<Client> newClient =
        new Client(self->mGlobal, aResult.get_ClientInfoAndState());
      outerPromise->MaybeResolve(newClient);
    }, [self, outerPromise] (nsresult aResult) {
      // TODO: Improve this error in bug 1412856.  Ideally we should throw
      //       the TypeError in the child process and pass it back to here.
      outerPromise->MaybeReject(NS_ERROR_TYPE_ERR);
    });

  return outerPromise.forget();
}

} // namespace dom
} // namespace mozilla
