/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TabContext.h"
#include "mozilla/dom/PTabContext.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::dom::ipc;
using namespace mozilla::layout;

namespace mozilla {
namespace dom {

TabContext::TabContext()
    : mInitialized(false),
      mChromeOuterWindowID(0),
      mJSPluginID(-1),
      mShowFocusRings(UIStateChangeType_NoChange),
      mMaxTouchPoints(0) {}

bool TabContext::IsJSPlugin() const { return mJSPluginID >= 0; }

int32_t TabContext::JSPluginId() const { return mJSPluginID; }

uint64_t TabContext::ChromeOuterWindowID() const {
  return mChromeOuterWindowID;
}

bool TabContext::SetTabContext(const TabContext& aContext) {
  NS_ENSURE_FALSE(mInitialized, false);

  *this = aContext;
  mInitialized = true;

  return true;
}

void TabContext::SetPrivateBrowsingAttributes(bool aIsPrivateBrowsing) {
  mOriginAttributes.SyncAttributesWithPrivateBrowsing(aIsPrivateBrowsing);
}

void TabContext::SetFirstPartyDomainAttributes(
    const nsAString& aFirstPartyDomain) {
  mOriginAttributes.SetFirstPartyDomain(true, aFirstPartyDomain);
}

bool TabContext::UpdateTabContextAfterSwap(const TabContext& aContext) {
  // This is only used after already initialized.
  MOZ_ASSERT(mInitialized);

  // The only permissable changes are to mChromeOuterWindowID.  All other fields
  // must match for the change to be accepted.
  if (aContext.mOriginAttributes != mOriginAttributes) {
    return false;
  }

  mChromeOuterWindowID = aContext.mChromeOuterWindowID;
  return true;
}

const OriginAttributes& TabContext::OriginAttributesRef() const {
  return mOriginAttributes;
}

const nsAString& TabContext::PresentationURL() const {
  return mPresentationURL;
}

UIStateChangeType TabContext::ShowFocusRings() const { return mShowFocusRings; }

bool TabContext::SetTabContext(uint64_t aChromeOuterWindowID,
                               UIStateChangeType aShowFocusRings,
                               const OriginAttributes& aOriginAttributes,
                               const nsAString& aPresentationURL,
                               uint32_t aMaxTouchPoints) {
  NS_ENSURE_FALSE(mInitialized, false);

  mInitialized = true;
  mChromeOuterWindowID = aChromeOuterWindowID;
  mOriginAttributes = aOriginAttributes;
  mPresentationURL = aPresentationURL;
  mShowFocusRings = aShowFocusRings;
  mMaxTouchPoints = aMaxTouchPoints;
  return true;
}

bool TabContext::SetTabContextForJSPluginFrame(int32_t aJSPluginID) {
  NS_ENSURE_FALSE(mInitialized, false);

  mInitialized = true;
  mJSPluginID = aJSPluginID;
  return true;
}

IPCTabContext TabContext::AsIPCTabContext() const {
  if (IsJSPlugin()) {
    return IPCTabContext(JSPluginFrameIPCTabContext(mJSPluginID));
  }

  return IPCTabContext(
      FrameIPCTabContext(mOriginAttributes, mChromeOuterWindowID,
                         mPresentationURL, mShowFocusRings, mMaxTouchPoints));
}

MaybeInvalidTabContext::MaybeInvalidTabContext(const IPCTabContext& aParams)
    : mInvalidReason(nullptr) {
  uint64_t chromeOuterWindowID = 0;
  int32_t jsPluginId = -1;
  OriginAttributes originAttributes;
  nsAutoString presentationURL;
  UIStateChangeType showFocusRings = UIStateChangeType_NoChange;
  uint32_t maxTouchPoints = 0;

  switch (aParams.type()) {
    case IPCTabContext::TPopupIPCTabContext: {
      const PopupIPCTabContext& ipcContext = aParams.get_PopupIPCTabContext();

      TabContext* context;
      if (ipcContext.opener().type() == PBrowserOrId::TPBrowserParent) {
        context =
            BrowserParent::GetFrom(ipcContext.opener().get_PBrowserParent());
        if (!context) {
          mInvalidReason =
              "Child is-browser process tried to "
              "open a null tab.";
          return;
        }
      } else if (ipcContext.opener().type() == PBrowserOrId::TPBrowserChild) {
        context =
            static_cast<BrowserChild*>(ipcContext.opener().get_PBrowserChild());
      } else if (ipcContext.opener().type() == PBrowserOrId::TTabId) {
        // We should never get here because this PopupIPCTabContext is only
        // used for allocating a new tab id, not for allocating a PBrowser.
        mInvalidReason =
            "Child process tried to open an tab without the opener "
            "information.";
        return;
      } else {
        // This should be unreachable because PopupIPCTabContext::opener is not
        // a nullable field.
        mInvalidReason = "PopupIPCTabContext::opener was null (?!).";
        return;
      }

      originAttributes = context->mOriginAttributes;
      chromeOuterWindowID = ipcContext.chromeOuterWindowID();
      break;
    }
    case IPCTabContext::TJSPluginFrameIPCTabContext: {
      const JSPluginFrameIPCTabContext& ipcContext =
          aParams.get_JSPluginFrameIPCTabContext();

      jsPluginId = ipcContext.jsPluginId();
      break;
    }
    case IPCTabContext::TFrameIPCTabContext: {
      const FrameIPCTabContext& ipcContext = aParams.get_FrameIPCTabContext();

      chromeOuterWindowID = ipcContext.chromeOuterWindowID();
      presentationURL = ipcContext.presentationURL();
      showFocusRings = ipcContext.showFocusRings();
      originAttributes = ipcContext.originAttributes();
      maxTouchPoints = ipcContext.maxTouchPoints();
      break;
    }
    case IPCTabContext::TUnsafeIPCTabContext: {
      // XXXcatalinb: This used *only* by ServiceWorkerClients::OpenWindow.
      // It is meant as a temporary solution until service workers can
      // provide a BrowserChild equivalent. Don't allow this on b2g since
      // it might be used to escalate privileges.
      if (!StaticPrefs::dom_serviceWorkers_enabled()) {
        mInvalidReason = "ServiceWorkers should be enabled.";
        return;
      }

      break;
    }
    default: {
      MOZ_CRASH();
    }
  }

  bool rv;
  if (jsPluginId >= 0) {
    rv = mTabContext.SetTabContextForJSPluginFrame(jsPluginId);
  } else {
    rv = mTabContext.SetTabContext(chromeOuterWindowID, showFocusRings,
                                   originAttributes, presentationURL,
                                   maxTouchPoints);
  }
  if (!rv) {
    mInvalidReason = "Couldn't initialize TabContext.";
  }
}

bool MaybeInvalidTabContext::IsValid() { return mInvalidReason == nullptr; }

const char* MaybeInvalidTabContext::GetInvalidReason() {
  return mInvalidReason;
}

const TabContext& MaybeInvalidTabContext::GetTabContext() {
  if (!IsValid()) {
    MOZ_CRASH("Can't GetTabContext() if !IsValid().");
  }

  return mTabContext;
}

}  // namespace dom
}  // namespace mozilla
