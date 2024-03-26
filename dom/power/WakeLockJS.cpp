/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ErrorList.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WakeLockBinding.h"
#include "mozilla/Hal.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsError.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsPIDOMWindow.h"
#include "nsContentPermissionHelper.h"
#include "nsServiceManagerUtils.h"
#include "nscore.h"
#include "WakeLock.h"
#include "WakeLockJS.h"
#include "WakeLockSentinel.h"

namespace mozilla::dom {

static mozilla::LazyLogModule sLogger("ScreenWakeLock");

#define MIN_BATTERY_LEVEL 0.05

nsLiteralCString WakeLockJS::GetRequestErrorMessage(RequestError aRv) {
  switch (aRv) {
    case RequestError::DocInactive:
      return "The requesting document is inactive."_ns;
    case RequestError::DocHidden:
      return "The requesting document is hidden."_ns;
    case RequestError::PolicyDisallowed:
      return "A permissions policy does not allow screen-wake-lock for the requesting document."_ns;
    case RequestError::PrefDisabled:
      return "The pref dom.screenwakelock.enabled is disabled."_ns;
    case RequestError::InternalFailure:
      return "A browser-internal error occured."_ns;
    case RequestError::PermissionDenied:
      return "Permission to request screen-wake-lock was denied."_ns;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown error reason");
      return "Unknown error"_ns;
  }
}

// https://w3c.github.io/screen-wake-lock/#the-request-method steps 2-5
WakeLockJS::RequestError WakeLockJS::WakeLockAllowedForDocument(
    Document* aDoc) {
  if (!aDoc) {
    return RequestError::InternalFailure;
  }

  // Step 2. check policy-controlled feature screen-wake-lock
  if (!FeaturePolicyUtils::IsFeatureAllowed(aDoc, u"screen-wake-lock"_ns)) {
    return RequestError::PolicyDisallowed;
  }

  // Step 3. Deny wake lock for user agent specific reasons
  if (!StaticPrefs::dom_screenwakelock_enabled()) {
    return RequestError::PrefDisabled;
  }

  // Step 4 check doc active
  if (!aDoc->IsActive()) {
    return RequestError::DocInactive;
  }

  // Step 5. check doc visible
  if (aDoc->Hidden()) {
    return RequestError::DocHidden;
  }

  return RequestError::Success;
}

// https://w3c.github.io/screen-wake-lock/#dfn-applicable-wake-lock
static bool IsWakeLockApplicable(WakeLockType aType) {
  hal::BatteryInformation batteryInfo;
  hal::GetCurrentBatteryInformation(&batteryInfo);
  if (batteryInfo.level() <= MIN_BATTERY_LEVEL && !batteryInfo.charging()) {
    return false;
  }

  // only currently supported wake lock type
  return aType == WakeLockType::Screen;
}

// https://w3c.github.io/screen-wake-lock/#dfn-release-a-wake-lock
void ReleaseWakeLock(Document* aDoc, WakeLockSentinel* aLock,
                     WakeLockType aType) {
  MOZ_ASSERT(aLock);
  MOZ_ASSERT(aDoc);

  RefPtr<WakeLockSentinel> kungFuDeathGrip = aLock;
  aDoc->ActiveWakeLocks(aType).Remove(aLock);
  aLock->NotifyLockReleased();
  MOZ_LOG(sLogger, LogLevel::Debug, ("Released wake lock sentinel"));
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WakeLockJS)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WakeLockJS)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WakeLockJS)
  tmp->DetachListeners();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WakeLockJS)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WakeLockJS)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WakeLockJS)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentActivity)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

WakeLockJS::WakeLockJS(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {
  AttachListeners();
}

WakeLockJS::~WakeLockJS() { DetachListeners(); }

nsISupports* WakeLockJS::GetParentObject() const { return mWindow; }

JSObject* WakeLockJS::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return WakeLock_Binding::Wrap(aCx, this, aGivenProto);
}

// https://w3c.github.io/screen-wake-lock/#the-request-method Step 7.3
Result<already_AddRefed<WakeLockSentinel>, WakeLockJS::RequestError>
WakeLockJS::Obtain(WakeLockType aType) {
  // Step 7.3.1. check visibility again
  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();
  if (!doc) {
    return Err(RequestError::InternalFailure);
  }
  if (doc->Hidden()) {
    return Err(RequestError::DocHidden);
  }
  // Step 7.3.3. let lock be a new WakeLockSentinel
  RefPtr<WakeLockSentinel> lock =
      MakeRefPtr<WakeLockSentinel>(mWindow->AsGlobal(), aType);
  // Step 7.3.2. acquire a wake lock
  if (IsWakeLockApplicable(aType)) {
    lock->AcquireActualLock();
  }

  // Steps 7.3.4. append lock to locks
  doc->ActiveWakeLocks(aType).Insert(lock);

  return lock.forget();
}

