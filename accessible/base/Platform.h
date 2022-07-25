/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_Platform_h
#define mozilla_a11y_Platform_h

#include <stdint.h>
#include "nsStringFwd.h"

#if defined(ANDROID)
#  include "nsTArray.h"
#  include "nsRect.h"
#endif

#ifdef MOZ_WIDGET_COCOA
#  include "mozilla/a11y/Role.h"
#endif

#if defined(XP_WIN)
#  include "Units.h"
#endif

namespace mozilla {
namespace a11y {

class RemoteAccessible;

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

#if defined(XP_WIN)
/*
 * Do we have AccessibleHandler.dll registered.
 */
bool IsHandlerRegistered();

/*
 * Name of platform service that instantiated accessibility
 */
void SetInstantiator(const uint32_t aInstantiatorPid);
bool GetInstantiator(nsIFile** aOutInstantiator);
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
 * called when a new RemoteAccessible is created, so the platform may setup a
 * wrapper for it, or take other action.
 */
void ProxyCreated(RemoteAccessible* aProxy);

/**
 * Called just before a RemoteAccessible is destroyed so its wrapper can be
 * disposed of and other action taken.
 */
void ProxyDestroyed(RemoteAccessible*);

/**
 * Callied when an event is fired on a proxied accessible.
 */
void ProxyEvent(RemoteAccessible* aTarget, uint32_t aEventType);
void ProxyStateChangeEvent(RemoteAccessible* aTarget, uint64_t aState,
                           bool aEnabled);

#if defined(XP_WIN)
void ProxyFocusEvent(RemoteAccessible* aTarget,
                     const LayoutDeviceIntRect& aCaretRect);
void ProxyCaretMoveEvent(RemoteAccessible* aTarget,
                         const LayoutDeviceIntRect& aCaretRect,
                         int32_t aGranularity);
#else
void ProxyCaretMoveEvent(RemoteAccessible* aTarget, int32_t aOffset,
                         bool aIsSelectionCollapsed, int32_t aGranularity);
#endif
void ProxyTextChangeEvent(RemoteAccessible* aTarget, const nsAString& aStr,
                          int32_t aStart, uint32_t aLen, bool aIsInsert,
                          bool aFromUser);
void ProxyShowHideEvent(RemoteAccessible* aTarget, RemoteAccessible* aParent,
                        bool aInsert, bool aFromUser);
void ProxySelectionEvent(RemoteAccessible* aTarget, RemoteAccessible* aWidget,
                         uint32_t aType);

#if defined(ANDROID)
void ProxyVirtualCursorChangeEvent(RemoteAccessible* aTarget,
                                   RemoteAccessible* aOldPosition,
                                   int32_t aOldStartOffset,
                                   int32_t aOldEndOffset,
                                   RemoteAccessible* aNewPosition,
                                   int32_t aNewStartOffset,
                                   int32_t aNewEndOffset, int16_t aReason,
                                   int16_t aBoundaryType, bool aFromUser);

void ProxyScrollingEvent(RemoteAccessible* aTarget, uint32_t aEventType,
                         uint32_t aScrollX, uint32_t aScrollY,
                         uint32_t aMaxScrollX, uint32_t aMaxScrollY);

void ProxyAnnouncementEvent(RemoteAccessible* aTarget,
                            const nsAString& aAnnouncement, uint16_t aPriority);

class BatchData;

void ProxyBatch(RemoteAccessible* aDocument, const uint64_t aBatchType,
                const nsTArray<RemoteAccessible*>& aAccessibles,
                const nsTArray<BatchData>& aData);

bool LocalizeString(const nsAString& aToken, nsAString& aLocalized);
#endif

#ifdef MOZ_WIDGET_COCOA
class TextRangeData;
void ProxyTextSelectionChangeEvent(RemoteAccessible* aTarget,
                                   const nsTArray<TextRangeData>& aSelection);

void ProxyRoleChangedEvent(RemoteAccessible* aTarget, const a11y::role& aRole);
#endif

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_Platform_h
