/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "Platform.h"
#include "ProxyAccessible.h"
#include "DocAccessibleParent.h"
#include "mozTableAccessible.h"

#include "nsAppShell.h"

namespace mozilla {
namespace a11y {

// Mac a11y whitelisting
static bool sA11yShouldBeEnabled = false;

bool ShouldA11yBeEnabled() {
  EPlatformDisabledState disabledState = PlatformDisabledState();
  return (disabledState == ePlatformIsForceEnabled) ||
         ((disabledState == ePlatformIsEnabled) && sA11yShouldBeEnabled);
}

void PlatformInit() {}

void PlatformShutdown() {}

void ProxyCreated(ProxyAccessible* aProxy, uint32_t) {
  // Pass in dummy state for now as retrieving proxy state requires IPC.
  // Note that we can use ProxyAccessible::IsTable* functions here because they
  // do not use IPC calls but that might change after bug 1210477.
  Class type;
  if (aProxy->IsTable())
    type = [mozTableAccessible class];
  else if (aProxy->IsTableRow())
    type = [mozTableRowAccessible class];
  else if (aProxy->IsTableCell())
    type = [mozTableCellAccessible class];
  else
    type = GetTypeFromRole(aProxy->Role());

  uintptr_t accWrap = reinterpret_cast<uintptr_t>(aProxy) | IS_PROXY;
  mozAccessible* mozWrapper = [[type alloc] initWithAccessible:accWrap];
  aProxy->SetWrapper(reinterpret_cast<uintptr_t>(mozWrapper));

  mozAccessible* nativeParent = nullptr;
  if (aProxy->IsDoc() && aProxy->AsDoc()->IsTopLevel()) {
    // If proxy is top level, the parent we need to invalidate the children of
    // will be a non-remote accessible.
    Accessible* outerDoc = aProxy->OuterDocOfRemoteBrowser();
    if (outerDoc) {
      nativeParent = GetNativeFromGeckoAccessible(outerDoc);
    }
  } else {
    // Non-top level proxies need proxy parents' children invalidated.
    ProxyAccessible* parent = aProxy->Parent();
    MOZ_ASSERT(parent ||
                   // It's expected that an OOP iframe might not have a parent yet.
                   (aProxy->IsDoc() && aProxy->AsDoc()->IsTopLevelInContentProcess()),
               "a non-top-level proxy is missing a parent?");
    nativeParent = parent ? GetNativeFromProxy(parent) : nullptr;
  }

  if (nativeParent) {
    [nativeParent invalidateChildren];
  }
}

void ProxyDestroyed(ProxyAccessible* aProxy) {
  mozAccessible* nativeParent = nil;
  if (aProxy->IsDoc() && aProxy->AsDoc()->IsTopLevel()) {
    // Invalidate native parent in parent process's children on proxy destruction
    Accessible* outerDoc = aProxy->OuterDocOfRemoteBrowser();
    if (outerDoc) {
      nativeParent = GetNativeFromGeckoAccessible(outerDoc);
    }
  } else {
    if (!aProxy->Document()->IsShutdown()) {
      // Only do if the document has not been shut down, else parent will return
      // garbage since we don't shut down children from top down.
      ProxyAccessible* parent = aProxy->Parent();
      // Invalidate proxy parent's children.
      if (parent) {
        nativeParent = GetNativeFromProxy(parent);
      }
    }
  }

  mozAccessible* wrapper = GetNativeFromProxy(aProxy);
  [wrapper expire];
  [wrapper release];
  aProxy->SetWrapper(0);

  if (nativeParent) {
    [nativeParent invalidateChildren];
  }
}

void ProxyEvent(ProxyAccessible* aProxy, uint32_t aEventType) {
  if (aEventType == nsIAccessibleEvent::EVENT_REORDER && aProxy->ChildrenCount() == 1 &&
      aProxy->ChildAt(0)->IsDoc()) {
    // This is a remote OuterDocAccessible. The reorder event indicates that Its
    // embedded document has been added or changed. If the document itself is
    // an existing Accessible, ProxyCreated won't have been called, so we won't
    // have invalidated native children. This can happen for in-process iframes
    // if the OuterDocAccessible is re-created (e.g. due to layout reflow).
    // It always happens for out-of-process iframes, as we must always call
    // ProxyCreated before DocAccessibleParent::AddChildDoc for those.
    mozAccessible* wrapper = GetNativeFromProxy(aProxy);
    if (wrapper) {
      [wrapper invalidateChildren];
    }
  }

  // ignore everything but focus-changed, value-changed, caret and selection
  // events for now.
  if (aEventType != nsIAccessibleEvent::EVENT_FOCUS &&
      aEventType != nsIAccessibleEvent::EVENT_VALUE_CHANGE &&
      aEventType != nsIAccessibleEvent::EVENT_TEXT_VALUE_CHANGE &&
      aEventType != nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED &&
      aEventType != nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED &&
      aEventType != nsIAccessibleEvent::EVENT_REORDER)
    return;

  mozAccessible* wrapper = GetNativeFromProxy(aProxy);
  if (wrapper) {
    [wrapper handleAccessibleEvent:aEventType];
  }
}

void ProxyStateChangeEvent(ProxyAccessible* aProxy, uint64_t aState, bool aEnabled) {
  mozAccessible* wrapper = GetNativeFromProxy(aProxy);
  if (wrapper) {
    [wrapper stateChanged:aState isEnabled:aEnabled];
  }
}

void ProxyCaretMoveEvent(ProxyAccessible* aTarget, int32_t aOffset) {
  mozAccessible* wrapper = GetNativeFromProxy(aTarget);
  if (wrapper) {
    [wrapper handleAccessibleEvent:nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED];
  }
}

void ProxyTextChangeEvent(ProxyAccessible*, const nsString&, int32_t, uint32_t, bool, bool) {}

void ProxyShowHideEvent(ProxyAccessible*, ProxyAccessible*, bool, bool) {}

void ProxySelectionEvent(ProxyAccessible* aTarget, ProxyAccessible* aWidget, uint32_t aEventType) {
  mozAccessible* wrapper = GetNativeFromProxy(aWidget);
  if (wrapper) {
    [wrapper handleAccessibleEvent:aEventType];
  }
}
}  // namespace a11y
}  // namespace mozilla

@interface GeckoNSApplication (a11y)
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute;
@end

@implementation GeckoNSApplication (a11y)

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  if ([attribute isEqualToString:@"AXEnhancedUserInterface"])
    mozilla::a11y::sA11yShouldBeEnabled = ([value intValue] == 1);

  return [super accessibilitySetValue:value forAttribute:attribute];
}

@end
