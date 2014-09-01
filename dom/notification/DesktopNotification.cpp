/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/DesktopNotification.h"
#include "mozilla/dom/DesktopNotificationBinding.h"
#include "mozilla/dom/AppNotificationServiceOptionsBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsContentPermissionHelper.h"
#include "nsXULAppAPI.h"
#include "mozilla/dom/PBrowserChild.h"
#include "nsIDOMDesktopNotification.h"
#include "mozilla/Preferences.h"
#include "nsGlobalWindow.h"
#include "nsIAppsService.h"
#include "nsIScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"
#include "PermissionMessageUtils.h"

namespace mozilla {
namespace dom {

/*
 * Simple Request
 */
class DesktopNotificationRequest : public nsIContentPermissionRequest
                                 , public nsRunnable
{
  virtual ~DesktopNotificationRequest()
  {
  }

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICONTENTPERMISSIONREQUEST

  explicit DesktopNotificationRequest(DesktopNotification* aNotification)
    : mDesktopNotification(aNotification) {}

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    nsCOMPtr<nsPIDOMWindow> window = mDesktopNotification->GetOwner();
    nsContentPermissionUtils::AskPermission(this, window);
    return NS_OK;
  }

  nsRefPtr<DesktopNotification> mDesktopNotification;
};

/* ------------------------------------------------------------------------ */
/* AlertServiceObserver                                                     */
/* ------------------------------------------------------------------------ */

NS_IMPL_ISUPPORTS(AlertServiceObserver, nsIObserver)

/* ------------------------------------------------------------------------ */
/* DesktopNotification                                                      */
/* ------------------------------------------------------------------------ */

uint32_t DesktopNotification::sCount = 0;

nsresult
DesktopNotification::PostDesktopNotification()
{
  if (!mObserver) {
    mObserver = new AlertServiceObserver(this);
  }

#ifdef MOZ_B2G
  nsCOMPtr<nsIAppNotificationService> appNotifier =
    do_GetService("@mozilla.org/system-alerts-service;1");
  if (appNotifier) {
    nsCOMPtr<nsPIDOMWindow> window = GetOwner();
    uint32_t appId = (window.get())->GetDoc()->NodePrincipal()->GetAppId();

    if (appId != nsIScriptSecurityManager::UNKNOWN_APP_ID) {
      nsCOMPtr<nsIAppsService> appsService = do_GetService("@mozilla.org/AppsService;1");
      nsString manifestUrl = EmptyString();
      appsService->GetManifestURLByLocalId(appId, manifestUrl);
      mozilla::AutoSafeJSContext cx;
      JS::Rooted<JS::Value> val(cx);
      AppNotificationServiceOptions ops;
      ops.mTextClickable = true;
      ops.mManifestURL = manifestUrl;

      if (!ToJSValue(cx, ops, &val)) {
        return NS_ERROR_FAILURE;
      }

      return appNotifier->ShowAppNotification(mIconURL, mTitle, mDescription,
                                              mObserver, val);
    }
  }
#endif

  nsCOMPtr<nsIAlertsService> alerts = do_GetService("@mozilla.org/alerts-service;1");
  if (!alerts) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Generate a unique name (which will also be used as a cookie) because
  // the nsIAlertsService will coalesce notifications with the same name.
  // In the case of IPC, the parent process will use the cookie to map
  // to nsIObservers, thus cookies must be unique to differentiate observers.
  nsString uniqueName = NS_LITERAL_STRING("desktop-notification:");
  uniqueName.AppendInt(sCount++);
  nsIPrincipal* principal = GetOwner()->GetDoc()->NodePrincipal();
  return alerts->ShowAlertNotification(mIconURL, mTitle, mDescription,
                                       true,
                                       uniqueName,
                                       mObserver,
                                       uniqueName,
                                       NS_LITERAL_STRING("auto"),
                                       EmptyString(),
                                       EmptyString(),
                                       principal);
}

DesktopNotification::DesktopNotification(const nsAString & title,
                                         const nsAString & description,
                                         const nsAString & iconURL,
                                         nsPIDOMWindow *aWindow,
                                         nsIPrincipal* principal)
  : DOMEventTargetHelper(aWindow)
  , mTitle(title)
  , mDescription(description)
  , mIconURL(iconURL)
  , mPrincipal(principal)
  , mAllow(false)
  , mShowHasBeenCalled(false)
{
  if (Preferences::GetBool("notification.disabled", false)) {
    return;
  }

  // If we are in testing mode (running mochitests, for example)
  // and we are suppose to allow requests, then just post an allow event.
  if (Preferences::GetBool("notification.prompt.testing", false) &&
      Preferences::GetBool("notification.prompt.testing.allow", true)) {
    mAllow = true;
  }
}

void
DesktopNotification::Init()
{
  nsRefPtr<DesktopNotificationRequest> request = new DesktopNotificationRequest(this);

  NS_DispatchToMainThread(request);
}

DesktopNotification::~DesktopNotification()
{
  if (mObserver) {
    mObserver->Disconnect();
  }
}

void
DesktopNotification::DispatchNotificationEvent(const nsString& aName)
{
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
  if (NS_SUCCEEDED(rv)) {
    // it doesn't bubble, and it isn't cancelable
    rv = event->InitEvent(aName, false, false);
    if (NS_SUCCEEDED(rv)) {
      event->SetTrusted(true);
      DispatchDOMEvent(nullptr, event, nullptr, nullptr);
    }
  }
}

nsresult
DesktopNotification::SetAllow(bool aAllow)
{
  mAllow = aAllow;

  // if we have called Show() already, lets go ahead and post a notification
  if (mShowHasBeenCalled && aAllow) {
    return PostDesktopNotification();
  }

  return NS_OK;
}

void
DesktopNotification::HandleAlertServiceNotification(const char *aTopic)
{
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  if (!strcmp("alertclickcallback", aTopic)) {
    DispatchNotificationEvent(NS_LITERAL_STRING("click"));
  } else if (!strcmp("alertfinished", aTopic)) {
    DispatchNotificationEvent(NS_LITERAL_STRING("close"));
  }
}

void
DesktopNotification::Show(ErrorResult& aRv)
{
  mShowHasBeenCalled = true;

  if (!mAllow) {
    return;
  }

  aRv = PostDesktopNotification();
}

JSObject*
DesktopNotification::WrapObject(JSContext* aCx)
{
  return DesktopNotificationBinding::Wrap(aCx, this);
}

/* ------------------------------------------------------------------------ */
/* DesktopNotificationCenter                                                */
/* ------------------------------------------------------------------------ */

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(DesktopNotificationCenter)
NS_IMPL_CYCLE_COLLECTING_ADDREF(DesktopNotificationCenter)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DesktopNotificationCenter)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DesktopNotificationCenter)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

