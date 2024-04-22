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

namespace mozilla::dom {

TabContext::TabContext()
    : mInitialized(false), mChromeOuterWindowID(0), mMaxTouchPoints(0) {}

uint64_t TabContext::ChromeOuterWindowID() const {
  return mChromeOuterWindowID;
}

bool TabContext::SetTabContext(const TabContext& aContext) {
  NS_ENSURE_FALSE(mInitialized, false);

  *this = aContext;
  mInitialized = true;

  return true;
}

bool TabContext::UpdateTabContextAfterSwap(const TabContext& aContext) {
  // This is only used after already initialized.
  MOZ_ASSERT(mInitialized);

  // The only permissable changes are to mChromeOuterWindowID.  All other fields
  // must match for the change to be accepted.

  mChromeOuterWindowID = aContext.mChromeOuterWindowID;
  return true;
}

bool TabContext::SetTabContext(uint64_t aChromeOuterWindowID,
                               uint32_t aMaxTouchPoints) {
  NS_ENSURE_FALSE(mInitialized, false);

  mInitialized = true;
  mChromeOuterWindowID = aChromeOuterWindowID;
  mMaxTouchPoints = aMaxTouchPoints;
  return true;
}

IPCTabContext TabContext::AsIPCTabContext() const {
  return IPCTabContext(
      FrameIPCTabContext(mChromeOuterWindowID, mMaxTouchPoints));
}

MaybeInvalidTabContext::MaybeInvalidTabContext(const IPCTabContext& aParams)
    : mInvalidReason(nullptr) {
  uint64_t chromeOuterWindowID = 0;
  uint32_t maxTouchPoints = 0;

  switch (aParams.type()) {
    case IPCTabContext::TPopupIPCTabContext: {
      const PopupIPCTabContext& ipcContext = aParams.get_PopupIPCTabContext();

      chromeOuterWindowID = ipcContext.chromeOuterWindowID();
      break;
    }
    case IPCTabContext::TFrameIPCTabContext: {
      const FrameIPCTabContext& ipcContext = aParams.get_FrameIPCTabContext();

      chromeOuterWindowID = ipcContext.chromeOuterWindowID();
      maxTouchPoints = ipcContext.maxTouchPoints();
      break;
    }
    default: {
      MOZ_CRASH();
    }
  }

  if (!mTabContext.SetTabContext(chromeOuterWindowID, maxTouchPoints)) {
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

}  // namespace mozilla::dom
