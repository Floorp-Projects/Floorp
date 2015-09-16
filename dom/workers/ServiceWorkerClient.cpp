/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ServiceWorkerClient.h"
#include "ServiceWorkerContainer.h"

#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/Navigator.h"
#include "nsGlobalWindow.h"
#include "nsIDocument.h"
#include "WorkerPrivate.h"

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

ServiceWorkerClientInfo::ServiceWorkerClientInfo(nsIDocument* aDoc)
  : mWindowId(0)
{
  MOZ_ASSERT(aDoc);
  nsresult rv = aDoc->GetId(mClientId);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get the UUID of the document.");
  }

  nsRefPtr<nsGlobalWindow> innerWindow = static_cast<nsGlobalWindow*>(aDoc->GetInnerWindow());
  if (innerWindow) {
    // XXXcatalinb: The inner window can be null if the document is navigating
    // and was detached.
    mWindowId = innerWindow->WindowID();
  }

  aDoc->GetURL(mUrl);
  mVisibilityState = aDoc->VisibilityState();

  ErrorResult result;
  mFocused = aDoc->HasFocus(result);
  if (result.Failed()) {
    NS_WARNING("Failed to get focus information.");
  }

  nsRefPtr<nsGlobalWindow> outerWindow = static_cast<nsGlobalWindow*>(aDoc->GetWindow());
  MOZ_ASSERT(outerWindow);
  if (!outerWindow->IsTopLevelWindow()) {
    mFrameType = FrameType::Nested;
  } else if (outerWindow->HadOriginalOpener()) {
    mFrameType = FrameType::Auxiliary;
  } else {
    mFrameType = FrameType::Top_level;
  }
}

JSObject*
ServiceWorkerClient::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ClientBinding::Wrap(aCx, this, aGivenProto);
}

namespace {

class ServiceWorkerClientPostMessageRunnable final
  : public nsRunnable
  , public StructuredCloneHelper
{
  uint64_t mWindowId;

public:
  explicit ServiceWorkerClientPostMessageRunnable(uint64_t aWindowId)
    : StructuredCloneHelper(CloningSupported, TransferringSupported,
                            SameProcessDifferentThread)
    , mWindowId(aWindowId)
  {}

  NS_IMETHOD
  Run()
  {
    AssertIsOnMainThread();
    nsGlobalWindow* window = nsGlobalWindow::GetInnerWindowWithId(mWindowId);
    if (!window) {
      return NS_ERROR_FAILURE;
    }

    ErrorResult result;
    dom::Navigator* navigator = window->GetNavigator(result);
    if (NS_WARN_IF(result.Failed())) {
      return result.StealNSResult();
    }

    nsRefPtr<ServiceWorkerContainer> container = navigator->ServiceWorker();
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(window))) {
      return NS_ERROR_FAILURE;
    }
    JSContext* cx = jsapi.cx();

    return DispatchDOMEvent(cx, container);
  }

private:
  NS_IMETHOD
  DispatchDOMEvent(JSContext* aCx, ServiceWorkerContainer* aTargetContainer)
  {
    AssertIsOnMainThread();

    JS::Rooted<JS::Value> messageData(aCx);
    ErrorResult rv;
    Read(aTargetContainer->GetParentObject(), aCx, &messageData, rv);
    if (NS_WARN_IF(rv.Failed())) {
      xpc::Throw(aCx, rv.StealNSResult());
      return NS_ERROR_FAILURE;
    }

    nsRefPtr<MessageEvent> event = new MessageEvent(aTargetContainer,
                                                    nullptr, nullptr);
    rv = event->InitMessageEvent(NS_LITERAL_STRING("message"),
                                 false /* non-bubbling */,
                                 false /* not cancelable */,
                                 messageData,
                                 EmptyString(),
                                 EmptyString(),
                                 nullptr);
    if (NS_WARN_IF(rv.Failed())) {
      xpc::Throw(aCx, rv.StealNSResult());
      return NS_ERROR_FAILURE;
    }

    nsTArray<nsRefPtr<MessagePort>> ports = TakeTransferredPorts();

    nsRefPtr<MessagePortList> portList =
      new MessagePortList(static_cast<dom::Event*>(event.get()),
                          ports);
    event->SetPorts(portList);

    event->SetTrusted(true);
    bool status = false;
    aTargetContainer->DispatchEvent(static_cast<dom::Event*>(event.get()),
                                    &status);

    if (!status) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }
};

} // namespace

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

  nsRefPtr<ServiceWorkerClientPostMessageRunnable> runnable =
    new ServiceWorkerClientPostMessageRunnable(mWindowId);

  runnable->Write(aCx, aMessage, transferable, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  aRv = NS_DispatchToMainThread(runnable);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