already_AddRefed<DesktopNotification>
DesktopNotificationCenter::CreateNotification(const nsAString& aTitle,
                                              const nsAString& aDescription,
                                              const nsAString& aIconURL)
{
  MOZ_ASSERT(mOwner);

  nsRefPtr<DesktopNotification> notification =
    new DesktopNotification(aTitle,
                            aDescription,
                            aIconURL,
                            mOwner,
                            mPrincipal);
  notification->Init();
  return notification.forget();
}

JSObject*
DesktopNotificationCenter::WrapObject(JSContext* aCx)
{
  return DesktopNotificationCenterBinding::Wrap(aCx, this);
}

/* ------------------------------------------------------------------------ */
/* DesktopNotificationRequest                                               */
/* ------------------------------------------------------------------------ */

NS_IMPL_ISUPPORTS_INHERITED(DesktopNotificationRequest, nsRunnable,
                            nsIContentPermissionRequest)

NS_IMETHODIMP
DesktopNotificationRequest::GetPrincipal(nsIPrincipal * *aRequestingPrincipal)
{
  if (!mDesktopNotification) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_IF_ADDREF(*aRequestingPrincipal = mDesktopNotification->mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
DesktopNotificationRequest::GetWindow(nsIDOMWindow * *aRequestingWindow)
{
  if (!mDesktopNotification) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_IF_ADDREF(*aRequestingWindow = mDesktopNotification->GetOwner());
  return NS_OK;
}

NS_IMETHODIMP
DesktopNotificationRequest::GetElement(nsIDOMElement * *aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);
  *aElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
DesktopNotificationRequest::Cancel()
{
  nsresult rv = mDesktopNotification->SetAllow(false);
  mDesktopNotification = nullptr;
  return rv;
}

NS_IMETHODIMP
DesktopNotificationRequest::Allow(JS::HandleValue aChoices)
{
  MOZ_ASSERT(aChoices.isUndefined());
  nsresult rv = mDesktopNotification->SetAllow(true);
  mDesktopNotification = nullptr;
  return rv;
}

NS_IMETHODIMP
DesktopNotificationRequest::GetTypes(nsIArray** aTypes)
{
  nsTArray<nsString> emptyOptions;
  return nsContentPermissionUtils::CreatePermissionArray(NS_LITERAL_CSTRING("desktop-notification"),
                                                         NS_LITERAL_CSTRING("unused"),
                                                         emptyOptions,
                                                         aTypes);
}

} // namespace dom
} // namespace mozilla
