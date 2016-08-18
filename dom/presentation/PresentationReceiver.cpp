/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationReceiver.h"

#include "mozilla/dom/PresentationReceiverBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsContentUtils.h"
#include "nsIPresentationService.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "PresentationConnection.h"
#include "PresentationConnectionList.h"
#include "PresentationLog.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PresentationReceiver,
                                      mOwner,
                                      mGetConnectionListPromise,
                                      mConnectionList)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PresentationReceiver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PresentationReceiver)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PresentationReceiver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIPresentationRespondingListener)
NS_INTERFACE_MAP_END

/* static */ already_AddRefed<PresentationReceiver>
PresentationReceiver::Create(nsPIDOMWindowInner* aWindow)
{
  RefPtr<PresentationReceiver> receiver = new PresentationReceiver(aWindow);
  return NS_WARN_IF(!receiver->Init()) ? nullptr : receiver.forget();
}

PresentationReceiver::PresentationReceiver(nsPIDOMWindowInner* aWindow)
  : mOwner(aWindow)
{
  MOZ_ASSERT(aWindow);
}

PresentationReceiver::~PresentationReceiver()
{
  Shutdown();
}

bool
PresentationReceiver::Init()
{
  if (NS_WARN_IF(!mOwner)) {
    return false;
  }
  mWindowId = mOwner->WindowID();

  nsCOMPtr<nsIDocShell> docShell = mOwner->GetDocShell();
  MOZ_ASSERT(docShell);

  nsContentUtils::GetPresentationURL(docShell, mUrl);
  return !mUrl.IsEmpty();
}

void PresentationReceiver::Shutdown()
{
  PRES_DEBUG("receiver shutdown:windowId[%d]\n", mWindowId);

  // Unregister listener for incoming sessions.
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return;
  }

  nsresult rv = service->UnregisterRespondingListener(mWindowId);
  NS_WARN_IF(NS_FAILED(rv));
}

/* virtual */ JSObject*
PresentationReceiver::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto)
{
  return PresentationReceiverBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
PresentationReceiver::NotifySessionConnect(uint64_t aWindowId,
                                           const nsAString& aSessionId)
{
  PRES_DEBUG("receiver session connect:id[%s], windowId[%x]\n",
             NS_ConvertUTF16toUTF8(aSessionId).get(), aWindowId);

  if (NS_WARN_IF(!mOwner)) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(aWindowId != mWindowId)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!mConnectionList)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<PresentationConnection> connection =
    PresentationConnection::Create(mOwner, aSessionId, mUrl,
                                   nsIPresentationService::ROLE_RECEIVER,
                                   mConnectionList);
  if (NS_WARN_IF(!connection)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

already_AddRefed<Promise>
PresentationReceiver::GetConnectionList(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mOwner);
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  if (!mGetConnectionListPromise) {
    mGetConnectionListPromise = Promise::Create(global, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    RefPtr<PresentationReceiver> self = this;
    nsresult rv =
      NS_DispatchToMainThread(NS_NewRunnableFunction([self] () -> void {
        self->CreateConnectionList();
      }));
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
  }

  RefPtr<Promise> promise = mGetConnectionListPromise;
  return promise.forget();
}

void
PresentationReceiver::CreateConnectionList()
{
  MOZ_ASSERT(mGetConnectionListPromise);

  if (mConnectionList) {
    return;
  }

  mConnectionList = new PresentationConnectionList(mOwner,
                                                   mGetConnectionListPromise);

  // Register listener for incoming sessions.
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    mGetConnectionListPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  nsresult rv = service->RegisterRespondingListener(mWindowId, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mGetConnectionListPromise->MaybeReject(rv);
  }
}

} // namespace dom
} // namespace mozilla
