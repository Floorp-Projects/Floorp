/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Platform.h"
#include "RemoteAccessible.h"
#include "DocAccessibleParent.h"

#import "MUIAccessible.h"

namespace mozilla {
namespace a11y {

bool ShouldA11yBeEnabled() {
  // XXX: Figure out proper a11y activation strategies in iOS.
  return true;
}

void PlatformInit() {}

void PlatformShutdown() {}

void ProxyCreated(RemoteAccessible* aProxy) {
  MUIAccessible* mozWrapper = [[MUIAccessible alloc] initWithAccessible:aProxy];
  aProxy->SetWrapper(reinterpret_cast<uintptr_t>(mozWrapper));
}

void ProxyDestroyed(RemoteAccessible* aProxy) {
  MUIAccessible* wrapper = GetNativeFromGeckoAccessible(aProxy);
  [wrapper expire];
  [wrapper release];
  aProxy->SetWrapper(0);
}

void PlatformEvent(Accessible*, uint32_t) {}

void PlatformStateChangeEvent(Accessible*, uint64_t, bool) {}

void PlatformFocusEvent(Accessible* aTarget,
                        const LayoutDeviceIntRect& aCaretRect) {}

void PlatformCaretMoveEvent(Accessible* aTarget, int32_t aOffset,
                            bool aIsSelectionCollapsed, int32_t aGranularity,
                            const LayoutDeviceIntRect& aCaretRect,
                            bool aFromUser) {}

void PlatformTextChangeEvent(Accessible*, const nsAString&, int32_t, uint32_t,
                             bool, bool) {}

void PlatformShowHideEvent(Accessible*, Accessible*, bool, bool) {}

void PlatformSelectionEvent(Accessible*, Accessible*, uint32_t) {}

}  // namespace a11y
}  // namespace mozilla
