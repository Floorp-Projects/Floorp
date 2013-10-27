/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PCOMContentPermissionRequestChild.h"
#include "mozilla/dom/Notification.h"
#include "mozilla/dom/AppNotificationServiceOptionsBinding.h"
#include "mozilla/dom/OwningNonNull.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/Preferences.h"
#include "TabChild.h"
#include "nsContentUtils.h"
#include "nsDOMEvent.h"
#include "nsIAlertsService.h"
#include "nsIAppsService.h"
#include "nsIContentPermissionPrompt.h"
#include "nsIDocument.h"
#include "nsINotificationStorage.h"
#include "nsIPermissionManager.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"
#include "nsToolkitCompsCID.h"
#include "nsGlobalWindow.h"
#include "nsDOMJSUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentPermissionHelper.h"
#ifdef MOZ_B2G
#include "nsIDOMDesktopNotification.h"
#endif

namespace mozilla {
namespace dom {

class NotificationStorageCallback MOZ_FINAL : public nsINotificationStorageCallback
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(NotificationStorageCallback)

  NotificationStorageCallback(const GlobalObject& aGlobal, nsPIDOMWindow* aWindow, Promise* aPromise)
    : mCount(0),
      mGlobal(aGlobal.Get()),
      mWindow(aWindow),
      mPromise(aPromise)
  {
    MOZ_ASSERT(aWindow);
    MOZ_ASSERT(aPromise);
    JSContext* cx = aGlobal.GetContext();
    JSAutoCompartment ac(cx, mGlobal);
    mNotifications = JS_NewArrayObject(cx, 0, nullptr);
    HoldData();
  }

  NS_IMETHOD Handle(const nsAString& aID,
                    const nsAString& aTitle,
                    const nsAString& aDir,
                    const nsAString& aLang,
                    const nsAString& aBody,
                    const nsAString& aTag,
                    const nsAString& aIcon,
                    JSContext* aCx)
  {
    MOZ_ASSERT(!aID.IsEmpty());
    MOZ_ASSERT(!aTitle.IsEmpty());

    NotificationOptions options;
    options.mDir = Notification::StringToDirection(nsString(aDir));
    options.mLang = aLang;
    options.mBody = aBody;
    options.mTag = aTag;
    options.mIcon = aIcon;
    nsRefPtr<Notification> notification = Notification::CreateInternal(mWindow,
                                                                       aID,
                                                                       aTitle,
                                                                       options);
    JSAutoCompartment ac(aCx, mGlobal);
    JS::RootedObject scope(aCx, mGlobal);
    JS::RootedObject element(aCx, notification->WrapObject(aCx, scope));
    NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);

    if (!JS_DefineElement(aCx, mNotifications, mCount++,
                          JS::ObjectValue(*element), nullptr, nullptr, 0)) {
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

  NS_IMETHOD Done(JSContext* aCx)
  {
    JSAutoCompartment ac(aCx, mGlobal);
    Optional<JS::HandleValue> result(aCx, JS::ObjectValue(*mNotifications));
    mPromise->MaybeResolve(aCx, result);
    return NS_OK;
  }

private:
  ~NotificationStorageCallback()
  {
    DropData();
  }

  void HoldData()
  {
    mozilla::HoldJSObjects(this);
  }

  void DropData()
  {
    mGlobal = nullptr;
    mNotifications = nullptr;
    mozilla::DropJSObjects(this);
  }

  uint32_t  mCount;
  JS::Heap<JSObject *> mGlobal;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsRefPtr<Promise> mPromise;
  JS::Heap<JSObject *> mNotifications;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(NotificationStorageCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(NotificationStorageCallback)
NS_IMPL_CYCLE_COLLECTION_CLASS(NotificationStorageCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NotificationStorageCallback)
  NS_INTERFACE_MAP_ENTRY(nsINotificationStorageCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(NotificationStorageCallback)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mNotifications)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(NotificationStorageCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(NotificationStorageCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromise)
  tmp->DropData();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

class NotificationPermissionRequest : public nsIContentPermissionRequest,
                                      public PCOMContentPermissionRequestChild,
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
      mCallback(aCallback) {}

  virtual ~NotificationPermissionRequest() {}

  bool Recv__delete__(const bool& aAllow);
  void IPDLRelease() { Release(); }

protected:
  nsresult CallCallback();
  nsresult DispatchCallback();
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  NotificationPermission mPermission;
  nsRefPtr<NotificationPermissionCallback> mCallback;
};

class NotificationObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  NotificationObserver(Notification* aNotification)
    : mNotification(aNotification) {}

  virtual ~NotificationObserver() {}

protected:
  nsRefPtr<Notification> mNotification;
};

class NotificationTask : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  enum NotificationAction {
    eShow,
    eClose
  };

  NotificationTask(Notification* aNotification, NotificationAction aAction)
    : mNotification(aNotification), mAction(aAction) {}

  virtual ~NotificationTask() {}

protected:
  nsRefPtr<Notification> mNotification;
  NotificationAction mAction;
};

uint32_t Notification::sCount = 0;

NS_IMPL_CYCLE_COLLECTION_1(NotificationPermissionRequest, mWindow)

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

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    // because owner implements nsITabChild, we can assume that it is
    // the one and only TabChild.
    TabChild* child = TabChild::GetFrom(mWindow->GetDocShell());
    if (!child) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    // Retain a reference so the object isn't deleted without IPDL's knowledge.
    // Corresponding release occurs in DeallocPContentPermissionRequest.
    AddRef();

