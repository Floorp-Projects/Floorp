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
#include "ServiceWorkerManager.h"

#include "mozilla/Services.h"
#include "mozilla/unused.h"

#include "mozilla/dom/BodyUtil.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace dom {

using workers::AssertIsOnMainThread;
using workers::ServiceWorkerManager;

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
  nsTArray<uint8_t> data;
  if (!data.SetCapacity(aDataLen, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (!data.InsertElementsAt(0, aData, aDataLen, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NotifyPush(aScope, aPrincipal, aMessageId, Some(data));
}

NS_IMETHODIMP
PushNotifier::NotifyPush(const nsACString& aScope, nsIPrincipal* aPrincipal,
                         const nsAString& aMessageId)
{
  return NotifyPush(aScope, aPrincipal, aMessageId, Nothing());
}

NS_IMETHODIMP
PushNotifier::NotifySubscriptionChange(const nsACString& aScope,
                                       nsIPrincipal* aPrincipal)
{
  nsresult rv = NotifySubscriptionChangeObservers(aScope, aPrincipal);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  if (XRE_IsContentProcess()) {
    // Forward XPCOM observer notifications to the parent.
    ContentChild* parentActor = ContentChild::GetSingleton();
    if (!NS_WARN_IF(!parentActor)) {
      Unused << NS_WARN_IF(
        !parentActor->SendNotifyPushSubscriptionChangeObservers(
          PromiseFlatCString(aScope), IPC::Principal(aPrincipal)));
    }
  }

  rv = NotifySubscriptionChangeWorkers(aScope, aPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
PushNotifier::NotifySubscriptionModified(const nsACString& aScope,
                                         nsIPrincipal* aPrincipal)
{
  nsresult rv = NotifySubscriptionModifiedObservers(aScope, aPrincipal);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  if (XRE_IsContentProcess()) {
    ContentChild* parentActor = ContentChild::GetSingleton();
    if (!NS_WARN_IF(!parentActor)) {
      Unused << NS_WARN_IF(
        !parentActor->SendNotifyPushSubscriptionModifiedObservers(
          PromiseFlatCString(aScope), IPC::Principal(aPrincipal)));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
PushNotifier::NotifyError(const nsACString& aScope, nsIPrincipal* aPrincipal,
                          const nsAString& aMessage, uint32_t aFlags)
{
  if (ShouldNotifyWorkers(aPrincipal)) {
    // For service worker subscriptions, report the error to all clients.
    NotifyErrorWorkers(aScope, aMessage, aFlags);
    return NS_OK;
  }
  // For system subscriptions, log the error directly to the browser console.
  return nsContentUtils::ReportToConsoleNonLocalized(aMessage,
                                                     aFlags,
                                                     NS_LITERAL_CSTRING("Push"),
                                                     nullptr, /* aDocument */
                                                     nullptr, /* aURI */
                                                     EmptyString(), /* aLine */
                                                     0, /* aLineNumber */
                                                     0, /* aColumnNumber */
                                                     nsContentUtils::eOMIT_LOCATION);
}

nsresult
PushNotifier::NotifyPush(const nsACString& aScope, nsIPrincipal* aPrincipal,
                         const nsAString& aMessageId,
                         const Maybe<nsTArray<uint8_t>>& aData)
{
  // Notify XPCOM observers in the current process.
  nsresult rv = NotifyPushObservers(aScope, aPrincipal, aData);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  if (XRE_IsContentProcess()) {
    // If we're in the content process, forward the notification to the parent.
    // We don't need to do anything if we're already in the parent;
    // `ContentChild::RecvPush` will notify content process observers.
    ContentChild* parentActor = ContentChild::GetSingleton();
    if (!NS_WARN_IF(!parentActor)) {
      if (aData) {
        Unused << NS_WARN_IF(
          !parentActor->SendNotifyPushObserversWithData(
            PromiseFlatCString(aScope), IPC::Principal(aPrincipal),
            PromiseFlatString(aMessageId), aData.ref()));
      } else {
        Unused << NS_WARN_IF(
          !parentActor->SendNotifyPushObservers(
            PromiseFlatCString(aScope), IPC::Principal(aPrincipal),
            PromiseFlatString(aMessageId)));
      }
    }
  }

  rv = NotifyPushWorkers(aScope, aPrincipal, aMessageId, aData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
PushNotifier::NotifyPushWorkers(const nsACString& aScope,
                                nsIPrincipal* aPrincipal,
                                const nsAString& aMessageId,
                                const Maybe<nsTArray<uint8_t>>& aData)
{
  AssertIsOnMainThread();
  NS_ENSURE_ARG(aPrincipal);

  if (XRE_IsContentProcess() || !BrowserTabsRemoteAutostart()) {
    // Notify the worker from the current process. Either we're running in
    // the content process and received a message from the parent, or e10s
    // is disabled.
    if (!ShouldNotifyWorkers(aPrincipal)) {
      return NS_OK;
    }
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      return NS_ERROR_FAILURE;
    }
    nsAutoCString originSuffix;
    nsresult rv = aPrincipal->GetOriginSuffix(originSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return swm->SendPushEvent(originSuffix, aScope, aMessageId, aData);
  }

  // Otherwise, we're in the parent and e10s is enabled. Broadcast the event
  // to all content processes.
  bool ok = true;
  nsTArray<ContentParent*> contentActors;
  ContentParent::GetAll(contentActors);
  for (uint32_t i = 0; i < contentActors.Length(); ++i) {
    if (aData) {
      ok &= contentActors[i]->SendPushWithData(PromiseFlatCString(aScope),
        IPC::Principal(aPrincipal), PromiseFlatString(aMessageId), aData.ref());
    } else {
      ok &= contentActors[i]->SendPush(PromiseFlatCString(aScope),
        IPC::Principal(aPrincipal), PromiseFlatString(aMessageId));
    }
  }
  return ok ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
PushNotifier::NotifySubscriptionChangeWorkers(const nsACString& aScope,
                                              nsIPrincipal* aPrincipal)
{
  AssertIsOnMainThread();
  NS_ENSURE_ARG(aPrincipal);

  if (XRE_IsContentProcess() || !BrowserTabsRemoteAutostart()) {
    // Content process or e10s disabled.
    if (!ShouldNotifyWorkers(aPrincipal)) {
      return NS_OK;
    }
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      return NS_ERROR_FAILURE;
    }
    nsAutoCString originSuffix;
    nsresult rv = aPrincipal->GetOriginSuffix(originSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return swm->SendPushSubscriptionChangeEvent(originSuffix, aScope);
  }

  // Parent process, e10s enabled.
  bool ok = true;
  nsTArray<ContentParent*> contentActors;
  ContentParent::GetAll(contentActors);
  for (uint32_t i = 0; i < contentActors.Length(); ++i) {
    ok &= contentActors[i]->SendPushSubscriptionChange(
      PromiseFlatCString(aScope), IPC::Principal(aPrincipal));
  }
  return ok ? NS_OK : NS_ERROR_FAILURE;
}

void
PushNotifier::NotifyErrorWorkers(const nsACString& aScope,
                                 const nsAString& aMessage,
                                 uint32_t aFlags)
{
  AssertIsOnMainThread();

  if (XRE_IsContentProcess() || !BrowserTabsRemoteAutostart()) {
    // Content process or e10s disabled.
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->ReportToAllClients(PromiseFlatCString(aScope),
                              PromiseFlatString(aMessage),
                              NS_ConvertUTF8toUTF16(aScope), /* aFilename */
                              EmptyString(), /* aLine */
                              0, /* aLineNumber */
                              0, /* aColumnNumber */
                              aFlags);
    }
    return;
  }

  // Parent process, e10s enabled.
  nsTArray<ContentParent*> contentActors;
  ContentParent::GetAll(contentActors);
  if (!contentActors.IsEmpty()) {
    // At least one content process active.
    for (uint32_t i = 0; i < contentActors.Length(); ++i) {
      Unused << NS_WARN_IF(
        !contentActors[i]->SendPushError(PromiseFlatCString(aScope),
          PromiseFlatString(aMessage), aFlags));
    }
    return;
  }
  // Report to the console if no content processes are active.
  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aScope);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  Unused << NS_WARN_IF(NS_FAILED(
    nsContentUtils::ReportToConsoleNonLocalized(aMessage,
                                                aFlags,
                                                NS_LITERAL_CSTRING("Push"),
                                                nullptr, /* aDocument */
                                                scopeURI, /* aURI */
                                                EmptyString(), /* aLine */
                                                0, /* aLineNumber */
                                                0, /* aColumnNumber */
                                                nsContentUtils::eOMIT_LOCATION)));
}

nsresult
PushNotifier::NotifyPushObservers(const nsACString& aScope,
                                  nsIPrincipal* aPrincipal,
                                  const Maybe<nsTArray<uint8_t>>& aData)
{
  nsCOMPtr<nsIPushData> data;
  if (aData) {
    data = new PushData(aData.ref());
  }
  nsCOMPtr<nsIPushMessage> message = new PushMessage(aPrincipal, data);
  return DoNotifyObservers(message, OBSERVER_TOPIC_PUSH, aScope);
}

nsresult
PushNotifier::NotifySubscriptionChangeObservers(const nsACString& aScope,
                                                nsIPrincipal* aPrincipal)
{
  return DoNotifyObservers(aPrincipal, OBSERVER_TOPIC_SUBSCRIPTION_CHANGE,
                           aScope);
}

nsresult
PushNotifier::NotifySubscriptionModifiedObservers(const nsACString& aScope,
                                                  nsIPrincipal* aPrincipal)
{
  return DoNotifyObservers(aPrincipal, OBSERVER_TOPIC_SUBSCRIPTION_MODIFIED,
                           aScope);
}

nsresult
PushNotifier::DoNotifyObservers(nsISupports *aSubject, const char *aTopic,
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
    nsXPIDLCString contractId;
    nsresult rv = catMan->GetCategoryEntry("push",
                                           PromiseFlatCString(aScope).get(),
                                           getter_Copies(contractId));
    if (NS_SUCCEEDED(rv)) {
      // Ensure the service is created - we don't need to do anything with
      // it though - we assume the service constructor attaches a listener.
      nsCOMPtr<nsISupports> service = do_GetService(contractId);
    }
  }
  return obsService->NotifyObservers(aSubject, aTopic,
                                     NS_ConvertUTF8toUTF16(aScope).get());
}

bool
PushNotifier::ShouldNotifyWorkers(nsIPrincipal* aPrincipal)
{
  // System subscriptions use observer notifications instead of service worker
  // events. The `testing.notifyWorkers` pref disables worker events for
  // non-system subscriptions.
  return !nsContentUtils::IsSystemPrincipal(aPrincipal) &&
         Preferences::GetBool("dom.push.testing.notifyWorkers", true);
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
  uint8_t* data = static_cast<uint8_t*>(NS_Alloc(length * sizeof(uint8_t)));
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

} // namespace dom
} // namespace mozilla
