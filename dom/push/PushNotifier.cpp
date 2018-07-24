/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PushNotifier.h"

#include "nsContentUtils.h"
#include "nsCOMPtr.h"
#include "nsICategoryManager.h"
#include "nsIXULRuntime.h"
#include "nsNetUtil.h"
#include "nsXPCOM.h"
#include "mozilla/dom/ServiceWorkerManager.h"

#include "mozilla/Services.h"
#include "mozilla/Unused.h"

#include "mozilla/dom/BodyUtil.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace dom {

PushNotifier::PushNotifier()
{}

PushNotifier::~PushNotifier()
{}

NS_IMPL_CYCLE_COLLECTION_0(PushNotifier)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushNotifier)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPushNotifier)
  NS_INTERFACE_MAP_ENTRY(nsIPushNotifier)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PushNotifier)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushNotifier)

NS_IMETHODIMP
PushNotifier::NotifyPushWithData(const nsACString& aScope,
                                 nsIPrincipal* aPrincipal,
                                 const nsAString& aMessageId,
                                 uint32_t aDataLen, uint8_t* aData)
{
  NS_ENSURE_ARG(aPrincipal);
  nsTArray<uint8_t> data;
  if (!data.SetCapacity(aDataLen, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (!data.InsertElementsAt(0, aData, aDataLen, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PushMessageDispatcher dispatcher(aScope, aPrincipal, aMessageId, Some(data));
  return Dispatch(dispatcher);
}

NS_IMETHODIMP
PushNotifier::NotifyPush(const nsACString& aScope, nsIPrincipal* aPrincipal,
                         const nsAString& aMessageId)
{
  NS_ENSURE_ARG(aPrincipal);
  PushMessageDispatcher dispatcher(aScope, aPrincipal, aMessageId, Nothing());
  return Dispatch(dispatcher);
}

NS_IMETHODIMP
PushNotifier::NotifySubscriptionChange(const nsACString& aScope,
                                       nsIPrincipal* aPrincipal)
{
  NS_ENSURE_ARG(aPrincipal);
  PushSubscriptionChangeDispatcher dispatcher(aScope, aPrincipal);
  return Dispatch(dispatcher);
}

NS_IMETHODIMP
PushNotifier::NotifySubscriptionModified(const nsACString& aScope,
                                         nsIPrincipal* aPrincipal)
{
  NS_ENSURE_ARG(aPrincipal);
  PushSubscriptionModifiedDispatcher dispatcher(aScope, aPrincipal);
  return Dispatch(dispatcher);
}

NS_IMETHODIMP
PushNotifier::NotifyError(const nsACString& aScope, nsIPrincipal* aPrincipal,
                          const nsAString& aMessage, uint32_t aFlags)
{
  NS_ENSURE_ARG(aPrincipal);
  PushErrorDispatcher dispatcher(aScope, aPrincipal, aMessage, aFlags);
  return Dispatch(dispatcher);
}

nsresult
PushNotifier::Dispatch(PushDispatcher& aDispatcher)
{
  if (XRE_IsParentProcess()) {
    // Always notify XPCOM observers in the parent process.
    Unused << NS_WARN_IF(NS_FAILED(aDispatcher.NotifyObservers()));

    nsTArray<ContentParent*> contentActors;
    ContentParent::GetAll(contentActors);
    if (!contentActors.IsEmpty() && !ServiceWorkerParentInterceptEnabled()) {
      // At least one content process is active, so e10s must be enabled.
      // Broadcast a message to notify observers and service workers.
      for (uint32_t i = 0; i < contentActors.Length(); ++i) {
        // We need to filter based on process type, only "web" AKA the default
        // remote type is acceptable.
        if (!contentActors[i]->GetRemoteType().EqualsLiteral(
               DEFAULT_REMOTE_TYPE)) {
          continue;
        }

        // Ensure that the content actor has the permissions avaliable for the
        // principal the push is being sent for before sending the push message
        // down.
        Unused << contentActors[i]->
          TransmitPermissionsForPrincipal(aDispatcher.GetPrincipal());
        if (aDispatcher.SendToChild(contentActors[i])) {
          // Only send the push message to the first content process to avoid
          // multiple SWs showing the same notification. See bug 1300112.
          break;
        }
      }
      return NS_OK;
    }

    if (BrowserTabsRemoteAutostart() && !ServiceWorkerParentInterceptEnabled()) {
      // e10s is enabled, but no content processes are active.
      return aDispatcher.HandleNoChildProcesses();
    }

    // e10s is disabled; notify workers in the parent.
    return aDispatcher.NotifyWorkers();
  }

  // Otherwise, we're in the content process, so e10s must be enabled. Notify
  // observers and workers, then send a message to notify observers in the
  // parent.
  MOZ_ASSERT(XRE_IsContentProcess());

  nsresult rv = aDispatcher.NotifyObserversAndWorkers();

  ContentChild* parentActor = ContentChild::GetSingleton();
  if (!NS_WARN_IF(!parentActor)) {
    Unused << NS_WARN_IF(!aDispatcher.SendToParent(parentActor));
  }

  return rv;
}

PushData::PushData(const nsTArray<uint8_t>& aData)
  : mData(aData)
{}

PushData::~PushData()
{}

NS_IMPL_CYCLE_COLLECTION_0(PushData)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushData)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPushData)
  NS_INTERFACE_MAP_ENTRY(nsIPushData)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PushData)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushData)

