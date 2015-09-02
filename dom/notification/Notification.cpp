/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Notification.h"

#include "mozilla/Move.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"

#include "mozilla/dom/AppNotificationServiceOptionsBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/NotificationEvent.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"

#include "nsContentPermissionHelper.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsGlobalWindow.h"
#include "nsIAlertsService.h"
#include "nsIAppsService.h"
#include "nsIContentPermissionPrompt.h"
#include "nsIDocument.h"
#include "nsILoadContext.h"
#include "nsINotificationStorage.h"
#include "nsIPermissionManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIServiceWorkerManager.h"
#include "nsIUUIDGenerator.h"
#include "nsIXPConnect.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsServiceManagerUtils.h"
#include "nsStructuredCloneContainer.h"
#include "nsToolkitCompsCID.h"
#include "ServiceWorkerManager.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

#ifdef MOZ_B2G
#include "nsIDOMDesktopNotification.h"
#endif

namespace mozilla {
namespace dom {

using namespace workers;

struct NotificationStrings
{
  const nsString mID;
  const nsString mTitle;
  const nsString mDir;
  const nsString mLang;
  const nsString mBody;
  const nsString mTag;
  const nsString mIcon;
  const nsString mData;
  const nsString mBehavior;
  const nsString mServiceWorkerRegistrationID;
};

class ScopeCheckingGetCallback : public nsINotificationStorageCallback
{
  const nsString mScope;
public:
  explicit ScopeCheckingGetCallback(const nsAString& aScope)
    : mScope(aScope)
  {}

  NS_IMETHOD Handle(const nsAString& aID,
                    const nsAString& aTitle,
                    const nsAString& aDir,
                    const nsAString& aLang,
                    const nsAString& aBody,
                    const nsAString& aTag,
                    const nsAString& aIcon,
                    const nsAString& aData,
                    const nsAString& aBehavior,
                    const nsAString& aServiceWorkerRegistrationID,
                    JSContext* aCx) final
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(!aID.IsEmpty());

    // Skip scopes that don't match when called from getNotifications().
    if (!mScope.IsEmpty() && !mScope.Equals(aServiceWorkerRegistrationID)) {
      return NS_OK;
    }

    NotificationStrings strings = {
      nsString(aID),
      nsString(aTitle),
      nsString(aDir),
      nsString(aLang),
      nsString(aBody),
      nsString(aTag),
      nsString(aIcon),
      nsString(aData),
      nsString(aBehavior),
      nsString(aServiceWorkerRegistrationID),
    };

    mStrings.AppendElement(Move(strings));
    return NS_OK;
  }

  NS_IMETHOD Done(JSContext* aCx) override = 0;

protected:
  virtual ~ScopeCheckingGetCallback()
  {}

  nsTArray<NotificationStrings> mStrings;
};

class NotificationStorageCallback final : public ScopeCheckingGetCallback
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(NotificationStorageCallback)

  NotificationStorageCallback(nsIGlobalObject* aWindow, const nsAString& aScope,
                              Promise* aPromise)
    : ScopeCheckingGetCallback(aScope),
      mWindow(aWindow),
      mPromise(aPromise)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWindow);
    MOZ_ASSERT(aPromise);
  }

  NS_IMETHOD Done(JSContext* aCx) final
  {
    AutoJSAPI jsapi;
    DebugOnly<bool> ok = jsapi.Init(mWindow, aCx);
    MOZ_ASSERT(ok);

    ErrorResult result;
    nsAutoTArray<nsRefPtr<Notification>, 5> notifications;

    for (uint32_t i = 0; i < mStrings.Length(); ++i) {
      nsRefPtr<Notification> n =
        Notification::ConstructFromFields(mWindow,
                                          mStrings[i].mID,
                                          mStrings[i].mTitle,
                                          mStrings[i].mDir,
                                          mStrings[i].mLang,
                                          mStrings[i].mBody,
                                          mStrings[i].mTag,
                                          mStrings[i].mIcon,
                                          mStrings[i].mData,
                                          /* mStrings[i].mBehavior, not
                                           * supported */
                                          mStrings[i].mServiceWorkerRegistrationID,
                                          result);

      n->SetStoredState(true);
      unused << NS_WARN_IF(result.Failed());
      if (!result.Failed()) {
        notifications.AppendElement(n.forget());
      }
    }

    mPromise->MaybeResolve(notifications);
    return NS_OK;
  }

private:
  virtual ~NotificationStorageCallback()
  {}

  nsCOMPtr<nsIGlobalObject> mWindow;
  nsRefPtr<Promise> mPromise;
  const nsString mScope;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(NotificationStorageCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(NotificationStorageCallback)
NS_IMPL_CYCLE_COLLECTION(NotificationStorageCallback, mWindow, mPromise);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NotificationStorageCallback)
  NS_INTERFACE_MAP_ENTRY(nsINotificationStorageCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

class NotificationGetRunnable final : public nsRunnable
{
  const nsString mOrigin;
  const nsString mTag;
  nsCOMPtr<nsINotificationStorageCallback> mCallback;
public:
  NotificationGetRunnable(const nsAString& aOrigin,
                          const nsAString& aTag,
                          nsINotificationStorageCallback* aCallback)
    : mOrigin(aOrigin), mTag(aTag), mCallback(aCallback)
  {}

  NS_IMETHOD
  Run() override
  {
    nsresult rv;
    nsCOMPtr<nsINotificationStorage> notificationStorage =
      do_GetService(NS_NOTIFICATION_STORAGE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = notificationStorage->Get(mOrigin, mTag, mCallback);
    //XXXnsm Is it guaranteed mCallback will be called in case of failure?
    unused << NS_WARN_IF(NS_FAILED(rv));
    return rv;
  }
};

class NotificationPermissionRequest : public nsIContentPermissionRequest,
                                      public nsIRunnable
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_DECL_NSIRUNNABLE
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(NotificationPermissionRequest,
                                           nsIContentPermissionRequest)

  NotificationPermissionRequest(nsIPrincipal* aPrincipal, nsPIDOMWindow* aWindow,
                                NotificationPermissionCallback* aCallback)
    : mPrincipal(aPrincipal), mWindow(aWindow),
      mPermission(NotificationPermission::Default),
      mCallback(aCallback)
  {
    mRequester = new nsContentPermissionRequester(mWindow);
  }

protected:
  virtual ~NotificationPermissionRequest() {}

  nsresult CallCallback();
  nsresult DispatchCallback();
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  NotificationPermission mPermission;
  nsRefPtr<NotificationPermissionCallback> mCallback;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
};

namespace {
class ReleaseNotificationControlRunnable final : public MainThreadWorkerControlRunnable
{
  Notification* mNotification;

public:
  explicit ReleaseNotificationControlRunnable(Notification* aNotification)
    : MainThreadWorkerControlRunnable(aNotification->mWorkerPrivate)
    , mNotification(aNotification)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mNotification->ReleaseObject();
    return true;
  }
};

class GetPermissionRunnable final : public WorkerMainThreadRunnable
{
  NotificationPermission mPermission;

public:
  explicit GetPermissionRunnable(WorkerPrivate* aWorker)
    : WorkerMainThreadRunnable(aWorker)
    , mPermission(NotificationPermission::Denied)
  { }

  bool
  MainThreadRun() override
  {
    ErrorResult result;
    mPermission =
      Notification::GetPermissionInternal(mWorkerPrivate->GetPrincipal(),
                                          result);
    return true;
  }

  NotificationPermission
  GetPermission()
  {
    return mPermission;
  }
};

class FocusWindowRunnable final : public nsRunnable
{
  nsMainThreadPtrHandle<nsPIDOMWindow> mWindow;
public:
  explicit FocusWindowRunnable(const nsMainThreadPtrHandle<nsPIDOMWindow>& aWindow)
    : mWindow(aWindow)
  { }

  NS_IMETHOD
  Run()
  {
    AssertIsOnMainThread();
    if (!mWindow->IsCurrentInnerWindow()) {
      // Window has been closed, this observer is not valid anymore
      return NS_OK;
    }

    nsIDocument* doc = mWindow->GetExtantDoc();
    if (doc) {
      // Browser UI may use DOMWebNotificationClicked to focus the tab
      // from which the event was dispatched.
      nsContentUtils::DispatchChromeEvent(doc, mWindow->GetOuterWindow(),
                                          NS_LITERAL_STRING("DOMWebNotificationClicked"),
                                          true, true);
    }

    return NS_OK;
  }
};

nsresult
CheckScope(nsIPrincipal* aPrincipal, const nsACString& aScope)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aScope, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return aPrincipal->CheckMayLoad(scopeURI, /* report = */ true,
                                  /* allowIfInheritsPrincipal = */ false);
}
} // anonymous namespace

