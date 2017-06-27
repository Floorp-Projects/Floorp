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

ServiceWorkerClientInfo::ServiceWorkerClientInfo(nsIDocument* aDoc, uint32_t aOrdinal)
  : mType(ClientType::Window)
  , mOrdinal(aOrdinal)
  , mWindowId(0)
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

  mLastFocusTime = aDoc->LastFocusTime();

  ErrorResult result;
  mFocused = aDoc->HasFocus(result);
  if (result.Failed()) {
    NS_WARNING("Failed to get focus information.");
  }

  MOZ_ASSERT_IF(mLastFocusTime.IsNull(), !mFocused);
  MOZ_ASSERT_IF(mFocused, !mLastFocusTime.IsNull());

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

bool
ServiceWorkerClientInfo::operator<(const ServiceWorkerClientInfo& aRight) const
{
  // Note: the mLastFocusTime comparisons are reversed because we need to
  // put most recently focused values first.  The mOrdinal comparison is
  // normal, though, because otherwise we want normal creation order.

  if (mLastFocusTime == aRight.mLastFocusTime) {
    return mOrdinal < aRight.mOrdinal;
  }

  if (mLastFocusTime.IsNull()) {
    return false;
  }

  if (aRight.mLastFocusTime.IsNull()) {
    return true;
  }

  return mLastFocusTime > aRight.mLastFocusTime;
}

bool
ServiceWorkerClientInfo::operator==(const ServiceWorkerClientInfo& aRight) const
{
  return mLastFocusTime == aRight.mLastFocusTime &&
         mOrdinal == aRight.mOrdinal &&
         mClientId == aRight.mClientId;
}

JSObject*
ServiceWorkerClient::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ClientBinding::Wrap(aCx, this, aGivenProto);
}

ClientType
ServiceWorkerClient::Type() const
{
  return mType;
}

namespace {

class ServiceWorkerClientPostMessageRunnable final
  : public Runnable
  , public StructuredCloneHolder
{
  const uint64_t mSourceID;
  const nsCString mSourceScope;
  const uint64_t mWindowId;

public:
  ServiceWorkerClientPostMessageRunnable(uint64_t aSourceID,
                                         const nsACString& aSourceScope,
                                         uint64_t aWindowId)
    : mozilla::Runnable("ServiceWorkerClientPostMessageRunnable")
    , StructuredCloneHolder(CloningSupported,
                            TransferringSupported,
                            StructuredCloneScope::SameProcessDifferentThread)
    , mSourceID(aSourceID)
    , mSourceScope(aSourceScope)
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

    dom::Navigator* navigator = window->Navigator();
    if (!navigator) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<ServiceWorkerContainer> container = navigator->ServiceWorker();
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(window))) {
      return NS_ERROR_FAILURE;
    }
    JSContext* cx = jsapi.cx();

    return DispatchDOMEvent(cx, window->AsInner(), container);
  }

private:
  NS_IMETHOD
  DispatchDOMEvent(JSContext* aCx, nsPIDOMWindowInner* aWindow,
                   ServiceWorkerContainer* aTargetContainer)
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

    RootedDictionary<MessageEventInit> init(aCx);

    nsCOMPtr<nsIPrincipal> principal = aTargetContainer->GetParentObject()->PrincipalOrNull();
    NS_WARNING_ASSERTION(principal, "Why is the principal null here?");

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
    init.mOrigin = NS_ConvertUTF8toUTF16(origin);


    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      RefPtr<ServiceWorkerRegistrationInfo> reg =
        swm->GetRegistration(principal, mSourceScope);
      if (reg) {
        RefPtr<ServiceWorkerInfo> serviceWorker = reg->GetByID(mSourceID);
        if (serviceWorker) {
          init.mSource.SetValue().SetAsServiceWorker() =
            serviceWorker->GetOrCreateInstance(aWindow);
        }
      }
    }

    if (!TakeTransferredPortsAsSequence(init.mPorts)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    RefPtr<MessageEvent> event =
      MessageEvent::Constructor(aTargetContainer, NS_LITERAL_STRING("message"),
                                init);

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
                                 const Sequence<JSObject*>& aTransferable,
                                 ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());

  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransferable,
                                                          &transferable);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // At the moment we only expose Client on ServiceWorker globals.
  MOZ_ASSERT(workerPrivate->IsServiceWorker());
  uint32_t serviceWorkerID = workerPrivate->ServiceWorkerID();
  nsCString scope = workerPrivate->ServiceWorkerScope();

  RefPtr<ServiceWorkerClientPostMessageRunnable> runnable =
    new ServiceWorkerClientPostMessageRunnable(serviceWorkerID, scope,
                                               mWindowId);

  runnable->Write(aCx, aMessage, transferable, JS::CloneDataPolicy().denySharedArrayBuffer(),
                  aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  aRv = workerPrivate->DispatchToMainThread(runnable.forget());
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