nsresult
PushData::EnsureDecodedText()
{
  if (mData.IsEmpty() || !mDecodedText.IsEmpty()) {
    return NS_OK;
  }
  nsresult rv = BodyUtil::ConsumeText(
    mData.Length(),
    reinterpret_cast<uint8_t*>(mData.Elements()),
    mDecodedText
  );
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mDecodedText.Truncate();
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
PushData::Text(nsAString& aText)
{
  nsresult rv = EnsureDecodedText();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  aText = mDecodedText;
  return NS_OK;
}

NS_IMETHODIMP
PushData::Json(JSContext* aCx,
               JS::MutableHandle<JS::Value> aResult)
{
  nsresult rv = EnsureDecodedText();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  ErrorResult error;
  BodyUtil::ConsumeJson(aCx, aResult, mDecodedText, error);
  return error.StealNSResult();
}

NS_IMETHODIMP
PushData::Binary(uint32_t* aDataLen, uint8_t** aData)
{
  NS_ENSURE_ARG_POINTER(aDataLen);
  NS_ENSURE_ARG_POINTER(aData);

  *aData = nullptr;
  if (mData.IsEmpty()) {
    *aDataLen = 0;
    return NS_OK;
  }
  uint32_t length = mData.Length();
  uint8_t* data = static_cast<uint8_t*>(moz_xmalloc(length * sizeof(uint8_t)));
  if (!data) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  memcpy(data, mData.Elements(), length * sizeof(uint8_t));
  *aDataLen = length;
  *aData = data;
  return NS_OK;
}

PushMessage::PushMessage(nsIPrincipal* aPrincipal, nsIPushData* aData)
  : mPrincipal(aPrincipal)
  , mData(aData)
{}

PushMessage::~PushMessage()
{}

NS_IMPL_CYCLE_COLLECTION(PushMessage, mPrincipal, mData)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushMessage)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPushMessage)
  NS_INTERFACE_MAP_ENTRY(nsIPushMessage)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PushMessage)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushMessage)

