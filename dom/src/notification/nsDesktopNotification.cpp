/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDesktopNotification.h"

#include "nsContentPermissionHelper.h"
#include "nsXULAppAPI.h"

#include "mozilla/dom/PBrowserChild.h"
#include "TabChild.h"
#include "mozilla/Preferences.h"
#include "nsGlobalWindow.h"
#include "nsIAppsService.h"
#include "nsIDOMDesktopNotification.h"

using namespace mozilla;
using namespace mozilla::dom;

/* ------------------------------------------------------------------------ */
/* AlertServiceObserver                                                     */
/* ------------------------------------------------------------------------ */

NS_IMPL_ISUPPORTS1(AlertServiceObserver, nsIObserver)

/* ------------------------------------------------------------------------ */
/* nsDesktopNotification                                                    */
/* ------------------------------------------------------------------------ */

uint32_t nsDOMDesktopNotification::sCount = 0;

nsresult
nsDOMDesktopNotification::PostDesktopNotification()
{
  if (!mObserver)
    mObserver = new AlertServiceObserver(this);

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
      return appNotifier->ShowAppNotification(mIconURL, mTitle, mDescription,
                                              true,
                                              manifestUrl,
                                              mObserver);
    }
  }
#endif

  nsCOMPtr<nsIAlertsService> alerts = do_GetService("@mozilla.org/alerts-service;1");
  if (!alerts)
    return NS_ERROR_NOT_IMPLEMENTED;

  // Generate a unique name (which will also be used as a cookie) because
  // the nsIAlertsService will coalesce notifications with the same name.
  // In the case of IPC, the parent process will use the cookie to map
  // to nsIObservers, thus cookies must be unique to differentiate observers.
  nsString uniqueName = NS_LITERAL_STRING("desktop-notification:");
  uniqueName.AppendInt(sCount++);
  return alerts->ShowAlertNotification(mIconURL, mTitle, mDescription,
                                       true,
                                       uniqueName,
                                       mObserver,
                                       uniqueName,
                                       NS_LITERAL_STRING("auto"),
                                       EmptyString());
}

DOMCI_DATA(DesktopNotification, nsDOMDesktopNotification)

NS_INTERFACE_MAP_BEGIN(nsDOMDesktopNotification)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDesktopNotification)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DesktopNotification)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsDOMDesktopNotification, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsDOMDesktopNotification, nsDOMEventTargetHelper)

NS_IMPL_EVENT_HANDLER(nsDOMDesktopNotification, click)
NS_IMPL_EVENT_HANDLER(nsDOMDesktopNotification, close)

