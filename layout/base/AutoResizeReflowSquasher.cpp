/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoResizeReflowSquasher.h"

namespace mozilla {

StaticRefPtr<PresShell> AutoResizeReflowSquasher::sPresShell;
bool AutoResizeReflowSquasher::sHasCapture = false;
nscoord AutoResizeReflowSquasher::sWidth = 0;
nscoord AutoResizeReflowSquasher::sHeight = 0;
ResizeReflowOptions AutoResizeReflowSquasher::sOptions =
    ResizeReflowOptions::NoOption;

AutoResizeReflowSquasher::AutoResizeReflowSquasher(PresShell* aShell) {
  // Don't allow nested AutoResizeReflowSquashers. Note that aShell may
  // be null, in which case this AutoResizeReflowSquasher should behave as
  // though it was never created. In this scenario nested instances are
  // allowed.
  MOZ_ASSERT(!sPresShell);
  sPresShell = aShell;
  MOZ_ASSERT(!sHasCapture);
}

AutoResizeReflowSquasher::~AutoResizeReflowSquasher() {
  RefPtr<PresShell> presShell = sPresShell;
  sPresShell = nullptr;
  if (sHasCapture) {
    presShell->ResizeReflow(sWidth, sHeight, sOptions);
    sHasCapture = false;
  }
}

bool AutoResizeReflowSquasher::CaptureResizeReflow(
    PresShell* aShell, nscoord aWidth, nscoord aHeight,
    ResizeReflowOptions aOptions) {
  if (!sPresShell) {
    return false;
  }
  if (sPresShell.get() != aShell) {
    return false;
  }
  if (!sHasCapture) {
    sHasCapture = true;
    sWidth = aWidth;
    sHeight = aHeight;
    sOptions = aOptions;
    return true;
  }

  MOZ_ASSERT(sWidth == aWidth);
  MOZ_ASSERT(sHeight == aHeight);
  MOZ_ASSERT(sOptions == aOptions);
  return true;
}

}  // namespace mozilla
