/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XlibDisplay.h"

#include "mozilla/Assertions.h"

namespace mozilla::gfx {

XlibDisplay::XlibDisplay(Display* aDisplay, bool aOwned)
    : mDisplay(aDisplay), mOwned(aOwned) {
  MOZ_ASSERT(mDisplay);
}

XlibDisplay::~XlibDisplay() {
  if (mOwned) {
    XCloseDisplay(mDisplay);
  }
}

/* static */
std::shared_ptr<XlibDisplay> XlibDisplay::Borrow(Display* aDisplay) {
  if (!aDisplay) {
    return nullptr;
  }
  return std::shared_ptr<XlibDisplay>(new XlibDisplay(aDisplay, false));
}

/* static */
std::shared_ptr<XlibDisplay> XlibDisplay::Open(const char* aDisplayName) {
  Display* disp = XOpenDisplay(aDisplayName);
  if (!disp) {
    return nullptr;
  }
  return std::shared_ptr<XlibDisplay>(new XlibDisplay(disp, true));
}

}  // namespace mozilla::gfx