// Subclass that can be directly dispatched to child workers from the main
// thread.
class NotificationWorkerRunnable : public WorkerRunnable
{
protected:
  explicit NotificationWorkerRunnable(WorkerPrivate* aWorkerPrivate)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  {
  }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return true;
  }

  void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) override
  {
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    aWorkerPrivate->ModifyBusyCountFromWorker(aCx, true);
    WorkerRunInternal(aCx, aWorkerPrivate);
    return true;
  }

  void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
          bool aRunResult) override
  {
    aWorkerPrivate->ModifyBusyCountFromWorker(aCx, false);
  }

  virtual void
  WorkerRunInternal(JSContext* aCx, WorkerPrivate* aWorkerPrivate) = 0;
};

// Overrides dispatch and run handlers so we can directly dispatch from main
// thread to child workers.
class NotificationEventWorkerRunnable final : public NotificationWorkerRunnable
{
  Notification* mNotification;
  const nsString mEventName;
public:
  NotificationEventWorkerRunnable(Notification* aNotification,
                                  const nsString& aEventName)
    : NotificationWorkerRunnable(aNotification->mWorkerPrivate)
    , mNotification(aNotification)
    , mEventName(aEventName)
  {}

  void
  WorkerRunInternal(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mNotification->DispatchTrustedEvent(mEventName);
  }
};

class ReleaseNotificationRunnable final : public NotificationWorkerRunnable
{
  Notification* mNotification;
public:
  explicit ReleaseNotificationRunnable(Notification* aNotification)
    : NotificationWorkerRunnable(aNotification->mWorkerPrivate)
    , mNotification(aNotification)
  {}

  void
  WorkerRunInternal(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mNotification->ReleaseObject();
  }
};

// Create one whenever you require ownership of the notification. Use with
// UniquePtr<>. See Notification.h for details.
class NotificationRef final {
  friend class WorkerNotificationObserver;

private:
  Notification* mNotification;
  bool mInited;

  // Only useful for workers.
  void
  Forget()
  {
    mNotification = nullptr;
  }

public:
  explicit NotificationRef(Notification* aNotification)
    : mNotification(aNotification)
  {
    MOZ_ASSERT(mNotification);
    if (mNotification->mWorkerPrivate) {
      mNotification->mWorkerPrivate->AssertIsOnWorkerThread();
    } else {
      AssertIsOnMainThread();
    }

    mInited = mNotification->AddRefObject();
  }

  // This is only required because Gecko runs script in a worker's onclose
  // handler (non-standard, Bug 790919) where calls to AddFeature() will fail.
  // Due to non-standardness and added complications if we decide to support
  // this, attempts to create a Notification in onclose just throw exceptions.
  bool
  Initialized()
  {
    return mInited;
  }

  ~NotificationRef()
  {
    if (Initialized() && mNotification) {
      Notification* notification = mNotification;
      mNotification = nullptr;
      if (notification->mWorkerPrivate && NS_IsMainThread()) {
        // Try to pass ownership back to the worker. If the dispatch succeeds we
        // are guaranteed this runnable will run, and that it will run after queued
        // event runnables, so event runnables will have a safe pointer to the
        // Notification.
        //
        // If the dispatch fails, the worker isn't running anymore and the event
        // runnables have already run or been canceled. We can use a control
        // runnable to release the reference.
        nsRefPtr<ReleaseNotificationRunnable> r =
          new ReleaseNotificationRunnable(notification);

        AutoJSAPI jsapi;
        jsapi.Init();
        if (!r->Dispatch(jsapi.cx())) {
          nsRefPtr<ReleaseNotificationControlRunnable> r =
            new ReleaseNotificationControlRunnable(notification);
          MOZ_ALWAYS_TRUE(r->Dispatch(jsapi.cx()));
        }
      } else {
        notification->AssertIsOnTargetThread();
        notification->ReleaseObject();
      }
    }
  }

  // XXXnsm, is it worth having some sort of WeakPtr like wrapper instead of
  // a rawptr that the NotificationRef can invalidate?
  Notification*
  GetNotification()
  {
    MOZ_ASSERT(Initialized());
    return mNotification;
  }
};

class NotificationTask : public nsRunnable
{
public:
  enum NotificationAction {
    eShow,
    eClose
  };

  NotificationTask(UniquePtr<NotificationRef> aRef, NotificationAction aAction)
    : mNotificationRef(Move(aRef)), mAction(aAction)
  {}

  NS_IMETHOD
  Run() override;
protected:
  virtual ~NotificationTask() {}

  UniquePtr<NotificationRef> mNotificationRef;
  NotificationAction mAction;
};

uint32_t Notification::sCount = 0;