    nsTArray<PermissionRequest> permArray;
    permArray.AppendElement(PermissionRequest(
                            NS_LITERAL_CSTRING("desktop-notification"),
                            NS_LITERAL_CSTRING("unused")));
    child->SendPContentPermissionRequestConstructor(this, permArray,
                                                    IPC::Principal(mPrincipal));

    Sendprompt();
    return NS_OK;
  }

  nsCOMPtr<nsIContentPermissionPrompt> prompt =
    do_GetService(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
  if (prompt) {
    prompt->Prompt(this);
  }

  return NS_OK;
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
NotificationPermissionRequest::Allow()
{
  mPermission = NotificationPermission::Granted;
  return DispatchCallback();
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
  return rv.ErrorCode();
}

NS_IMETHODIMP
NotificationPermissionRequest::GetTypes(nsIArray** aTypes)
{
  return CreatePermissionArray(NS_LITERAL_CSTRING("desktop-notification"),
                               NS_LITERAL_CSTRING("unused"),
                               aTypes);
}

bool
NotificationPermissionRequest::Recv__delete__(const bool& aAllow)
{
  if (aAllow) {
    (void) Allow();
  } else {
    (void) Cancel();
  }
  return true;
}

NS_IMPL_ISUPPORTS1(NotificationTask, nsIRunnable)

NS_IMETHODIMP
NotificationTask::Run()
{
  switch (mAction) {
  case eShow:
    mNotification->ShowInternal();
    break;
  case eClose:
    mNotification->CloseInternal();
    break;
  default:
    MOZ_CRASH("Unexpected action for NotificationTask.");
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(NotificationObserver, nsIObserver)

NS_IMETHODIMP
NotificationObserver::Observe(nsISupports* aSubject, const char* aTopic,
                              const PRUnichar* aData)
{
  if (!strcmp("alertclickcallback", aTopic)) {
    mNotification->DispatchTrustedEvent(NS_LITERAL_STRING("click"));
  } else if (!strcmp("alertfinished", aTopic)) {
    mNotification->mIsClosed = true;
    mNotification->DispatchTrustedEvent(NS_LITERAL_STRING("close"));
  } else if (!strcmp("alertshow", aTopic)) {
    mNotification->DispatchTrustedEvent(NS_LITERAL_STRING("show"));
  }

  return NS_OK;
}

Notification::Notification(const nsAString& aID, const nsAString& aTitle, const nsAString& aBody,
                           NotificationDirection aDir, const nsAString& aLang,
                           const nsAString& aTag, const nsAString& aIconUrl)
  : mID(aID), mTitle(aTitle), mBody(aBody), mDir(aDir), mLang(aLang),
    mTag(aTag), mIconUrl(aIconUrl), mIsClosed(false)
{
  SetIsDOMBinding();
}

// static
already_AddRefed<Notification>
Notification::Constructor(const GlobalObject& aGlobal,
                          const nsAString& aTitle,
                          const NotificationOptions& aOptions,
                          ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(window, "Window should not be null.");
  nsRefPtr<Notification> notification = CreateInternal(window,
                                                       EmptyString(),
                                                       aTitle,
                                                       aOptions);

  // Queue a task to show the notification.
  nsCOMPtr<nsIRunnable> showNotificationTask =
    new NotificationTask(notification, NotificationTask::eShow);
  NS_DispatchToCurrentThread(showNotificationTask);

  // Persist the notification.
  nsresult rv;
  nsCOMPtr<nsINotificationStorage> notificationStorage =
    do_GetService(NS_NOTIFICATION_STORAGE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsString origin;
  aRv = GetOrigin(window, origin);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsString id;
  notification->GetID(id);
  aRv = notificationStorage->Put(origin,
                                 id,
                                 aTitle,
                                 DirectionToString(aOptions.mDir),
                                 aOptions.mLang,
                                 aOptions.mBody,
                                 aOptions.mTag,
                                 aOptions.mIcon);
  if (aRv.Failed()) {
    return nullptr;
  }

  return notification.forget();
}

already_AddRefed<Notification>
Notification::CreateInternal(nsPIDOMWindow* aWindow,
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

  nsRefPtr<Notification> notification = new Notification(id,
                                                         aTitle,
                                                         aOptions.mBody,
                                                         aOptions.mDir,
                                                         aOptions.mLang,
                                                         aOptions.mTag,
                                                         aOptions.mIcon);

  notification->BindToOwner(aWindow);
  return notification.forget();
}

void
Notification::ShowInternal()
{
  nsCOMPtr<nsIAlertsService> alertService =
    do_GetService(NS_ALERTSERVICE_CONTRACTID);

  ErrorResult result;
  if (GetPermissionInternal(GetOwner(), result) !=
    NotificationPermission::Granted || !alertService) {
    // We do not have permission to show a notification or alert service
    // is not available.
    DispatchTrustedEvent(NS_LITERAL_STRING("error"));
    return;
  }

  nsresult rv;
  nsAutoString absoluteUrl;
  if (mIconUrl.Length() > 0) {
    // Resolve image URL against document base URI.
    nsIDocument* doc = GetOwner()->GetExtantDoc();
    if (doc) {
      nsCOMPtr<nsIURI> baseUri = doc->GetBaseURI();
      if (baseUri) {
        nsCOMPtr<nsIURI> srcUri;
        rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(srcUri),
                                                       mIconUrl, doc, baseUri);
        if (NS_SUCCEEDED(rv)) {
          nsAutoCString src;
          srcUri->GetSpec(src);
          absoluteUrl = NS_ConvertUTF8toUTF16(src);
        }
      }

    }
  }

  nsCOMPtr<nsIObserver> observer = new NotificationObserver(this);

  nsString alertName;
  rv = GetAlertName(alertName);
  NS_ENSURE_SUCCESS_VOID(rv);

#ifdef MOZ_B2G
  nsCOMPtr<nsIAppNotificationService> appNotifier =
    do_GetService("@mozilla.org/system-alerts-service;1");
  if (appNotifier) {
    nsCOMPtr<nsPIDOMWindow> window = GetOwner();
    uint32_t appId = (window.get())->GetDoc()->NodePrincipal()->GetAppId();

    if (appId != nsIScriptSecurityManager::UNKNOWN_APP_ID) {
      nsCOMPtr<nsIAppsService> appsService = do_GetService("@mozilla.org/AppsService;1");
      nsString manifestUrl = EmptyString();
      rv = appsService->GetManifestURLByLocalId(appId, manifestUrl);
      if (NS_SUCCEEDED(rv)) {
        mozilla::AutoSafeJSContext cx;
        JS::RootedValue val(cx);
        AppNotificationServiceOptions ops;
        ops.mTextClickable = true;
        ops.mManifestURL = manifestUrl;
        ops.mId = alertName;
        ops.mDir = DirectionToString(mDir);
        ops.mLang = mLang;

        if (!ops.ToObject(cx, JS::NullPtr(), &val)) {
          NS_WARNING("Converting dict to object failed!");
          return;
        }

        appNotifier->ShowAppNotification(mIconUrl, mTitle, mBody,
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
  alertService->ShowAlertNotification(absoluteUrl, mTitle, mBody, true,
                                      uniqueCookie, observer, alertName,
                                      DirectionToString(mDir), mLang);
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

NotificationPermission
Notification::GetPermission(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  return GetPermissionInternal(aGlobal.GetAsSupports(), aRv);
}

NotificationPermission
Notification::GetPermissionInternal(nsISupports* aGlobal, ErrorResult& aRv)
{
  // Get principal from global to check permission for notifications.
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aGlobal);
  if (!sop) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return NotificationPermission::Denied;
  }

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  if (nsContentUtils::IsSystemPrincipal(principal)) {
    return NotificationPermission::Granted;
  } else {
    // Allow files to show notifications by default.
    nsCOMPtr<nsIURI> uri;
    principal->GetURI(getter_AddRefs(uri));
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
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);

  permissionManager->TestPermissionFromPrincipal(principal,
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

already_AddRefed<Promise>
Notification::Get(const GlobalObject& aGlobal,
                  const GetNotificationOptions& aFilter,
                  ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(window);
  nsIDocument* doc = window->GetExtantDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsString origin;
  aRv = GetOrigin(window, origin);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsresult rv;
  nsCOMPtr<nsINotificationStorage> notificationStorage =
    do_GetService(NS_NOTIFICATION_STORAGE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsRefPtr<Promise> promise = new Promise(window);
  nsCOMPtr<nsINotificationStorageCallback> callback =
    new NotificationStorageCallback(aGlobal, window, promise);
  nsString tag = aFilter.mTag.WasPassed() ?
                 aFilter.mTag.Value() :
                 EmptyString();
  aRv = notificationStorage->Get(origin, tag, callback);
  if (aRv.Failed()) {
    return nullptr;
  }

  return promise.forget();
}

bool
Notification::PrefEnabled()
{
  return Preferences::GetBool("dom.webnotifications.enabled", false);
}

JSObject*
Notification::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return mozilla::dom::NotificationBinding::Wrap(aCx, aScope, this);
}

void
Notification::Close()
{
  // Queue a task to close the notification.
  nsCOMPtr<nsIRunnable> showNotificationTask =
    new NotificationTask(this, NotificationTask::eClose);
  NS_DispatchToMainThread(showNotificationTask);
}

void
Notification::CloseInternal()
{
  if (!mIsClosed) {
    nsresult rv;
    // Don't bail out if notification storage fails, since we still
    // want to send the close event through the alert service.
    nsCOMPtr<nsINotificationStorage> notificationStorage =
      do_GetService(NS_NOTIFICATION_STORAGE_CONTRACTID);
    if (notificationStorage) {
      nsString origin;
      rv = GetOrigin(GetOwner(), origin);
      if (NS_SUCCEEDED(rv)) {
        notificationStorage->Delete(origin, mID);
      }
    }

    nsCOMPtr<nsIAlertsService> alertService =
      do_GetService(NS_ALERTSERVICE_CONTRACTID);
    if (alertService) {
      nsString alertName;
      rv = GetAlertName(alertName);
      if (NS_SUCCEEDED(rv)) {
        alertService->CloseAlert(alertName);
      }
    }
  }
}

nsresult
Notification::GetOrigin(nsPIDOMWindow* aWindow, nsString& aOrigin)
{
  MOZ_ASSERT(aWindow);
  nsresult rv;
  nsIDocument* doc = aWindow->GetExtantDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);
  nsIPrincipal* principal = doc->NodePrincipal();
  NS_ENSURE_TRUE(principal, NS_ERROR_UNEXPECTED);

  uint16_t appStatus = principal->GetAppStatus();
  uint32_t appId = principal->GetAppId();

  if (appStatus == nsIPrincipal::APP_STATUS_NOT_INSTALLED ||
      appId == nsIScriptSecurityManager::NO_APP_ID ||
      appId == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    rv = nsContentUtils::GetUTFOrigin(principal, aOrigin);
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

nsresult
Notification::GetAlertName(nsString& aAlertName)
{
  // Get the notification name that is unique per origin + tag/ID.
  // The name of the alert is of the form origin#tag/ID.
  nsresult rv = GetOrigin(GetOwner(), aAlertName);
  NS_ENSURE_SUCCESS(rv, rv);
  aAlertName.AppendLiteral("#");
  if (!mTag.IsEmpty()) {
    aAlertName.Append(NS_LITERAL_STRING("tag:"));
    aAlertName.Append(mTag);
  } else {
    aAlertName.Append(NS_LITERAL_STRING("notag:"));
    aAlertName.Append(mID);
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

