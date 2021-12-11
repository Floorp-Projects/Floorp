/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FxRWindowManager.h"
#include "mozilla/Assertions.h"
#include "nsPIDOMWindow.h"
#include "mozilla/ClearOnShutdown.h"

#include "nsWindow.h"

static mozilla::StaticAutoPtr<FxRWindowManager> sFxrWinMgrInstance;

FxRWindowManager* FxRWindowManager::GetInstance() {
  if (sFxrWinMgrInstance == nullptr) {
    sFxrWinMgrInstance = new FxRWindowManager();
    ClearOnShutdown(&sFxrWinMgrInstance);
  }

  return sFxrWinMgrInstance;
}

FxRWindowManager::FxRWindowManager() : mWindow(nullptr) {}

// Track this new Firefox Reality window instance
void FxRWindowManager::AddWindow(nsPIDOMWindowOuter* aWindow) {
  if (mWindow != nullptr) {
    MOZ_CRASH("Only one window is supported");
  }

  mWindow = aWindow;
}

// Returns true if the window at the provided ID was created for Firefox Reality
bool FxRWindowManager::IsFxRWindow(uint64_t aOuterWindowID) {
  return (mWindow != nullptr) && (mWindow->WindowID() == aOuterWindowID);
}

// Returns true if the window was created for Firefox Reality
bool FxRWindowManager::IsFxRWindow(const nsWindow* aWindow) const {
  return (mWindow != nullptr) &&
         (aWindow ==
          mozilla::widget::WidgetUtils::DOMWindowToWidget(mWindow).take());
}

uint64_t FxRWindowManager::GetWindowID() const {
  MOZ_ASSERT(mWindow);
  return mWindow->WindowID();
}