NS_IMPL_CYCLE_COLLECTION(NotificationPermissionRequest, mWindow)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NotificationPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentPermissionRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(NotificationPermissionRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(NotificationPermissionRequest)

NS_IMETHODIMP
NotificationPermissionRequest::Run()
{
  if (nsContentUtils::IsSystemPrincipal(mPrincipal)) {
    mPermission = NotificationPermission::Granted;
  } else {
    // File are automatically granted permission.
    nsCOMPtr<nsIURI> uri;
    mPrincipal->GetURI(getter_AddRefs(uri));

    if (uri) {
      bool isFile;
      uri->SchemeIs("file", &isFile);
      if (isFile) {
        mPermission = NotificationPermission::Granted;
      }
    }
  }

  // Grant permission if pref'ed on.
  if (Preferences::GetBool("notification.prompt.testing", false)) {
    if (Preferences::GetBool("notification.prompt.testing.allow", true)) {
      mPermission = NotificationPermission::Granted;
    } else {
      mPermission = NotificationPermission::Denied;
    }
  }

  if (mPermission != NotificationPermission::Default) {
    return DispatchCallback();
  }

  return nsContentPermissionUtils::AskPermission(this, mWindow);
}

NS_IMETHODIMP
NotificationPermissionRequest::GetPrincipal(nsIPrincipal** aRequestingPrincipal)
{
  NS_ADDREF(*aRequestingPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
NotificationPermissionRequest::GetWindow(nsIDOMWindow** aRequestingWindow)
{
  NS_ADDREF(*aRequestingWindow = mWindow);
  return NS_OK;
}

NS_IMETHODIMP
NotificationPermissionRequest::GetElement(nsIDOMElement** aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);
  *aElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
NotificationPermissionRequest::Cancel()
{
  mPermission = NotificationPermission::Denied;
  return DispatchCallback();
}

NS_IMETHODIMP
NotificationPermissionRequest::Allow(JS::HandleValue aChoices)
{
  MOZ_ASSERT(aChoices.isUndefined());

  mPermission = NotificationPermission::Granted;
  return DispatchCallback();
}

NS_IMETHODIMP
NotificationPermissionRequest::GetRequester(nsIContentPermissionRequester** aRequester)
{
  NS_ENSURE_ARG_POINTER(aRequester);

  nsCOMPtr<nsIContentPermissionRequester> requester = mRequester;
  requester.forget(aRequester);
  return NS_OK;
}

inline nsresult
NotificationPermissionRequest::DispatchCallback()
{
  if (!mCallback) {
    return NS_OK;
  }

  nsCOMPtr<nsIRunnable> callbackRunnable = NS_NewRunnableMethod(this,
    &NotificationPermissionRequest::CallCallback);
  return NS_DispatchToMainThread(callbackRunnable);
}

nsresult
NotificationPermissionRequest::CallCallback()
{
  ErrorResult rv;
  mCallback->Call(mPermission, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
NotificationPermissionRequest::GetTypes(nsIArray** aTypes)
{
  nsTArray<nsString> emptyOptions;
  return nsContentPermissionUtils::CreatePermissionArray(NS_LITERAL_CSTRING("desktop-notification"),
                                                         NS_LITERAL_CSTRING("unused"),
                                                         emptyOptions,
                                                         aTypes);
}

class NotificationObserver : public nsIObserver
{
public:
  UniquePtr<NotificationRef> mNotificationRef;
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  explicit NotificationObserver(UniquePtr<NotificationRef> aRef)
    : mNotificationRef(Move(aRef))
  {
    AssertIsOnMainThread();
  }

protected:
  virtual ~NotificationObserver()
  {
    AssertIsOnMainThread();
  }
};

NS_IMPL_ISUPPORTS(NotificationObserver, nsIObserver)

NS_IMETHODIMP
NotificationTask::Run()
{
  AssertIsOnMainThread();

  // Get a pointer to notification before the notification takes ownership of
  // the ref (it owns itself temporarily, with ShowInternal() and
  // CloseInternal() passing on the ownership appropriately.)
  Notification* notif = mNotificationRef->GetNotification();
  notif->mTempRef.swap(mNotificationRef);
  if (mAction == eShow) {
    notif->ShowInternal();
  } else if (mAction == eClose) {
    notif->CloseInternal();
  } else {
    MOZ_CRASH("Invalid action");
  }

  MOZ_ASSERT(!mNotificationRef);
  return NS_OK;
}

// static
bool
Notification::PrefEnabled(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.webnotifications.enabled", false);
  }

  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->DOMWorkerNotificationEnabled();
}

// static
bool
Notification::IsGetEnabled(JSContext* aCx, JSObject* aObj)
{
  return NS_IsMainThread();
}

Notification::Notification(nsIGlobalObject* aGlobal, const nsAString& aID,
                           const nsAString& aTitle, const nsAString& aBody,
                           NotificationDirection aDir, const nsAString& aLang,
                           const nsAString& aTag, const nsAString& aIconUrl,
                           const NotificationBehavior& aBehavior)
  : DOMEventTargetHelper(),
    mWorkerPrivate(nullptr), mObserver(nullptr),
    mID(aID), mTitle(aTitle), mBody(aBody), mDir(aDir), mLang(aLang),
    mTag(aTag), mIconUrl(aIconUrl), mBehavior(aBehavior), mData(JS::NullValue()),
    mIsClosed(false), mIsStored(false), mTaskCount(0)
{
  if (NS_IsMainThread()) {
    // We can only call this on the main thread because
    // Event::SetEventType() called down the call chain when dispatching events
    // using DOMEventTargetHelper::DispatchTrustedEvent() will assume the event
    // is a main thread event if it has a valid owner. It will then attempt to
    // fetch the atom for the event name which asserts main thread only.
    BindToOwner(aGlobal);
  } else {
    mWorkerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(mWorkerPrivate);
  }
}

void
Notification::SetAlertName()
{
  AssertIsOnMainThread();
  if (!mAlertName.IsEmpty()) {
    return;
  }

  nsAutoString alertName;
  nsresult rv = GetOrigin(GetPrincipal(), alertName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Get the notification name that is unique per origin + tag/ID.
  // The name of the alert is of the form origin#tag/ID.
  alertName.Append('#');
  if (!mTag.IsEmpty()) {
    alertName.AppendLiteral("tag:");
    alertName.Append(mTag);
  } else {
    alertName.AppendLiteral("notag:");
    alertName.Append(mID);
  }

  mAlertName = alertName;
}

// May be called on any thread.
// static
already_AddRefed<Notification>
Notification::Constructor(const GlobalObject& aGlobal,
                          const nsAString& aTitle,
                          const NotificationOptions& aOptions,
                          ErrorResult& aRv)
{
  // FIXME(nsm): If the sticky flag is set, throw an error.
  ServiceWorkerGlobalScope* scope = nullptr;
  UNWRAP_WORKER_OBJECT(ServiceWorkerGlobalScope, aGlobal.Get(), scope);
  if (scope) {
    aRv.ThrowTypeError(MSG_NOTIFICATION_NO_CONSTRUCTOR_IN_SERVICEWORKER);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<Notification> notification =
    CreateAndShow(global, aTitle, aOptions, EmptyString(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // This is be ok since we are on the worker thread where this function will
  // run to completion before the Notification has a chance to go away.
  return notification.forget();
}

// static
already_AddRefed<Notification>
Notification::ConstructFromFields(
    nsIGlobalObject* aGlobal,
    const nsAString& aID,
    const nsAString& aTitle,
    const nsAString& aDir,
    const nsAString& aLang,
    const nsAString& aBody,
    const nsAString& aTag,
    const nsAString& aIcon,
    const nsAString& aData,
    const nsAString& aServiceWorkerRegistrationID,
    ErrorResult& aRv)
{
  MOZ_ASSERT(aGlobal);

  AutoJSAPI jsapi;
  DebugOnly<bool> ok = jsapi.Init(aGlobal);
  MOZ_ASSERT(ok);

  RootedDictionary<NotificationOptions> options(jsapi.cx());
  options.mDir = Notification::StringToDirection(nsString(aDir));
  options.mLang = aLang;
  options.mBody = aBody;
  options.mTag = aTag;
  options.mIcon = aIcon;
  nsRefPtr<Notification> notification = CreateInternal(aGlobal, aID, aTitle,
                                                       options);

  notification->InitFromBase64(jsapi.cx(), aData, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  notification->SetScope(aServiceWorkerRegistrationID);

  return notification.forget();
}

nsresult
Notification::PersistNotification()
{
  AssertIsOnMainThread();
  nsresult rv;
  nsCOMPtr<nsINotificationStorage> notificationStorage =
    do_GetService(NS_NOTIFICATION_STORAGE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsString origin;
  rv = GetOrigin(GetPrincipal(), origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString id;
  GetID(id);

  nsString alertName;
  GetAlertName(alertName);

  nsAutoString behavior;
  if (!mBehavior.ToJSON(behavior)) {
    return NS_ERROR_FAILURE;
  }

  rv = notificationStorage->Put(origin,
                                id,
                                mTitle,
                                DirectionToString(mDir),
                                mLang,
                                mBody,
                                mTag,
                                mIconUrl,
                                alertName,
                                mDataAsBase64,
                                behavior,
                                mScope);

  if (NS_FAILED(rv)) {
    return rv;
  }

  SetStoredState(true);
  return NS_OK;
}

void
Notification::UnpersistNotification()
{
  AssertIsOnMainThread();
  if (IsStored()) {
    nsCOMPtr<nsINotificationStorage> notificationStorage =
      do_GetService(NS_NOTIFICATION_STORAGE_CONTRACTID);
    if (notificationStorage) {
      nsString origin;
      nsresult rv = GetOrigin(GetPrincipal(), origin);
      if (NS_SUCCEEDED(rv)) {
        notificationStorage->Delete(origin, mID);
      }
    }
    SetStoredState(false);
  }
}

already_AddRefed<Notification>
Notification::CreateInternal(nsIGlobalObject* aGlobal,
                             const nsAString& aID,
                             const nsAString& aTitle,
                             const NotificationOptions& aOptions)
{
  nsString id;
  if (!aID.IsEmpty()) {
    id = aID;
  } else {
    nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1");
    NS_ENSURE_TRUE(uuidgen, nullptr);
    nsID uuid;
    nsresult rv = uuidgen->GenerateUUIDInPlace(&uuid);
    NS_ENSURE_SUCCESS(rv, nullptr);

    char buffer[NSID_LENGTH];
    uuid.ToProvidedString(buffer);
    NS_ConvertASCIItoUTF16 convertedID(buffer);
    id = convertedID;
  }

  nsRefPtr<Notification> notification = new Notification(aGlobal, id, aTitle,
                                                         aOptions.mBody,
                                                         aOptions.mDir,
                                                         aOptions.mLang,
                                                         aOptions.mTag,
                                                         aOptions.mIcon,
                                                         aOptions.mMozbehavior);
  return notification.forget();
}

Notification::~Notification()
{
  mData.setUndefined();
  mozilla::DropJSObjects(this);
  AssertIsOnTargetThread();
  MOZ_ASSERT(!mFeature);
  MOZ_ASSERT(!mTempRef);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Notification)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Notification, DOMEventTargetHelper)
  tmp->mData.setUndefined();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Notification, DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(Notification, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mData);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(Notification, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Notification, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Notification)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

nsIPrincipal*
Notification::GetPrincipal()
{
  AssertIsOnMainThread();
  if (mWorkerPrivate) {
    return mWorkerPrivate->GetPrincipal();
  } else {
    nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(GetOwner());
    NS_ENSURE_TRUE(sop, nullptr);
    return sop->GetPrincipal();
  }
}

class WorkerNotificationObserver final : public NotificationObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER

  explicit WorkerNotificationObserver(UniquePtr<NotificationRef> aRef)
    : NotificationObserver(Move(aRef))
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mNotificationRef->GetNotification()->mWorkerPrivate);
  }

  void
  ForgetNotification()
  {
    AssertIsOnMainThread();
    mNotificationRef->Forget();
  }

protected:
  virtual ~WorkerNotificationObserver()
  {
    AssertIsOnMainThread();

    MOZ_ASSERT(mNotificationRef);
    Notification* notification = mNotificationRef->GetNotification();
    if (notification) {
      notification->mObserver = nullptr;
    }
  }
};

NS_IMPL_ISUPPORTS_INHERITED0(WorkerNotificationObserver, NotificationObserver)

class ServiceWorkerNotificationObserver final : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ServiceWorkerNotificationObserver(const nsAString& aScope,
                                    nsIPrincipal* aPrincipal,
                                    const nsAString& aID)
    : mScope(aScope), mID(aID), mPrincipal(aPrincipal)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aPrincipal);
  }

private:
  ~ServiceWorkerNotificationObserver()
  {}

  const nsString mScope;
  const nsString mID;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

NS_IMPL_ISUPPORTS(ServiceWorkerNotificationObserver, nsIObserver)

// For ServiceWorkers.
bool
Notification::DispatchNotificationClickEvent()
{
  MOZ_ASSERT(mWorkerPrivate);
  MOZ_ASSERT(mWorkerPrivate->IsServiceWorker());
  mWorkerPrivate->AssertIsOnWorkerThread();

  NotificationEventInit options;
  options.mNotification = this;

  ErrorResult result;
  nsRefPtr<EventTarget> target = mWorkerPrivate->GlobalScope();
  nsRefPtr<NotificationEvent> event =
    NotificationEvent::Constructor(target,
                                   NS_LITERAL_STRING("notificationclick"),
                                   options,
                                   result);
  if (NS_WARN_IF(result.Failed())) {
    return false;
  }

  event->SetTrusted(true);
  WantsPopupControlCheck popupControlCheck(event);
  target->DispatchDOMEvent(nullptr, event, nullptr, nullptr);
  // We always return false since in case of dispatching on the serviceworker,
  // there is no well defined window to focus. The script may use the
  // Client.focus() API if it wishes.
  return false;
}

bool
Notification::DispatchClickEvent()
{
  AssertIsOnTargetThread();
  nsRefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  nsresult rv = event->InitEvent(NS_LITERAL_STRING("click"), false, true);
  NS_ENSURE_SUCCESS(rv, false);
  event->SetTrusted(true);
  WantsPopupControlCheck popupControlCheck(event);
  bool doDefaultAction = true;
  DispatchEvent(event, &doDefaultAction);
  return doDefaultAction;
}

// Overrides dispatch and run handlers so we can directly dispatch from main
// thread to child workers.
class NotificationClickWorkerRunnable final : public NotificationWorkerRunnable
{
  Notification* mNotification;
  // Optional window that gets focused if click event is not
  // preventDefault()ed.
  nsMainThreadPtrHandle<nsPIDOMWindow> mWindow;
public:
  NotificationClickWorkerRunnable(Notification* aNotification,
                                  const nsMainThreadPtrHandle<nsPIDOMWindow>& aWindow)
    : NotificationWorkerRunnable(aNotification->mWorkerPrivate)
    , mNotification(aNotification)
    , mWindow(aWindow)
  {
    MOZ_ASSERT_IF(mWorkerPrivate->IsServiceWorker(), !mWindow);
  }

  void
  WorkerRunInternal(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    bool doDefaultAction = mNotification->DispatchClickEvent();
    MOZ_ASSERT_IF(mWorkerPrivate->IsServiceWorker(), !doDefaultAction);
    if (doDefaultAction) {
      nsRefPtr<FocusWindowRunnable> r = new FocusWindowRunnable(mWindow);
      NS_DispatchToMainThread(r);
    }
  }
};

NS_IMETHODIMP
NotificationObserver::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mNotificationRef);
  Notification* notification = mNotificationRef->GetNotification();
  MOZ_ASSERT(notification);
  if (!strcmp("alertclickcallback", aTopic)) {
    nsCOMPtr<nsPIDOMWindow> window = notification->GetOwner();
    if (NS_WARN_IF(!window || !window->IsCurrentInnerWindow())) {
      // Window has been closed, this observer is not valid anymore
      return NS_ERROR_FAILURE;
    }

    bool doDefaultAction = notification->DispatchClickEvent();
    if (doDefaultAction) {
      nsIDocument* doc = window ? window->GetExtantDoc() : nullptr;
      if (doc) {
        // Browser UI may use DOMWebNotificationClicked to focus the tab
        // from which the event was dispatched.
        nsContentUtils::DispatchChromeEvent(doc, window->GetOuterWindow(),
                                            NS_LITERAL_STRING("DOMWebNotificationClicked"),
                                            true, true);
      }
    }
  } else if (!strcmp("alertfinished", aTopic)) {
    // In b2g-desktop, if the app is closed, closing a notification still
    // triggers the observer which might be alive even though the owner window
    // was closed. Keeping this until we remove the close event (Bug 1139363)
    // from implementation.
    nsCOMPtr<nsPIDOMWindow> window = notification->GetOwner();
    if (NS_WARN_IF(!window || !window->IsCurrentInnerWindow())) {
      return NS_ERROR_FAILURE;
    }

    notification->UnpersistNotification();
    notification->mIsClosed = true;
    notification->DispatchTrustedEvent(NS_LITERAL_STRING("close"));
  } else if (!strcmp("alertshow", aTopic)) {
    notification->DispatchTrustedEvent(NS_LITERAL_STRING("show"));
  }
  return NS_OK;
}

NS_IMETHODIMP
WorkerNotificationObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* aData)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mNotificationRef);
  // For an explanation of why it is OK to pass this rawptr to the event
  // runnables, see the Notification class comment.
  Notification* notification = mNotificationRef->GetNotification();
  // We can't assert notification here since the feature could've unset it.
  if (NS_WARN_IF(!notification)) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(notification->mWorkerPrivate);

  nsRefPtr<WorkerRunnable> r;
  if (!strcmp("alertclickcallback", aTopic)) {
    nsPIDOMWindow* window = nullptr;
    if (!notification->mWorkerPrivate->IsServiceWorker()) {
      WorkerPrivate* top = notification->mWorkerPrivate;
      while (top->GetParent()) {
        top = top->GetParent();
      }

      window = top->GetWindow();
      if (NS_WARN_IF(!window || !window->IsCurrentInnerWindow())) {
        // Window has been closed, this observer is not valid anymore
        return NS_ERROR_FAILURE;
      }
    }

    // Instead of bothering with adding features and other worker lifecycle
    // management, we simply hold strongrefs to the window and document.
    nsMainThreadPtrHandle<nsPIDOMWindow> windowHandle(
      new nsMainThreadPtrHolder<nsPIDOMWindow>(window));

    r = new NotificationClickWorkerRunnable(notification, windowHandle);
  } else if (!strcmp("alertfinished", aTopic)) {
    notification->UnpersistNotification();
    notification->mIsClosed = true;
    r = new NotificationEventWorkerRunnable(notification,
                                            NS_LITERAL_STRING("close"));
  } else if (!strcmp("alertshow", aTopic)) {
    r = new NotificationEventWorkerRunnable(notification,
                                            NS_LITERAL_STRING("show"));
  }

  MOZ_ASSERT(r);
  AutoSafeJSContext cx;
  if (!r->Dispatch(cx)) {
    NS_WARNING("Could not dispatch event to worker notification");
  }
  return NS_OK;
}

