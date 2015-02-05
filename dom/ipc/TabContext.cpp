/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TabContext.h"
#include "mozilla/dom/PTabContext.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/TabChild.h"
#include "nsIAppsService.h"
#include "nsIScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"

#define NO_APP_ID (nsIScriptSecurityManager::NO_APP_ID)

using namespace mozilla::dom::ipc;
using namespace mozilla::layout;

namespace mozilla {
namespace dom {

TabContext::TabContext()
  : mInitialized(false)
  , mOwnAppId(NO_APP_ID)
  , mContainingAppId(NO_APP_ID)
  , mScrollingBehavior(DEFAULT_SCROLLING)
  , mIsBrowser(false)
{
}

bool
TabContext::IsBrowserElement() const
{
  return mIsBrowser;
}

bool
TabContext::IsBrowserOrApp() const
{
  return HasOwnApp() || IsBrowserElement();
}

uint32_t
TabContext::OwnAppId() const
{
  return mOwnAppId;
}

already_AddRefed<mozIApplication>
TabContext::GetOwnApp() const
{
  nsCOMPtr<mozIApplication> ownApp = mOwnApp;
  return ownApp.forget();
}

bool
TabContext::HasOwnApp() const
{
  nsCOMPtr<mozIApplication> ownApp = GetOwnApp();
  return !!ownApp;
}

uint32_t
TabContext::BrowserOwnerAppId() const
{
  if (IsBrowserElement()) {
    return mContainingAppId;
  }
  return NO_APP_ID;
}

already_AddRefed<mozIApplication>
TabContext::GetBrowserOwnerApp() const
{
  nsCOMPtr<mozIApplication> ownerApp;
  if (IsBrowserElement()) {
    ownerApp = mContainingApp;
  }
  return ownerApp.forget();
}

bool
TabContext::HasBrowserOwnerApp() const
{
  nsCOMPtr<mozIApplication> ownerApp = GetBrowserOwnerApp();
  return !!ownerApp;
}

uint32_t
TabContext::AppOwnerAppId() const
{
  if (HasOwnApp()) {
    return mContainingAppId;
  }
  return NO_APP_ID;
}

already_AddRefed<mozIApplication>
TabContext::GetAppOwnerApp() const
{
  nsCOMPtr<mozIApplication> ownerApp;
  if (HasOwnApp()) {
    ownerApp = mContainingApp;
  }
  return ownerApp.forget();
}

bool
TabContext::HasAppOwnerApp() const
{
  nsCOMPtr<mozIApplication> ownerApp = GetAppOwnerApp();
  return !!ownerApp;
}

uint32_t
TabContext::OwnOrContainingAppId() const
{
  if (HasOwnApp()) {
    return mOwnAppId;
  }

  return mContainingAppId;
}

already_AddRefed<mozIApplication>
TabContext::GetOwnOrContainingApp() const
{
  nsCOMPtr<mozIApplication> ownOrContainingApp;
  if (HasOwnApp()) {
    ownOrContainingApp = mOwnApp;
  } else {
    ownOrContainingApp = mContainingApp;
  }

  return ownOrContainingApp.forget();
}

bool
TabContext::HasOwnOrContainingApp() const
{
  nsCOMPtr<mozIApplication> ownOrContainingApp = GetOwnOrContainingApp();
  return !!ownOrContainingApp;
}

bool
TabContext::SetTabContext(const TabContext& aContext)
{
  NS_ENSURE_FALSE(mInitialized, false);

  *this = aContext;
  mInitialized = true;

  return true;
}

bool
TabContext::SetTabContextForAppFrame(mozIApplication* aOwnApp,
                                     mozIApplication* aAppFrameOwnerApp,
                                     ScrollingBehavior aRequestedBehavior)
{
  NS_ENSURE_FALSE(mInitialized, false);

  // Get ids for both apps and only write to our member variables after we've
  // verified that this worked.
  uint32_t ownAppId = NO_APP_ID;
  if (aOwnApp) {
    nsresult rv = aOwnApp->GetLocalId(&ownAppId);
    NS_ENSURE_SUCCESS(rv, false);
    NS_ENSURE_TRUE(ownAppId != NO_APP_ID, false);
  }

  uint32_t containingAppId = NO_APP_ID;
  if (aAppFrameOwnerApp) {
    nsresult rv = aAppFrameOwnerApp->GetLocalId(&containingAppId);
    NS_ENSURE_SUCCESS(rv, false);
    NS_ENSURE_TRUE(containingAppId != NO_APP_ID, false);
  }

  mInitialized = true;
  mIsBrowser = false;
  mOwnAppId = ownAppId;
  mContainingAppId = containingAppId;
  mScrollingBehavior = aRequestedBehavior;
  mOwnApp = aOwnApp;
  mContainingApp = aAppFrameOwnerApp;
  return true;
}

bool
TabContext::SetTabContextForBrowserFrame(mozIApplication* aBrowserFrameOwnerApp,
                                         ScrollingBehavior aRequestedBehavior)
{
  NS_ENSURE_FALSE(mInitialized, false);

  uint32_t containingAppId = NO_APP_ID;
  if (aBrowserFrameOwnerApp) {
    nsresult rv = aBrowserFrameOwnerApp->GetLocalId(&containingAppId);
    NS_ENSURE_SUCCESS(rv, false);
    NS_ENSURE_TRUE(containingAppId != NO_APP_ID, false);
  }

  mInitialized = true;
  mIsBrowser = true;
  mOwnAppId = NO_APP_ID;
  mContainingAppId = containingAppId;
  mScrollingBehavior = aRequestedBehavior;
  mContainingApp = aBrowserFrameOwnerApp;
  return true;
}

bool
TabContext::SetTabContextForNormalFrame(ScrollingBehavior aRequestedBehavior)
{
  NS_ENSURE_FALSE(mInitialized, false);

  mInitialized = true;
  mScrollingBehavior = aRequestedBehavior;
  return true;
}

IPCTabContext
TabContext::AsIPCTabContext() const
{
  if (mIsBrowser) {
    return IPCTabContext(BrowserFrameIPCTabContext(mContainingAppId),
                         mScrollingBehavior);
  }

  return IPCTabContext(AppFrameIPCTabContext(mOwnAppId, mContainingAppId),
                       mScrollingBehavior);
}

static already_AddRefed<mozIApplication>
GetAppForId(uint32_t aAppId)
{
  nsCOMPtr<nsIAppsService> appsService = do_GetService(APPS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(appsService, nullptr);

  nsCOMPtr<mozIApplication> app;
  appsService->GetAppByLocalId(aAppId, getter_AddRefs(app));

  return app.forget();
}

MaybeInvalidTabContext::MaybeInvalidTabContext(const IPCTabContext& aParams)
  : mInvalidReason(nullptr)
{
  bool isBrowser = false;
  uint32_t ownAppId = NO_APP_ID;
  uint32_t containingAppId = NO_APP_ID;

  const IPCTabAppBrowserContext& appBrowser = aParams.appBrowserContext();
  switch(appBrowser.type()) {
    case IPCTabAppBrowserContext::TPopupIPCTabContext: {
      const PopupIPCTabContext &ipcContext = appBrowser.get_PopupIPCTabContext();

      TabContext *context;
      if (ipcContext.opener().type() == PBrowserOrId::TPBrowserParent) {
        context = TabParent::GetFrom(ipcContext.opener().get_PBrowserParent());
        if (context->IsBrowserElement() && !ipcContext.isBrowserElement()) {
          // If the TabParent corresponds to a browser element, then it can only
          // open other browser elements, for security reasons.  We should have
          // checked this before calling the TabContext constructor, so this is
          // a fatal error.
          mInvalidReason = "Child is-browser process tried to "
                           "open a non-browser tab.";
          return;
        }
      } else if (ipcContext.opener().type() == PBrowserOrId::TPBrowserChild) {
        context = static_cast<TabChild*>(ipcContext.opener().get_PBrowserChild());
      } else if (ipcContext.opener().type() == PBrowserOrId::TTabId) {
        // We should never get here because this PopupIPCTabContext is only
        // used for allocating a new tab id, not for allocating a PBrowser.
        mInvalidReason = "Child process tried to open an tab without the opener information.";
        return;
      } else {
        // This should be unreachable because PopupIPCTabContext::opener is not a
        // nullable field.
        mInvalidReason = "PopupIPCTabContext::opener was null (?!).";
        return;
      }

      // Browser elements can't nest other browser elements.  So if
      // our opener is browser element, we must be a new DOM window
      // opened by it.  In that case we inherit our containing app ID
      // (if any).
      //
      // Otherwise, we're a new app window and we inherit from our
      // opener app.
      if (ipcContext.isBrowserElement()) {
        isBrowser = true;
        ownAppId = NO_APP_ID;
        containingAppId = context->OwnOrContainingAppId();
      } else {
        isBrowser = false;
        ownAppId = context->mOwnAppId;
        containingAppId = context->mContainingAppId;
      }
      break;
    }
    case IPCTabAppBrowserContext::TAppFrameIPCTabContext: {
      const AppFrameIPCTabContext &ipcContext =
        appBrowser.get_AppFrameIPCTabContext();

      isBrowser = false;
      ownAppId = ipcContext.ownAppId();
      containingAppId = ipcContext.appFrameOwnerAppId();
      break;
    }
    case IPCTabAppBrowserContext::TBrowserFrameIPCTabContext: {
      const BrowserFrameIPCTabContext &ipcContext =
        appBrowser.get_BrowserFrameIPCTabContext();

      isBrowser = true;
      ownAppId = NO_APP_ID;
      containingAppId = ipcContext.browserFrameOwnerAppId();
      break;
    }
    case IPCTabAppBrowserContext::TVanillaFrameIPCTabContext: {
      isBrowser = false;
      ownAppId = NO_APP_ID;
      containingAppId = NO_APP_ID;
      break;
    }
    default: {
      MOZ_CRASH();
    }
  }

  nsCOMPtr<mozIApplication> ownApp = GetAppForId(ownAppId);
  if ((ownApp == nullptr) != (ownAppId == NO_APP_ID)) {
    mInvalidReason = "Got an ownAppId that didn't correspond to an app.";
    return;
  }

  nsCOMPtr<mozIApplication> containingApp = GetAppForId(containingAppId);
  if ((containingApp == nullptr) != (containingAppId == NO_APP_ID)) {
    mInvalidReason = "Got a containingAppId that didn't correspond to an app.";
    return;
  }

  bool rv;
  if (isBrowser) {
    rv = mTabContext.SetTabContextForBrowserFrame(containingApp,
                                                  aParams.scrollingBehavior());
  } else {
    rv = mTabContext.SetTabContextForAppFrame(ownApp,
                                              containingApp,
                                              aParams.scrollingBehavior());
  }

  if (!rv) {
    mInvalidReason = "Couldn't initialize TabContext.";
  }
}

bool
MaybeInvalidTabContext::IsValid()
{
  return mInvalidReason == nullptr;
}

const char*
MaybeInvalidTabContext::GetInvalidReason()
{
  return mInvalidReason;
}

const TabContext&
MaybeInvalidTabContext::GetTabContext()
{
  if (!IsValid()) {
    MOZ_CRASH("Can't GetTabContext() if !IsValid().");
  }

  return mTabContext;
}

} // namespace dom
} // namespace mozilla