NS_IMETHODIMP
PushMessage::GetPrincipal(nsIPrincipal** aPrincipal)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);

  nsCOMPtr<nsIPrincipal> principal = mPrincipal;
  principal.forget(aPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
PushMessage::GetData(nsIPushData** aData)
{
  NS_ENSURE_ARG_POINTER(aData);

  nsCOMPtr<nsIPushData> data = mData;
  data.forget(aData);
  return NS_OK;
}

PushDispatcher::PushDispatcher(const nsACString& aScope,
                               nsIPrincipal* aPrincipal)
  : mScope(aScope)
  , mPrincipal(aPrincipal)
{}

PushDispatcher::~PushDispatcher()
{}

nsresult
PushDispatcher::HandleNoChildProcesses()
{
  return NS_OK;
}

nsresult
PushDispatcher::NotifyObserversAndWorkers()
{
  Unused << NS_WARN_IF(NS_FAILED(NotifyObservers()));
  return NotifyWorkers();
}

bool
PushDispatcher::ShouldNotifyWorkers()
{
  if (NS_WARN_IF(!mPrincipal)) {
    return false;
  }
  // System subscriptions use observer notifications instead of service worker
  // events. The `testing.notifyWorkers` pref disables worker events for
  // non-system subscriptions.
  return !nsContentUtils::IsSystemPrincipal(mPrincipal) &&
         Preferences::GetBool("dom.push.testing.notifyWorkers", true);
}

nsresult
PushDispatcher::DoNotifyObservers(nsISupports *aSubject, const char *aTopic,
                                  const nsACString& aScope)
{
  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  if (!obsService) {
    return NS_ERROR_FAILURE;
  }
  // If there's a service for this push category, make sure it is alive.
  nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (catMan) {
    nsCString contractId;
    nsresult rv = catMan->GetCategoryEntry("push", mScope, contractId);
    if (NS_SUCCEEDED(rv)) {
      // Ensure the service is created - we don't need to do anything with
      // it though - we assume the service constructor attaches a listener.
      nsCOMPtr<nsISupports> service = do_GetService(contractId.get());
    }
  }
  return obsService->NotifyObservers(aSubject, aTopic,
                                     NS_ConvertUTF8toUTF16(mScope).get());
}

PushMessageDispatcher::PushMessageDispatcher(const nsACString& aScope,
                                             nsIPrincipal* aPrincipal,
                                             const nsAString& aMessageId,
                                             const Maybe<nsTArray<uint8_t>>& aData)
  : PushDispatcher(aScope, aPrincipal)
  , mMessageId(aMessageId)
  , mData(aData)
{}

PushMessageDispatcher::~PushMessageDispatcher()
{}

nsresult
PushMessageDispatcher::NotifyObservers()
{
  nsCOMPtr<nsIPushData> data;
  if (mData) {
    data = new PushData(mData.ref());
  }
  nsCOMPtr<nsIPushMessage> message = new PushMessage(mPrincipal, data);
  return DoNotifyObservers(message, OBSERVER_TOPIC_PUSH, mScope);
}

nsresult
PushMessageDispatcher::NotifyWorkers()
{
  if (!ShouldNotifyWorkers()) {
    return NS_OK;
  }
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return NS_ERROR_FAILURE;
  }
  nsAutoCString originSuffix;
  nsresult rv = mPrincipal->GetOriginSuffix(originSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return swm->SendPushEvent(originSuffix, mScope, mMessageId, mData);
}

bool
PushMessageDispatcher::SendToParent(ContentChild* aParentActor)
{
  if (mData) {
    return aParentActor->SendNotifyPushObserversWithData(mScope,
                                                         IPC::Principal(mPrincipal),
                                                         mMessageId,
                                                         mData.ref());
  }
  return aParentActor->SendNotifyPushObservers(mScope,
                                               IPC::Principal(mPrincipal),
                                               mMessageId);
}

bool
PushMessageDispatcher::SendToChild(ContentParent* aContentActor)
{
  if (mData) {
    return aContentActor->SendPushWithData(mScope, IPC::Principal(mPrincipal),
                                           mMessageId, mData.ref());
  }
  return aContentActor->SendPush(mScope, IPC::Principal(mPrincipal),
                                 mMessageId);
}

PushSubscriptionChangeDispatcher::PushSubscriptionChangeDispatcher(const nsACString& aScope,
                                                                   nsIPrincipal* aPrincipal)
  : PushDispatcher(aScope, aPrincipal)
{}

PushSubscriptionChangeDispatcher::~PushSubscriptionChangeDispatcher()
{}

nsresult
PushSubscriptionChangeDispatcher::NotifyObservers()
{
  return DoNotifyObservers(mPrincipal, OBSERVER_TOPIC_SUBSCRIPTION_CHANGE,
                           mScope);
}

nsresult
PushSubscriptionChangeDispatcher::NotifyWorkers()
{
  if (!ShouldNotifyWorkers()) {
    return NS_OK;
  }
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return NS_ERROR_FAILURE;
  }
  nsAutoCString originSuffix;
  nsresult rv = mPrincipal->GetOriginSuffix(originSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return swm->SendPushSubscriptionChangeEvent(originSuffix, mScope);
}

bool
PushSubscriptionChangeDispatcher::SendToParent(ContentChild* aParentActor)
{
  return aParentActor->SendNotifyPushSubscriptionChangeObservers(mScope,
                                                                 IPC::Principal(mPrincipal));
}

bool
PushSubscriptionChangeDispatcher::SendToChild(ContentParent* aContentActor)
{
  return aContentActor->SendPushSubscriptionChange(mScope,
                                                   IPC::Principal(mPrincipal));
}

PushSubscriptionModifiedDispatcher::PushSubscriptionModifiedDispatcher(const nsACString& aScope,
                                                                       nsIPrincipal* aPrincipal)
  : PushDispatcher(aScope, aPrincipal)
{}

PushSubscriptionModifiedDispatcher::~PushSubscriptionModifiedDispatcher()
{}

nsresult
PushSubscriptionModifiedDispatcher::NotifyObservers()
{
  return DoNotifyObservers(mPrincipal, OBSERVER_TOPIC_SUBSCRIPTION_MODIFIED,
                           mScope);
}

nsresult
PushSubscriptionModifiedDispatcher::NotifyWorkers()
{
  return NS_OK;
}

bool
PushSubscriptionModifiedDispatcher::SendToParent(ContentChild* aParentActor)
{
  return aParentActor->SendNotifyPushSubscriptionModifiedObservers(mScope,
                                                                   IPC::Principal(mPrincipal));
}

bool
PushSubscriptionModifiedDispatcher::SendToChild(ContentParent* aContentActor)
{
  return aContentActor->SendNotifyPushSubscriptionModifiedObservers(mScope,
                                                                    IPC::Principal(mPrincipal));
}

PushErrorDispatcher::PushErrorDispatcher(const nsACString& aScope,
                                         nsIPrincipal* aPrincipal,
                                         const nsAString& aMessage,
                                         uint32_t aFlags)
  : PushDispatcher(aScope, aPrincipal)
  , mMessage(aMessage)
  , mFlags(aFlags)
{}

PushErrorDispatcher::~PushErrorDispatcher()
{}

nsresult
PushErrorDispatcher::NotifyObservers()
{
  return NS_OK;
}

nsresult
PushErrorDispatcher::NotifyWorkers()
{
  if (!ShouldNotifyWorkers()) {
    // For system subscriptions, log the error directly to the browser console.
    return nsContentUtils::ReportToConsoleNonLocalized(mMessage,
                                                       mFlags,
                                                       NS_LITERAL_CSTRING("Push"),
                                                       nullptr, /* aDocument */
                                                       nullptr, /* aURI */
                                                       EmptyString(), /* aLine */
                                                       0, /* aLineNumber */
                                                       0, /* aColumnNumber */
                                                       nsContentUtils::eOMIT_LOCATION);
  }
  // For service worker subscriptions, report the error to all clients.
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm) {
    swm->ReportToAllClients(mScope,
                            mMessage,
                            NS_ConvertUTF8toUTF16(mScope), /* aFilename */
                            EmptyString(), /* aLine */
                            0, /* aLineNumber */
                            0, /* aColumnNumber */
                            mFlags);
  }
  return NS_OK;
}

bool
PushErrorDispatcher::SendToParent(ContentChild*)
{
  return true;
}

bool
PushErrorDispatcher::SendToChild(ContentParent* aContentActor)
{
  return aContentActor->SendPushError(mScope, IPC::Principal(mPrincipal),
                                      mMessage, mFlags);
}

nsresult
PushErrorDispatcher::HandleNoChildProcesses()
{
  // Report to the console if no content processes are active.
  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), mScope);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return nsContentUtils::ReportToConsoleNonLocalized(mMessage,
                                                     mFlags,
                                                     NS_LITERAL_CSTRING("Push"),
                                                     nullptr, /* aDocument */
                                                     scopeURI, /* aURI */
                                                     EmptyString(), /* aLine */
                                                     0, /* aLineNumber */
                                                     0, /* aColumnNumber */
                                                     nsContentUtils::eOMIT_LOCATION);
}

} // namespace dom
} // namespace mozilla