class NotificationClickEventCallback final : public nsINotificationStorageCallback
{
public:
  NS_DECL_ISUPPORTS

  NotificationClickEventCallback(nsIPrincipal* aPrincipal,
                                 const nsAString& aScope)
  : mPrincipal(aPrincipal), mScope(aScope)
  {
    MOZ_ASSERT(aPrincipal);
  }

  NS_IMETHOD Handle(const nsAString& aID,
                    const nsAString& aTitle,
                    const nsAString& aDir,
                    const nsAString& aLang,
                    const nsAString& aBody,
                    const nsAString& aTag,
                    const nsAString& aIcon,
                    const nsAString& aData,
                    const nsAString& aBehavior,
                    const nsAString& aServiceWorkerRegistrationID,
                    JSContext* aCx) override
  {
    MOZ_ASSERT(!aID.IsEmpty());
    MOZ_ASSERT(mScope.Equals(aServiceWorkerRegistrationID));

    AssertIsOnMainThread();

    nsAutoCString originSuffix;
    nsresult rv = mPrincipal->GetOriginSuffix(originSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIServiceWorkerManager> swm =
      mozilla::services::GetServiceWorkerManager();

    if (swm) {
      swm->SendNotificationClickEvent(originSuffix,
                                      NS_ConvertUTF16toUTF8(mScope),
                                      aID,
                                      aTitle,
                                      aDir,
                                      aLang,
                                      aBody,
                                      aTag,
                                      aIcon,
                                      aData,
                                      aBehavior);
    }
    return NS_OK;
  }

  NS_IMETHOD Done(JSContext* aCx) override
  {
    return NS_OK;
  }

private:
  ~NotificationClickEventCallback()
  {
  }

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsString mScope;
};

NS_IMPL_ISUPPORTS(NotificationClickEventCallback, nsINotificationStorageCallback)

NS_IMETHODIMP
ServiceWorkerNotificationObserver::Observe(nsISupports* aSubject,
                                           const char* aTopic,
                                           const char16_t* aData)
{
  AssertIsOnMainThread();
  // Persistent notifications only care about the click event.
  if (!strcmp("alertclickcallback", aTopic)) {
    nsresult rv;
    nsCOMPtr<nsINotificationStorage> notificationStorage =
      do_GetService(NS_NOTIFICATION_STORAGE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsINotificationStorageCallback> callback =
      new NotificationClickEventCallback(mPrincipal, mScope);

    nsAutoString origin;
    rv = Notification::GetOrigin(mPrincipal, origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = notificationStorage->GetByID(origin, mID, callback);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

void
Notification::ShowInternal()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mTempRef, "Notification should take ownership of itself before"
                       "calling ShowInternal!");
  // A notification can only have one observer and one call to ShowInternal.
  MOZ_ASSERT(!mObserver);

  // Transfer ownership to local scope so we can either release it at the end
  // of this function or transfer it to the observer.
  UniquePtr<NotificationRef> ownership;
  mozilla::Swap(ownership, mTempRef);
  MOZ_ASSERT(ownership->GetNotification() == this);

  nsresult rv = PersistNotification();
  if (NS_FAILED(rv)) {
    NS_WARNING("Could not persist Notification");
  }

  nsCOMPtr<nsIAlertsService> alertService =
    do_GetService(NS_ALERTSERVICE_CONTRACTID);

  ErrorResult result;
  NotificationPermission permission = NotificationPermission::Denied;
  if (mWorkerPrivate) {
    permission = GetPermissionInternal(mWorkerPrivate->GetPrincipal(), result);
  } else {
    permission = GetPermissionInternal(GetOwner(), result);
  }
  if (permission != NotificationPermission::Granted || !alertService) {
    if (mWorkerPrivate) {
      nsRefPtr<NotificationEventWorkerRunnable> r =
        new NotificationEventWorkerRunnable(this,
                                            NS_LITERAL_STRING("error"));
      AutoSafeJSContext cx;
      if (!r->Dispatch(cx)) {
        NS_WARNING("Could not dispatch event to worker notification");
      }
    } else {
      DispatchTrustedEvent(NS_LITERAL_STRING("error"));
    }
    return;
  }

  nsAutoString iconUrl;
  nsAutoString soundUrl;
  ResolveIconAndSoundURL(iconUrl, soundUrl);

  nsCOMPtr<nsIObserver> observer;
  if (mScope.IsEmpty()) {
    // Ownership passed to observer.
    if (mWorkerPrivate) {
      // Scope better be set on ServiceWorker initiated requests.
      MOZ_ASSERT(!mWorkerPrivate->IsServiceWorker());
      // Keep a pointer so that the feature can tell the observer not to release
      // the notification.
      mObserver = new WorkerNotificationObserver(Move(ownership));
      observer = mObserver;
    } else {
      observer = new NotificationObserver(Move(ownership));
    }
  } else {
    // This observer does not care about the Notification. It will be released
    // at the end of this function.
    //
    // The observer is wholly owned by the alerts service.
    observer = new ServiceWorkerNotificationObserver(mScope, GetPrincipal(), mID);
  }
  MOZ_ASSERT(observer);

#ifdef MOZ_B2G
  nsCOMPtr<nsIAppNotificationService> appNotifier =
    do_GetService("@mozilla.org/system-alerts-service;1");
  if (appNotifier) {
    uint32_t appId = nsIScriptSecurityManager::UNKNOWN_APP_ID;
    if (mWorkerPrivate) {
      appId = mWorkerPrivate->GetPrincipal()->GetAppId();
    } else {
      nsCOMPtr<nsPIDOMWindow> window = GetOwner();
      appId = (window.get())->GetDoc()->NodePrincipal()->GetAppId();
    }

    if (appId != nsIScriptSecurityManager::UNKNOWN_APP_ID) {
      nsCOMPtr<nsIAppsService> appsService = do_GetService("@mozilla.org/AppsService;1");
      nsString manifestUrl = EmptyString();
      nsresult rv = appsService->GetManifestURLByLocalId(appId, manifestUrl);
      if (NS_SUCCEEDED(rv)) {
        mozilla::AutoSafeJSContext cx;
        JS::Rooted<JS::Value> val(cx);
        AppNotificationServiceOptions ops;
        ops.mTextClickable = true;
        ops.mManifestURL = manifestUrl;
        GetAlertName(ops.mId);
        ops.mDbId = mID;
        ops.mDir = DirectionToString(mDir);
        ops.mLang = mLang;
        ops.mTag = mTag;
        ops.mData = mDataAsBase64;
        ops.mMozbehavior = mBehavior;
        ops.mMozbehavior.mSoundFile = soundUrl;

        if (!ToJSValue(cx, ops, &val)) {
          NS_WARNING("Converting dict to object failed!");
          return;
        }

        appNotifier->ShowAppNotification(iconUrl, mTitle, mBody,
                                         observer, val);
        return;
      }
    }
  }
#endif

  // In the case of IPC, the parent process uses the cookie to map to
  // nsIObserver. Thus the cookie must be unique to differentiate observers.
  nsString uniqueCookie = NS_LITERAL_STRING("notification:");
  uniqueCookie.AppendInt(sCount++);
  //XXXnsm Should this default to true?
  bool inPrivateBrowsing = false;
  nsIDocument* doc = mWorkerPrivate ? mWorkerPrivate->GetDocument()
                                    : GetOwner()->GetExtantDoc();
  if (doc) {
    nsCOMPtr<nsILoadContext> loadContext = doc->GetLoadContext();
    inPrivateBrowsing = loadContext && loadContext->UsePrivateBrowsing();
  } else if (mWorkerPrivate) {
    // Not all workers may have a document, but with Bug 1107516 fixed, they
    // should all have a loadcontext.
    nsCOMPtr<nsILoadGroup> loadGroup = mWorkerPrivate->GetLoadGroup();
    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(nullptr, loadGroup, NS_GET_IID(nsILoadContext),
                                  getter_AddRefs(loadContext));
    inPrivateBrowsing = loadContext && loadContext->UsePrivateBrowsing();
  }

  nsAutoString alertName;
  GetAlertName(alertName);
  alertService->ShowAlertNotification(iconUrl, mTitle, mBody, true,
                                      uniqueCookie, observer, alertName,
                                      DirectionToString(mDir), mLang,
                                      mDataAsBase64, GetPrincipal(),
                                      inPrivateBrowsing);
}

/* static */ bool
Notification::RequestPermissionEnabledForScope(JSContext* aCx, JSObject* /* unused */)
{
  // requestPermission() is not allowed on workers. The calling page should ask
  // for permission on the worker's behalf. This is to prevent 'which window
  // should show the browser pop-up'. See discussion:
  // http://lists.whatwg.org/pipermail/whatwg-whatwg.org/2013-October/041272.html
  return NS_IsMainThread();
}

void
Notification::RequestPermission(const GlobalObject& aGlobal,
                                const Optional<OwningNonNull<NotificationPermissionCallback> >& aCallback,
                                ErrorResult& aRv)
{
  // Get principal from global to make permission request for notifications.
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aGlobal.GetAsSupports());
  if (!sop) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();

  NotificationPermissionCallback* permissionCallback = nullptr;
  if (aCallback.WasPassed()) {
    permissionCallback = &aCallback.Value();
  }
  nsCOMPtr<nsIRunnable> request =
    new NotificationPermissionRequest(principal, window, permissionCallback);

  NS_DispatchToMainThread(request);
}

// static
NotificationPermission
Notification::GetPermission(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  return GetPermission(global, aRv);
}

// static
NotificationPermission
Notification::GetPermission(nsIGlobalObject* aGlobal, ErrorResult& aRv)
{
  if (NS_IsMainThread()) {
    return GetPermissionInternal(aGlobal, aRv);
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    nsRefPtr<GetPermissionRunnable> r =
      new GetPermissionRunnable(worker);
    if (!r->Dispatch(worker->GetJSContext())) {
      aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
      return NotificationPermission::Denied;
    }

    return r->GetPermission();
  }
}

/* static */ NotificationPermission
Notification::GetPermissionInternal(nsISupports* aGlobal, ErrorResult& aRv)
{
  // Get principal from global to check permission for notifications.
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aGlobal);
  if (!sop) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return NotificationPermission::Denied;
  }

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  return GetPermissionInternal(principal, aRv);
}

