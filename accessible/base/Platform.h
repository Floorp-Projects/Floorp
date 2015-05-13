/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

namespace mozilla {
namespace a11y {

class ProxyAccessible;

enum EPlatformDisabledState {
  ePlatformIsForceEnabled = -1,
  ePlatformIsEnabled = 0,
  ePlatformIsDisabled = 1
};

/**
 * Return the platform disabled state.
 */
EPlatformDisabledState PlatformDisabledState();

#ifdef MOZ_ACCESSIBILITY_ATK
/**
 * Perform initialization that should be done as soon as possible, in order
 * to minimize startup time.
 * XXX: this function and the next defined in ApplicationAccessibleWrap.cpp
 */
void PreInit();
#endif

#if defined(MOZ_ACCESSIBILITY_ATK) || defined(XP_MACOSX)
/**
 * Is platform accessibility enabled.
 * Only used on linux with atk and MacOS for now.
 */
bool ShouldA11yBeEnabled();
#endif

/**
 * Called to initialize platform specific accessibility support.
 * Note this is called after internal accessibility support is initialized.
 */
void PlatformInit();

/**
 * Shutdown platform accessibility.
 * Note this is called before internal accessibility support is shutdown.
 */
void PlatformShutdown();

/**
 * called when a new ProxyAccessible is created, so the platform may setup a
 * wrapper for it, or take other action.
 */
void ProxyCreated(ProxyAccessible* aProxy, uint32_t aInterfaces);

/**
 * Called just before a ProxyAccessible is destroyed so its wrapper can be
 * disposed of and other action taken.
 */
void ProxyDestroyed(ProxyAccessible*);

/**
 * Callied when an event is fired on a proxied accessible.
 */
void ProxyEvent(ProxyAccessible* aTarget, uint32_t aEventType);
void ProxyStateChangeEvent(ProxyAccessible* aTarget, uint64_t aState,
                           bool aEnabled);
void ProxyCaretMoveEvent(ProxyAccessible* aTarget, int32_t aOffset);
} // namespace a11y
} // namespace mozilla

