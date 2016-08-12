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
#include "mozilla/dom/ServiceWorkerMessageEvent.h"
#include "mozilla/dom/ServiceWorkerMessageEventBinding.h"
#include "nsGlobalWindow.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDocument.h"
#include "ServiceWorker.h"
#include "ServiceWorkerPrivate.h"
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
  , mFrameType(FrameType::None)
{
  MOZ_ASSERT(aDoc);
  nsresult rv = aDoc->GetOrCreateId(mClientId);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get the UUID of the document.");
  }

  RefPtr<nsGlobalWindow> innerWindow = nsGlobalWindow::Cast(aDoc->GetInnerWindow());
  if (innerWindow) {
    // XXXcatalinb: The inner window can be null if the document is navigating
    // and was detached.
    mWindowId = innerWindow->WindowID();
  }

  nsCOMPtr<nsIURI> originalURI = aDoc->GetOriginalURI();
  if (originalURI) {
    nsAutoCString spec;
    originalURI->GetSpec(spec);
    CopyUTF8toUTF16(spec, mUrl);
  }
  mVisibilityState = aDoc->VisibilityState();

  ErrorResult result;
  mFocused = aDoc->HasFocus(result);
  if (result.Failed()) {
    NS_WARNING("Failed to get focus information.");
  }

  RefPtr<nsGlobalWindow> outerWindow = nsGlobalWindow::Cast(aDoc->GetWindow());
  if (!outerWindow) {
    MOZ_ASSERT(mFrameType == FrameType::None);
  } else if (!outerWindow->IsTopLevelWindow()) {
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
  : public Runnable
  , public StructuredCloneHolder
{
  uint64_t mWindowId;

public:
  explicit ServiceWorkerClientPostMessageRunnable(uint64_t aWindowId)
    : StructuredCloneHolder(CloningSupported, TransferringSupported,
                            SameProcessDifferentThread)
    , mWindowId(aWindowId)
  {}

  NS_IMETHOD
  Run() override
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

    RefPtr<ServiceWorkerContainer> container = navigator->ServiceWorker();
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

    MOZ_ASSERT(aTargetContainer->GetParentObject(),
               "How come we don't have a window here?!");

    JS::Rooted<JS::Value> messageData(aCx);
    ErrorResult rv;
    Read(aTargetContainer->GetParentObject(), aCx, &messageData, rv);
    if (NS_WARN_IF(rv.Failed())) {
      xpc::Throw(aCx, rv.StealNSResult());
      return NS_ERROR_FAILURE;
    }

    RootedDictionary<ServiceWorkerMessageEventInit> init(aCx);

    nsCOMPtr<nsIPrincipal> principal = aTargetContainer->GetParentObject()->PrincipalOrNull();
    NS_WARN_IF_FALSE(principal, "Why is the principal null here?");

    bool isNullPrincipal = false;
    bool isSystemPrincipal = false;
    if (principal) {
      isNullPrincipal = principal->GetIsNullPrincipal();
      MOZ_ASSERT(!isNullPrincipal);
      isSystemPrincipal = principal->GetIsSystemPrincipal();
      MOZ_ASSERT(!isSystemPrincipal);
    }

    init.mData = messageData;
    nsAutoCString origin;
    if (principal && !isNullPrincipal && !isSystemPrincipal) {
      principal->GetOrigin(origin);
    }
    init.mOrigin.Construct(NS_ConvertUTF8toUTF16(origin));
    init.mLastEventId.Construct(EmptyString());
    init.mPorts.Construct();
    init.mPorts.Value().SetNull();

    RefPtr<ServiceWorker> serviceWorker = aTargetContainer->GetController();
    init.mSource.Construct();
    if (serviceWorker) {
      init.mSource.Value().SetValue().SetAsServiceWorker() = serviceWorker;
    } else {
      init.mSource.Value().SetNull();
    }

    RefPtr<ServiceWorkerMessageEvent> event =
      ServiceWorkerMessageEvent::Constructor(aTargetContainer,
                                             NS_LITERAL_STRING("message"), init, rv);

    nsTArray<RefPtr<MessagePort>> ports = TakeTransferredPorts();

    RefPtr<MessagePortList> portList =
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

  RefPtr<ServiceWorkerClientPostMessageRunnable> runnable =
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