/* static */ NotificationPermission
Notification::GetPermissionInternal(nsIPrincipal* aPrincipal,
                                    ErrorResult& aRv)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    return NotificationPermission::Granted;
  } else {
    // Allow files to show notifications by default.
    nsCOMPtr<nsIURI> uri;
    aPrincipal->GetURI(getter_AddRefs(uri));
    if (uri) {
      bool isFile;
      uri->SchemeIs("file", &isFile);
      if (isFile) {
        return NotificationPermission::Granted;
      }
    }
  }

  // We also allow notifications is they are pref'ed on.
  if (Preferences::GetBool("notification.prompt.testing", false)) {
    if (Preferences::GetBool("notification.prompt.testing.allow", true)) {
      return NotificationPermission::Granted;
    } else {
      return NotificationPermission::Denied;
    }
  }

  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;

  nsCOMPtr<nsIPermissionManager> permissionManager =
    services::GetPermissionManager();

  permissionManager->TestPermissionFromPrincipal(aPrincipal,
                                                 "desktop-notification",
                                                 &permission);

  // Convert the result to one of the enum types.
  switch (permission) {
  case nsIPermissionManager::ALLOW_ACTION:
    return NotificationPermission::Granted;
  case nsIPermissionManager::DENY_ACTION:
    return NotificationPermission::Denied;
  default:
    return NotificationPermission::Default;
  }
}

