/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TabContext.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/TabChild.h"
#include "nsIAppsService.h"

using namespace mozilla::dom::ipc;
using namespace mozilla::layout;

namespace mozilla {
namespace dom {

TabContext::TabContext()
  : mInitialized(false)
  , mOwnAppId(nsIScriptSecurityManager::NO_APP_ID)
  , mContainingAppId(nsIScriptSecurityManager::NO_APP_ID)
  , mScrollingBehavior(DEFAULT_SCROLLING)
  , mIsBrowser(false)
{
}

TabContext::TabContext(const IPCTabContext& aParams)
  : mInitialized(true)
{
  const IPCTabAppBrowserContext& appBrowser = aParams.appBrowserContext();
  switch(appBrowser.type()) {
    case IPCTabAppBrowserContext::TPopupIPCTabContext: {
      const PopupIPCTabContext &ipcContext = appBrowser.get_PopupIPCTabContext();

      TabContext *context;
      if (ipcContext.openerParent()) {
        context = static_cast<TabParent*>(ipcContext.openerParent());
        if (context->IsBrowserElement() && !ipcContext.isBrowserElement()) {
          // If the TabParent corresponds to a browser element, then it can only
          // open other browser elements, for security reasons.  We should have
          // checked this before calling the TabContext constructor, so this is
          // a fatal error.
          MOZ_CRASH();
        }
      }
      else if (ipcContext.openerChild()) {
        context = static_cast<TabChild*>(ipcContext.openerChild());
      }
      else {
        // This should be unreachable because PopupIPCTabContext::opener is not a
        // nullable field.
        MOZ_CRASH();
      }

      // Browser elements can't nest other browser elements.  So if
      // our opener is browser element, we must be a new DOM window
      // opened by it.  In that case we inherit our containing app ID
      // (if any).
      //
      // Otherwise, we're a new app window and we inherit from our
      // opener app.
      if (ipcContext.isBrowserElement()) {
        mIsBrowser = true;
        mOwnAppId = nsIScriptSecurityManager::NO_APP_ID;
        mContainingAppId = context->OwnOrContainingAppId();
      }
      else {
        mIsBrowser = false;
        mOwnAppId = context->mOwnAppId;
        mContainingAppId = context->mContainingAppId;
      }
      break;
    }
    case IPCTabAppBrowserContext::TAppFrameIPCTabContext: {
      const AppFrameIPCTabContext &ipcContext =
        appBrowser.get_AppFrameIPCTabContext();

      mIsBrowser = false;
      mOwnAppId = ipcContext.ownAppId();
      mContainingAppId = ipcContext.appFrameOwnerAppId();
      break;
    }
    case IPCTabAppBrowserContext::TBrowserFrameIPCTabContext: {
      const BrowserFrameIPCTabContext &ipcContext =
        appBrowser.get_BrowserFrameIPCTabContext();

      mIsBrowser = true;
      mOwnAppId = nsIScriptSecurityManager::NO_APP_ID;
      mContainingAppId = ipcContext.browserFrameOwnerAppId();
      break;
    }
    case IPCTabAppBrowserContext::TVanillaFrameIPCTabContext: {
      mIsBrowser = false;
      mOwnAppId = nsIScriptSecurityManager::NO_APP_ID;
      mContainingAppId = nsIScriptSecurityManager::NO_APP_ID;
      break;
    }
    default: {
      MOZ_CRASH();
    }
  }

  mScrollingBehavior = aParams.scrollingBehavior();
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
  return GetAppForId(OwnAppId());
}

bool
TabContext::HasOwnApp() const
{
  return mOwnAppId != nsIScriptSecurityManager::NO_APP_ID;
}

uint32_t
TabContext::BrowserOwnerAppId() const
{
  if (mIsBrowser) {
    return mContainingAppId;
  }
  return nsIScriptSecurityManager::NO_APP_ID;
}

already_AddRefed<mozIApplication>
TabContext::GetBrowserOwnerApp() const
{
  return GetAppForId(BrowserOwnerAppId());
}

bool
TabContext::HasBrowserOwnerApp() const
{
  return BrowserOwnerAppId() != nsIScriptSecurityManager::NO_APP_ID;
}

uint32_t
TabContext::AppOwnerAppId() const
{
  if (mOwnAppId != nsIScriptSecurityManager::NO_APP_ID) {
    return mContainingAppId;
  }
  return nsIScriptSecurityManager::NO_APP_ID;
}

already_AddRefed<mozIApplication>
TabContext::GetAppOwnerApp() const
{
  return GetAppForId(AppOwnerAppId());
}

bool
TabContext::HasAppOwnerApp() const
{
  return AppOwnerAppId() != nsIScriptSecurityManager::NO_APP_ID;
}

uint32_t
TabContext::OwnOrContainingAppId() const
{
  if (mIsBrowser) {
    MOZ_ASSERT(mOwnAppId == nsIScriptSecurityManager::NO_APP_ID);
    return mContainingAppId;
  }

  if (mOwnAppId) {
    return mOwnAppId;
  }

  return mContainingAppId;
}

already_AddRefed<mozIApplication>
TabContext::GetOwnOrContainingApp() const
{
  return GetAppForId(OwnOrContainingAppId());
}

bool
TabContext::HasOwnOrContainingApp() const
{
  return OwnOrContainingAppId() != nsIScriptSecurityManager::NO_APP_ID;
}

bool
TabContext::SetTabContext(const TabContext& aContext)
{
  NS_ENSURE_FALSE(mInitialized, false);

  // Verify that we can actually get apps for the given ids.  This step gives us
  // confidence that HasX() returns true iff GetX() returns true.
  if (aContext.mOwnAppId != nsIScriptSecurityManager::NO_APP_ID) {
    nsCOMPtr<mozIApplication> app = GetAppForId(aContext.mOwnAppId);
    NS_ENSURE_TRUE(app, false);
  }

  if (aContext.mContainingAppId != nsIScriptSecurityManager::NO_APP_ID) {
    nsCOMPtr<mozIApplication> app = GetAppForId(aContext.mContainingAppId);
    NS_ENSURE_TRUE(app, false);
  }

  mInitialized = true;
  mIsBrowser = aContext.mIsBrowser;
  mOwnAppId = aContext.mOwnAppId;
  mContainingAppId = aContext.mContainingAppId;
  mScrollingBehavior = aContext.mScrollingBehavior;
  return true;
}

bool
TabContext::SetTabContextForAppFrame(mozIApplication* aOwnApp, mozIApplication* aAppFrameOwnerApp,
                                     ScrollingBehavior aRequestedBehavior)
{
  NS_ENSURE_FALSE(mInitialized, false);

  // Get ids for both apps and only write to our member variables after we've
  // verified that this worked.
  uint32_t ownAppId = nsIScriptSecurityManager::NO_APP_ID;
  if (aOwnApp) {
    nsresult rv = aOwnApp->GetLocalId(&ownAppId);
    NS_ENSURE_SUCCESS(rv, false);
  }

  uint32_t containingAppId = nsIScriptSecurityManager::NO_APP_ID;
  if (aAppFrameOwnerApp) {
    nsresult rv = aOwnApp->GetLocalId(&containingAppId);
    NS_ENSURE_SUCCESS(rv, false);
  }

  mInitialized = true;
  mIsBrowser = false;
  mOwnAppId = ownAppId;
  mContainingAppId = containingAppId;
  mScrollingBehavior = aRequestedBehavior;
  return true;
}

bool
TabContext::SetTabContextForBrowserFrame(mozIApplication* aBrowserFrameOwnerApp,
                                         ScrollingBehavior aRequestedBehavior)
{
  NS_ENSURE_FALSE(mInitialized, false);

  uint32_t containingAppId = nsIScriptSecurityManager::NO_APP_ID;
  if (aBrowserFrameOwnerApp) {
    nsresult rv = aBrowserFrameOwnerApp->GetLocalId(&containingAppId);
    NS_ENSURE_SUCCESS(rv, false);
  }

  mInitialized = true;
  mIsBrowser = true;
  mOwnAppId = nsIScriptSecurityManager::NO_APP_ID;
  mContainingAppId = containingAppId;
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

already_AddRefed<mozIApplication>
TabContext::GetAppForId(uint32_t aAppId) const
{
  if (aAppId == nsIScriptSecurityManager::NO_APP_ID) {
    return nullptr;
  }

  nsCOMPtr<nsIAppsService> appsService = do_GetService(APPS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(appsService, nullptr);

  nsCOMPtr<mozIDOMApplication> domApp;
  appsService->GetAppByLocalId(aAppId, getter_AddRefs(domApp));

  nsCOMPtr<mozIApplication> app = do_QueryInterface(domApp);
  return app.forget();
}

} // namespace dom
} // namespace mozilla
