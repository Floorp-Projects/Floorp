/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ServiceWorkerClient.h"

#include "mozilla/dom/MessageEvent.h"
#include "nsGlobalWindow.h"
#include "WorkerPrivate.h"

#include "mozilla/dom/ServiceWorkerClientBinding.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::workers;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ServiceWorkerClient, mOwner)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ServiceWorkerClient)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ServiceWorkerClient)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorkerClient)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
ServiceWorkerClient::WrapObject(JSContext* aCx)
{
  return ServiceWorkerClientBinding::Wrap(aCx, this);
}

namespace {

class ServiceWorkerClientPostMessageRunnable MOZ_FINAL : public nsRunnable
{
  uint64_t mId;
  JSAutoStructuredCloneBuffer mBuffer;
  nsTArray<nsCOMPtr<nsISupports>> mClonedObjects;

public:
  ServiceWorkerClientPostMessageRunnable(uint64_t aId,
                              JSAutoStructuredCloneBuffer&& aData,
                              nsTArray<nsCOMPtr<nsISupports>>& aClonedObjects)
    : mId(aId),
      mBuffer(Move(aData))
  {
    mClonedObjects.SwapElements(aClonedObjects);
  }

  NS_IMETHOD
  Run()
  {
    AssertIsOnMainThread();
    nsGlobalWindow* window = nsGlobalWindow::GetInnerWindowWithId(mId);
    if (!window) {
      return NS_ERROR_FAILURE;
    }

    AutoJSAPI jsapi;
    jsapi.Init(window);
    JSContext* cx = jsapi.cx();

    return DispatchDOMEvent(cx, window);
  }

private:
  NS_IMETHOD
  DispatchDOMEvent(JSContext* aCx, nsGlobalWindow* aTargetWindow)
  {
    AssertIsOnMainThread();

    // Release reference to objects that were AddRef'd for
    // cloning into worker when array goes out of scope.
    nsTArray<nsCOMPtr<nsISupports>> clonedObjects;
    clonedObjects.SwapElements(mClonedObjects);

    JS::Rooted<JS::Value> messageData(aCx);
    if (!mBuffer.read(aCx, &messageData,
                      WorkerStructuredCloneCallbacks(true))) {
      xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDOMMessageEvent> event = new MessageEvent(aTargetWindow,
                                                          nullptr, nullptr);
    nsresult rv =
      event->InitMessageEvent(NS_LITERAL_STRING("message"),
                              false /* non-bubbling */,
                              false /* not cancelable */,
                              messageData,
                              EmptyString(),
                              EmptyString(),
                              nullptr);
    if (NS_FAILED(rv)) {
      xpc::Throw(aCx, rv);
      return NS_ERROR_FAILURE;
    }

    event->SetTrusted(true);
    bool status = false;
    aTargetWindow->DispatchEvent(event, &status);

    if (!status) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }
};

} // anonymous namespace

void
ServiceWorkerClient::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                                 const Optional<Sequence<JS::Value>>& aTransferable,
                                 ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());
  if (aTransferable.WasPassed()) {
    const Sequence<JS::Value>& realTransferable = aTransferable.Value();

    JS::HandleValueArray elements =
      JS::HandleValueArray::fromMarkedLocation(realTransferable.Length(),
                                               realTransferable.Elements());

    JSObject* array = JS_NewArrayObject(aCx, elements);
    if (!array) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    transferable.setObject(*array);
  }

  JSStructuredCloneCallbacks* callbacks = WorkerStructuredCloneCallbacks(false);

  nsTArray<nsCOMPtr<nsISupports>> clonedObjects;

  JSAutoStructuredCloneBuffer buffer;
  if (!buffer.write(aCx, aMessage, transferable, callbacks, &clonedObjects)) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  nsRefPtr<ServiceWorkerClientPostMessageRunnable> runnable =
    new ServiceWorkerClientPostMessageRunnable(mId, Move(buffer), clonedObjects);
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