nsDOMDesktopNotification::nsDOMDesktopNotification(const nsAString & title,
                                                   const nsAString & description,
                                                   const nsAString & iconURL,
                                                   nsPIDOMWindow *aWindow,
                                                   nsIPrincipal* principal)
  : mTitle(title)
  , mDescription(description)
  , mIconURL(iconURL)
  , mPrincipal(principal)
  , mAllow(false)
  , mShowHasBeenCalled(false)
{
  BindToOwner(aWindow);
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
nsDOMDesktopNotification::Init()
{
  nsRefPtr<nsDesktopNotificationRequest> request = new nsDesktopNotificationRequest(this);

  // if we are in the content process, then remote it to the parent.
  if (XRE_GetProcessType() == GeckoProcessType_Content) {

    // if for some reason mOwner is null, just silently
    // bail.  The user will not see a notification, and that
    // is fine.
    if (!GetOwner())
      return;

    // because owner implements nsITabChild, we can assume that it is
    // the one and only TabChild for this docshell.
    TabChild* child = GetTabChildFrom(GetOwner()->GetDocShell());

    // Retain a reference so the object isn't deleted without IPDL's knowledge.
    // Corresponding release occurs in DeallocPContentPermissionRequest.
    nsRefPtr<nsDesktopNotificationRequest> copy = request;

    child->SendPContentPermissionRequestConstructor(copy.forget().get(),
                                                    NS_LITERAL_CSTRING("desktop-notification"),
                                                    NS_LITERAL_CSTRING("unused"),
                                                    IPC::Principal(mPrincipal));

    request->Sendprompt();
    return;
  }

  // otherwise, dispatch it
  NS_DispatchToMainThread(request);
}

nsDOMDesktopNotification::~nsDOMDesktopNotification()
{
  if (mObserver) {
    mObserver->Disconnect();
  }
}

void
nsDOMDesktopNotification::DispatchNotificationEvent(const nsString& aName)
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
nsDOMDesktopNotification::SetAllow(bool aAllow)
{
  mAllow = aAllow;

  // if we have called Show() already, lets go ahead and post a notification
  if (mShowHasBeenCalled && aAllow)
    return PostDesktopNotification();

  return NS_OK;
}

void
nsDOMDesktopNotification::HandleAlertServiceNotification(const char *aTopic)
{
  if (NS_FAILED(CheckInnerWindowCorrectness()))
    return;

  if (!strcmp("alertclickcallback", aTopic)) {
    DispatchNotificationEvent(NS_LITERAL_STRING("click"));
  } else if (!strcmp("alertfinished", aTopic)) {
    DispatchNotificationEvent(NS_LITERAL_STRING("close"));
  }
}

NS_IMETHODIMP
nsDOMDesktopNotification::Show()
{
  mShowHasBeenCalled = true;

  if (!mAllow)
    return NS_OK;

  return PostDesktopNotification();
}

/* ------------------------------------------------------------------------ */
/* nsDesktopNotificationCenter                                              */
/* ------------------------------------------------------------------------ */

DOMCI_DATA(DesktopNotificationCenter, nsDesktopNotificationCenter)

NS_INTERFACE_MAP_BEGIN(nsDesktopNotificationCenter)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDesktopNotificationCenter)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDesktopNotificationCenter)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DesktopNotificationCenter)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDesktopNotificationCenter)
NS_IMPL_RELEASE(nsDesktopNotificationCenter)

NS_IMETHODIMP
nsDesktopNotificationCenter::CreateNotification(const nsAString & title,
                                                const nsAString & description,
                                                const nsAString & iconURL,
                                                nsIDOMDesktopNotification **aResult)
{
  NS_ENSURE_STATE(mOwner);
  nsRefPtr<nsDOMDesktopNotification> notification = new nsDOMDesktopNotification(title, 
                                                                                 description,
                                                                                 iconURL,
                                                                                 mOwner,
                                                                                 mPrincipal);
  notification->Init();
  notification.forget(aResult);
  return NS_OK;
}


/* ------------------------------------------------------------------------ */
/* nsDesktopNotificationRequest                                             */
/* ------------------------------------------------------------------------ */

NS_IMPL_ISUPPORTS2(nsDesktopNotificationRequest,
                   nsIContentPermissionRequest,
                   nsIRunnable)

NS_IMETHODIMP
nsDesktopNotificationRequest::GetPrincipal(nsIPrincipal * *aRequestingPrincipal)
{
  if (!mDesktopNotification)
    return NS_ERROR_NOT_INITIALIZED;

  NS_IF_ADDREF(*aRequestingPrincipal = mDesktopNotification->mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsDesktopNotificationRequest::GetWindow(nsIDOMWindow * *aRequestingWindow)
{
  if (!mDesktopNotification)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMWindow> window =
    do_QueryInterface(mDesktopNotification->GetOwner());
  NS_IF_ADDREF(*aRequestingWindow = window);
  return NS_OK;
}

NS_IMETHODIMP
nsDesktopNotificationRequest::GetElement(nsIDOMElement * *aElement)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDesktopNotificationRequest::Cancel()
{
  nsresult rv = mDesktopNotification->SetAllow(false);
  mDesktopNotification = nullptr;
  return rv;
}

NS_IMETHODIMP
nsDesktopNotificationRequest::Allow()
{
  nsresult rv = mDesktopNotification->SetAllow(true);
  mDesktopNotification = nullptr;
  return rv;
}

NS_IMETHODIMP
nsDesktopNotificationRequest::GetType(nsACString & aType)
{
  aType = "desktop-notification";
  return NS_OK;
}

NS_IMETHODIMP
nsDesktopNotificationRequest::GetAccess(nsACString & aAccess)
{
  aAccess = "unused";
  return NS_OK;
}