// https://w3c.github.io/screen-wake-lock/#the-request-method
already_AddRefed<Promise> WakeLockJS::Request(WakeLockType aType,
                                              ErrorResult& aRv) {
  MOZ_LOG(sLogger, LogLevel::Debug, ("Received request for wake lock"));
  nsCOMPtr<nsIGlobalObject> global = mWindow->AsGlobal();

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_FALSE(aRv.Failed(), nullptr);

  // Steps 1-5
  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();
  RequestError rv = WakeLockAllowedForDocument(doc);
  if (rv != RequestError::Success) {
    promise->MaybeRejectWithNotAllowedError(GetRequestErrorMessage(rv));
    return promise.forget();
  }

  // For now, we don't check the permission as we always grant the lock
  // Step 7.3. Queue a task
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "ObtainWakeLock", [aType, promise, self = RefPtr<WakeLockJS>(this)]() {
        auto lockOrErr = self->Obtain(aType);
        if (lockOrErr.isOk()) {
          RefPtr<WakeLockSentinel> lock = lockOrErr.unwrap();
          promise->MaybeResolve(lock);
          MOZ_LOG(sLogger, LogLevel::Debug,
                  ("Resolved promise with wake lock sentinel"));
        } else {
          promise->MaybeRejectWithNotAllowedError(
              GetRequestErrorMessage(lockOrErr.unwrapErr()));
        }
      }));

  return promise.forget();
}

void WakeLockJS::AttachListeners() {
  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();
  MOZ_ASSERT(doc);
  DebugOnly<nsresult> rv =
      doc->AddSystemEventListener(u"visibilitychange"_ns, this, true, false);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  doc->RegisterActivityObserver(ToSupports(this));

  hal::RegisterBatteryObserver(this);

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  MOZ_ASSERT(prefBranch);
  rv = prefBranch->AddObserver("dom.screenwakelock.enabled", this, true);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void WakeLockJS::DetachListeners() {
  if (mWindow) {
    if (nsCOMPtr<Document> doc = mWindow->GetExtantDoc()) {
      doc->RemoveSystemEventListener(u"visibilitychange"_ns, this, true);

      doc->UnregisterActivityObserver(ToSupports(this));
    }
  }

  hal::UnregisterBatteryObserver(this);

  if (nsCOMPtr<nsIPrefBranch> prefBranch =
          do_GetService(NS_PREFSERVICE_CONTRACTID)) {
    prefBranch->RemoveObserver("dom.screenwakelock.enabled", this);
  }
}

NS_IMETHODIMP WakeLockJS::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData) {
  if (nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    if (!StaticPrefs::dom_screenwakelock_enabled()) {
      nsCOMPtr<Document> doc = mWindow->GetExtantDoc();
      MOZ_ASSERT(doc);
      doc->UnlockAllWakeLocks(WakeLockType::Screen);
    }
  }
  return NS_OK;
}

void WakeLockJS::NotifyOwnerDocumentActivityChanged() {
  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();
  MOZ_ASSERT(doc);
  if (!doc->IsActive()) {
    doc->UnlockAllWakeLocks(WakeLockType::Screen);
  }
}

NS_IMETHODIMP WakeLockJS::HandleEvent(Event* aEvent) {
  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("visibilitychange")) {
    nsCOMPtr<Document> doc = do_QueryInterface(aEvent->GetTarget());
    NS_ENSURE_STATE(doc);

    if (doc->Hidden()) {
      doc->UnlockAllWakeLocks(WakeLockType::Screen);
    }
  }

  return NS_OK;
}

void WakeLockJS::Notify(const hal::BatteryInformation& aBatteryInfo) {
  if (aBatteryInfo.level() > MIN_BATTERY_LEVEL || aBatteryInfo.charging()) {
    return;
  }
  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();
  MOZ_ASSERT(doc);
  doc->UnlockAllWakeLocks(WakeLockType::Screen);
}

}  // namespace mozilla::dom