nsresult
Notification::ResolveIconAndSoundURL(nsString& iconUrl, nsString& soundUrl)
{
  AssertIsOnMainThread();
  nsresult rv = NS_OK;

  nsCOMPtr<nsIURI> baseUri;

  // XXXnsm If I understand correctly, the character encoding for resolving
  // URIs in new specs is dictated by the URL spec, which states that unless
  // the URL parser is passed an override encoding, the charset to be used is
  // UTF-8. The new Notification icon/sound specification just says to use the
  // Fetch API, where the Request constructor defers to URL parsing specifying
  // the API base URL and no override encoding. So we've to use UTF-8 on
  // workers, but for backwards compat keeping it document charset on main
  // thread.
  const char* charset = "UTF-8";

  if (mWorkerPrivate) {
    baseUri = mWorkerPrivate->GetBaseURI();
  } else {
    nsIDocument* doc = GetOwner()->GetExtantDoc();
    if (doc) {
      baseUri = doc->GetBaseURI();
      charset = doc->GetDocumentCharacterSet().get();
    } else {
      NS_WARNING("No document found for main thread notification!");
      return NS_ERROR_FAILURE;
    }
  }

  if (baseUri) {
    if (mIconUrl.Length() > 0) {
      nsCOMPtr<nsIURI> srcUri;
      rv = NS_NewURI(getter_AddRefs(srcUri), mIconUrl, charset, baseUri);
      if (NS_SUCCEEDED(rv)) {
        nsAutoCString src;
        srcUri->GetSpec(src);
        iconUrl = NS_ConvertUTF8toUTF16(src);
      }
    }
    if (mBehavior.mSoundFile.Length() > 0) {
      nsCOMPtr<nsIURI> srcUri;
      rv = NS_NewURI(getter_AddRefs(srcUri), mBehavior.mSoundFile, charset, baseUri);
      if (NS_SUCCEEDED(rv)) {
        nsAutoCString src;
        srcUri->GetSpec(src);
        soundUrl = NS_ConvertUTF8toUTF16(src);
      }
    }
  }

  return rv;
}

already_AddRefed<Promise>
Notification::Get(nsPIDOMWindow* aWindow,
                  const GetNotificationOptions& aFilter,
                  const nsAString& aScope,
                  ErrorResult& aRv)
{
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsString origin;
  aRv = GetOrigin(doc->NodePrincipal(), origin);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aWindow);
  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsINotificationStorageCallback> callback =
    new NotificationStorageCallback(global, aScope, promise);

  nsRefPtr<NotificationGetRunnable> r =
    new NotificationGetRunnable(origin, aFilter.mTag, callback);

  aRv = NS_DispatchToMainThread(r);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return promise.forget();
}

already_AddRefed<Promise>
Notification::Get(const GlobalObject& aGlobal,
                  const GetNotificationOptions& aFilter,
                  ErrorResult& aRv)
{
  AssertIsOnMainThread();
  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(global);

  return Get(window, aFilter, EmptyString(), aRv);
}

class WorkerGetResultRunnable final : public NotificationWorkerRunnable
{
  nsRefPtr<PromiseWorkerProxy> mPromiseProxy;
  const nsTArray<NotificationStrings> mStrings;
public:
  WorkerGetResultRunnable(WorkerPrivate* aWorkerPrivate,
                          PromiseWorkerProxy* aPromiseProxy,
                          const nsTArray<NotificationStrings>&& aStrings)
    : NotificationWorkerRunnable(aWorkerPrivate)
    , mPromiseProxy(aPromiseProxy)
    , mStrings(Move(aStrings))
  {
  }

  void
  WorkerRunInternal(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    nsRefPtr<Promise> workerPromise = mPromiseProxy->WorkerPromise();

    ErrorResult result;
    nsAutoTArray<nsRefPtr<Notification>, 5> notifications;
    for (uint32_t i = 0; i < mStrings.Length(); ++i) {
      nsRefPtr<Notification> n =
        Notification::ConstructFromFields(aWorkerPrivate->GlobalScope(),
                                          mStrings[i].mID,
                                          mStrings[i].mTitle,
                                          mStrings[i].mDir,
                                          mStrings[i].mLang,
                                          mStrings[i].mBody,
                                          mStrings[i].mTag,
                                          mStrings[i].mIcon,
                                          mStrings[i].mData,
                                          /* mStrings[i].mBehavior, not
                                           * supported */
                                          mStrings[i].mServiceWorkerRegistrationID,
                                          result);

      n->SetStoredState(true);
      unused << NS_WARN_IF(result.Failed());
      if (!result.Failed()) {
        notifications.AppendElement(n.forget());
      }
    }

    workerPromise->MaybeResolve(notifications);
    mPromiseProxy->CleanUp(aCx);
  }
};

class WorkerGetCallback final : public ScopeCheckingGetCallback
{
  nsRefPtr<PromiseWorkerProxy> mPromiseProxy;
public:
  NS_DECL_ISUPPORTS

  WorkerGetCallback(PromiseWorkerProxy* aProxy, const nsAString& aScope)
    : ScopeCheckingGetCallback(aScope), mPromiseProxy(aProxy)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aProxy);
  }

  NS_IMETHOD Done(JSContext* aCx) final
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mPromiseProxy, "Was Done() called twice?");

    nsRefPtr<PromiseWorkerProxy> proxy = mPromiseProxy.forget();
    MutexAutoLock lock(proxy->Lock());
    if (proxy->CleanedUp()) {
      return NS_OK;
    }

    nsRefPtr<WorkerGetResultRunnable> r =
      new WorkerGetResultRunnable(proxy->GetWorkerPrivate(),
                                  proxy,
                                  Move(mStrings));

    r->Dispatch(aCx);
    return NS_OK;
  }

private:
  ~WorkerGetCallback()
  {}
};

NS_IMPL_ISUPPORTS(WorkerGetCallback, nsINotificationStorageCallback)

class WorkerGetRunnable final : public nsRunnable
{
  nsRefPtr<PromiseWorkerProxy> mPromiseProxy;
  const nsString mTag;
  const nsString mScope;
public:
  WorkerGetRunnable(PromiseWorkerProxy* aProxy,
                    const nsAString& aTag,
                    const nsAString& aScope)
    : mPromiseProxy(aProxy), mTag(aTag), mScope(aScope)
  {
    MOZ_ASSERT(mPromiseProxy);
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    nsCOMPtr<nsINotificationStorageCallback> callback =
      new WorkerGetCallback(mPromiseProxy, mScope);

    AutoJSAPI jsapi;
    jsapi.Init();

    nsresult rv;
    nsCOMPtr<nsINotificationStorage> notificationStorage =
      do_GetService(NS_NOTIFICATION_STORAGE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      callback->Done(jsapi.cx());
      return rv;
    }

    MutexAutoLock lock(mPromiseProxy->Lock());
    if (mPromiseProxy->CleanedUp()) {
      return NS_OK;
    }

    nsString origin;
    rv =
      Notification::GetOrigin(mPromiseProxy->GetWorkerPrivate()->GetPrincipal(),
                              origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      callback->Done(jsapi.cx());
      return rv;
    }

    rv = notificationStorage->Get(origin, mTag, callback);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      callback->Done(jsapi.cx());
      return rv;
    }

    return NS_OK;
  }
private:
  ~WorkerGetRunnable()
  {}
};

already_AddRefed<Promise>
Notification::WorkerGet(WorkerPrivate* aWorkerPrivate,
                        const GetNotificationOptions& aFilter,
                        const nsAString& aScope,
                        ErrorResult& aRv)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
  nsRefPtr<Promise> p = Promise::Create(aWorkerPrivate->GlobalScope(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsRefPtr<PromiseWorkerProxy> proxy =
    PromiseWorkerProxy::Create(aWorkerPrivate, p);
  if (!proxy) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return nullptr;
  }

  nsRefPtr<WorkerGetRunnable> r =
    new WorkerGetRunnable(proxy, aFilter.mTag, aScope);
  // Since this is called from script via
  // ServiceWorkerRegistration::GetNotifications, we can assert dispatch.
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(r)));
  return p.forget();
}

JSObject*
Notification::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::NotificationBinding::Wrap(aCx, this, aGivenProto);
}

void
Notification::Close()
{
  AssertIsOnTargetThread();
  auto ref = MakeUnique<NotificationRef>(this);
  if (!ref->Initialized()) {
    return;
  }

  nsCOMPtr<nsIRunnable> closeNotificationTask =
    new NotificationTask(Move(ref), NotificationTask::eClose);
  nsresult rv = NS_DispatchToMainThread(closeNotificationTask);

  if (NS_FAILED(rv)) {
    DispatchTrustedEvent(NS_LITERAL_STRING("error"));
    // If dispatch fails, NotificationTask will release the ref when it goes
    // out of scope at the end of this function.
  }
}

void
Notification::CloseInternal()
{
  AssertIsOnMainThread();
  // Transfer ownership (if any) to local scope so we can release it at the end
  // of this function. This is relevant when the call is from
  // NotificationTask::Run().
  UniquePtr<NotificationRef> ownership;
  mozilla::Swap(ownership, mTempRef);

  SetAlertName();
  UnpersistNotification();
  if (!mIsClosed) {
    nsCOMPtr<nsIAlertsService> alertService =
      do_GetService(NS_ALERTSERVICE_CONTRACTID);
    if (alertService) {
      nsAutoString alertName;
      GetAlertName(alertName);
      alertService->CloseAlert(alertName, GetPrincipal());
    }
  }
}

nsresult
Notification::GetOrigin(nsIPrincipal* aPrincipal, nsString& aOrigin)
{
  if (!aPrincipal) {
    return NS_ERROR_FAILURE;
  }

  uint16_t appStatus = aPrincipal->GetAppStatus();
  uint32_t appId = aPrincipal->GetAppId();

  nsresult rv;
  if (appStatus == nsIPrincipal::APP_STATUS_NOT_INSTALLED ||
      appId == nsIScriptSecurityManager::NO_APP_ID ||
      appId == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    rv = nsContentUtils::GetUTFOrigin(aPrincipal, aOrigin);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // If we are in "app code", use manifest URL as unique origin since
    // multiple apps can share the same origin but not same notifications.
    nsCOMPtr<nsIAppsService> appsService =
      do_GetService("@mozilla.org/AppsService;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    appsService->GetManifestURLByLocalId(appId, aOrigin);
  }

  return NS_OK;
}

void
Notification::GetData(JSContext* aCx,
                      JS::MutableHandle<JS::Value> aRetval)
{
  if (mData.isNull() && !mDataAsBase64.IsEmpty()) {
    nsresult rv;
    nsRefPtr<nsStructuredCloneContainer> container =
      new nsStructuredCloneContainer();
    rv = container->InitFromBase64(mDataAsBase64, JS_STRUCTURED_CLONE_VERSION,
                                   aCx);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRetval.setNull();
      return;
    }

    JS::Rooted<JS::Value> data(aCx);
    rv = container->DeserializeToJsval(aCx, &data);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRetval.setNull();
      return;
    }

    if (data.isGCThing()) {
      mozilla::HoldJSObjects(this);
    }
    mData = data;
  }
  if (mData.isNull()) {
    aRetval.setNull();
    return;
  }

  JS::ExposeValueToActiveJS(mData);
  aRetval.set(mData);
}

void
Notification::InitFromJSVal(JSContext* aCx, JS::Handle<JS::Value> aData,
                            ErrorResult& aRv)
{
  if (!mDataAsBase64.IsEmpty() || aData.isNull()) {
    return;
  }
  nsRefPtr<nsStructuredCloneContainer> dataObjectContainer =
    new nsStructuredCloneContainer();
  aRv = dataObjectContainer->InitFromJSVal(aData, aCx);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  dataObjectContainer->GetDataAsBase64(mDataAsBase64);
}

void Notification::InitFromBase64(JSContext* aCx, const nsAString& aData,
                                  ErrorResult& aRv)
{
  if (!mDataAsBase64.IsEmpty() || aData.IsEmpty()) {
    return;
  }

  // To and fro to ensure it is valid base64.
  nsRefPtr<nsStructuredCloneContainer> container =
    new nsStructuredCloneContainer();
  aRv = container->InitFromBase64(aData, JS_STRUCTURED_CLONE_VERSION,
                                  aCx);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  container->GetDataAsBase64(mDataAsBase64);
}

bool
Notification::AddRefObject()
{
  AssertIsOnTargetThread();
  MOZ_ASSERT_IF(mWorkerPrivate && !mFeature, mTaskCount == 0);
  MOZ_ASSERT_IF(mWorkerPrivate && mFeature, mTaskCount > 0);
  if (mWorkerPrivate && !mFeature) {
    if (!RegisterFeature()) {
      return false;
    }
  }
  AddRef();
  ++mTaskCount;
  return true;
}

void
Notification::ReleaseObject()
{
  AssertIsOnTargetThread();
  MOZ_ASSERT(mTaskCount > 0);
  MOZ_ASSERT_IF(mWorkerPrivate, mFeature);

  --mTaskCount;
  if (mWorkerPrivate && mTaskCount == 0) {
    UnregisterFeature();
  }
  Release();
}

NotificationFeature::NotificationFeature(Notification* aNotification)
  : mNotification(aNotification)
{
  MOZ_ASSERT(mNotification->mWorkerPrivate);
  mNotification->mWorkerPrivate->AssertIsOnWorkerThread();
}

/*
 * Called from the worker, runs on main thread, blocks worker.
 *
 * We can freely access mNotification here because the feature supplied it and
 * the Notification owns the feature.
 */
class CloseNotificationRunnable final
  : public WorkerMainThreadRunnable
{
  Notification* mNotification;
  bool mHadObserver;

  public:
  explicit CloseNotificationRunnable(Notification* aNotification)
    : WorkerMainThreadRunnable(aNotification->mWorkerPrivate)
    , mNotification(aNotification)
    , mHadObserver(false)
  {}

  bool
  MainThreadRun() override
  {
    if (mNotification->mObserver) {
      // The Notify() take's responsibility of releasing the Notification.
      mNotification->mObserver->ForgetNotification();
      mNotification->mObserver = nullptr;
      mHadObserver = true;
    }
    mNotification->CloseInternal();
    return true;
  }

  bool
  HadObserver()
  {
    return mHadObserver;
  }
};

bool
NotificationFeature::Notify(JSContext* aCx, Status aStatus)
{
  if (aStatus >= Canceling) {
    // CloseNotificationRunnable blocks the worker by pushing a sync event loop
    // on the stack. Meanwhile, WorkerControlRunnables dispatched to the worker
    // can still continue running. One of these is
    // ReleaseNotificationControlRunnable that releases the notification,
    // invalidating the notification and this feature. We hold this reference to
    // keep the notification valid until we are done with it.
    //
    // An example of when the control runnable could get dispatched to the
    // worker is if a Notification is created and the worker is immediately
    // closed, but there is no permission to show it so that the main thread
    // immediately drops the NotificationRef. In this case, this function blocks
    // on the main thread, but the main thread dispatches the control runnable,
    // invalidating mNotification.
    nsRefPtr<Notification> kungFuDeathGrip = mNotification;

    // Dispatched to main thread, blocks on closing the Notification.
    nsRefPtr<CloseNotificationRunnable> r =
      new CloseNotificationRunnable(mNotification);
    r->Dispatch(aCx);

    // Only call ReleaseObject() to match the observer's NotificationRef
    // ownership (since CloseNotificationRunnable asked the observer to drop the
    // reference to the notification).
    if (r->HadObserver()) {
      mNotification->ReleaseObject();
    }

    // From this point we cannot touch properties of this feature because
    // ReleaseObject() may have led to the notification going away and the
    // notification owns this feature!
  }
  return true;
}

bool
Notification::RegisterFeature()
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(!mFeature);
  mFeature = MakeUnique<NotificationFeature>(this);
  bool added = mWorkerPrivate->AddFeature(mWorkerPrivate->GetJSContext(),
                                          mFeature.get());
  if (!added) {
    mFeature = nullptr;
  }

  return added;
}

void
Notification::UnregisterFeature()
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mFeature);
  mWorkerPrivate->RemoveFeature(mWorkerPrivate->GetJSContext(),
                                mFeature.get());
  mFeature = nullptr;
}

/*
 * Checks:
 * 1) Is aWorker allowed to show a notification for scope?
 * 2) Is aWorker an active worker?
 *
 * If it is not an active worker, Result() will be NS_ERROR_NOT_AVAILABLE.
 */
class CheckLoadRunnable final : public WorkerMainThreadRunnable
{
  nsresult mRv;
  nsCString mScope;

public:
  explicit CheckLoadRunnable(WorkerPrivate* aWorker, const nsACString& aScope)
    : WorkerMainThreadRunnable(aWorker)
    , mRv(NS_ERROR_DOM_SECURITY_ERR)
    , mScope(aScope)
  { }

  bool
  MainThreadRun() override
  {
    nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
    mRv = CheckScope(principal, mScope);

    if (NS_FAILED(mRv)) {
      return true;
    }

    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    nsRefPtr<ServiceWorkerRegistrationInfo> registration =
      swm->GetRegistration(principal, mScope);

    // This is coming from a ServiceWorkerRegistrationWorkerThread.
    MOZ_ASSERT(registration);

    if (!registration->mActiveWorker ||
        registration->mActiveWorker->ID() != mWorkerPrivate->ServiceWorkerID()) {
      mRv = NS_ERROR_NOT_AVAILABLE;
    }

    return true;
  }

  nsresult
  Result()
  {
    return mRv;
  }

};

/* static */
already_AddRefed<Promise>
Notification::ShowPersistentNotification(nsIGlobalObject *aGlobal,
                                         const nsAString& aScope,
                                         const nsAString& aTitle,
                                         const NotificationOptions& aOptions,
                                         ErrorResult& aRv)
{
  MOZ_ASSERT(aGlobal);

  // Validate scope.
  // XXXnsm: This may be slow due to blocking the worker and waiting on the main
  // thread. On calls from content, we can be sure the scope is valid since
  // ServiceWorkerRegistrations have their scope set correctly. Can this be made
  // debug only? The problem is that there would be different semantics in
  // debug and non-debug builds in such a case.
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aGlobal);
    if (NS_WARN_IF(!sop)) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    nsIPrincipal* principal = sop->GetPrincipal();
    if (NS_WARN_IF(!principal)) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    aRv = CheckScope(principal, NS_ConvertUTF16toUTF8(aScope));
    if (NS_WARN_IF(aRv.Failed())) {
      aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->AssertIsOnWorkerThread();
    nsRefPtr<CheckLoadRunnable> loadChecker =
      new CheckLoadRunnable(worker, NS_ConvertUTF16toUTF8(aScope));
    if (!loadChecker->Dispatch(worker->GetJSContext())) {
      aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
      return nullptr;
    }

    if (NS_WARN_IF(NS_FAILED(loadChecker->Result()))) {
      if (loadChecker->Result() == NS_ERROR_NOT_AVAILABLE) {
        aRv.ThrowTypeError(MSG_NO_ACTIVE_WORKER);
      } else {
        aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      }
      return nullptr;
    }
  }


  nsRefPtr<Promise> p = Promise::Create(aGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // We check permission here rather than pass the Promise to NotificationTask
  // which leads to uglier code.
  NotificationPermission permission = GetPermission(aGlobal, aRv);

  // "If permission for notification’s origin is not "granted", reject promise with a TypeError exception, and terminate these substeps."
  if (NS_WARN_IF(aRv.Failed()) || permission == NotificationPermission::Denied) {
    ErrorResult result;
    result.ThrowTypeError(MSG_NOTIFICATION_PERMISSION_DENIED);
    p->MaybeReject(result);
    return p.forget();
  }

  // "Otherwise, resolve promise with undefined."
  // The Notification may still not be shown due to other errors, but the spec
  // is not concerned with those.
  p->MaybeResolve(JS::UndefinedHandleValue);

  nsRefPtr<Notification> notification =
    CreateAndShow(aGlobal, aTitle, aOptions, aScope, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return p.forget();
}

/* static */ already_AddRefed<Notification>
Notification::CreateAndShow(nsIGlobalObject* aGlobal,
                            const nsAString& aTitle,
                            const NotificationOptions& aOptions,
                            const nsAString& aScope,
                            ErrorResult& aRv)
{
  MOZ_ASSERT(aGlobal);

  AutoJSAPI jsapi;
  jsapi.Init(aGlobal);
  JSContext* cx = jsapi.cx();

  nsRefPtr<Notification> notification = CreateInternal(aGlobal, EmptyString(),
                                                       aTitle, aOptions);

  // Make a structured clone of the aOptions.mData object
  JS::Rooted<JS::Value> data(cx, aOptions.mData);
  notification->InitFromJSVal(cx, data, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  notification->SetScope(aScope);

  auto ref = MakeUnique<NotificationRef>(notification);
  if (NS_WARN_IF(!ref->Initialized())) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return nullptr;
  }

  // Queue a task to show the notification.
  nsCOMPtr<nsIRunnable> showNotificationTask =
    new NotificationTask(Move(ref), NotificationTask::eShow);
  nsresult rv = NS_DispatchToMainThread(showNotificationTask);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    notification->DispatchTrustedEvent(NS_LITERAL_STRING("error"));
  }

  return notification.forget();
}

} // namespace dom
} // namespace mozilla

