/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#import "MOXTextMarkerDelegate.h"

#include "Platform.h"
#include "RemoteAccessible.h"
#include "DocAccessibleParent.h"
#include "mozTableAccessible.h"
#include "mozTextAccessible.h"
#include "MOXWebAreaAccessible.h"
#include "nsAccUtils.h"
#include "TextRange.h"

#include "nsAppShell.h"
#include "nsCocoaUtils.h"
#include "mozilla/Telemetry.h"

// Available from 10.13 onwards; test availability at runtime before using
@interface NSWorkspace (AvailableSinceHighSierra)
@property(readonly) BOOL isVoiceOverEnabled;
@property(readonly) BOOL isSwitchControlEnabled;
@end

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

void ProxyCreated(RemoteAccessible* aProxy) {
  if (aProxy->Role() == roles::WHITESPACE) {
    // We don't create a native object if we're child of a "flat" accessible;
    // for example, on OS X buttons shouldn't have any children, because that
    // makes the OS confused. We also don't create accessibles for <br>
    // (whitespace) elements.
    return;
  }

  // Pass in dummy state for now as retrieving proxy state requires IPC.
  // Note that we can use RemoteAccessible::IsTable* functions here because they
  // do not use IPC calls but that might change after bug 1210477.
  Class type;
  if (aProxy->IsTable()) {
    type = [mozTableAccessible class];
  } else if (aProxy->IsTableRow()) {
    type = [mozTableRowAccessible class];
  } else if (aProxy->IsTableCell()) {
    type = [mozTableCellAccessible class];
  } else if (aProxy->IsDoc()) {
    type = [MOXWebAreaAccessible class];
  } else {
    type = GetTypeFromRole(aProxy->Role());
  }

  mozAccessible* mozWrapper = [[type alloc] initWithAccessible:aProxy];
  aProxy->SetWrapper(reinterpret_cast<uintptr_t>(mozWrapper));
}

void ProxyDestroyed(RemoteAccessible* aProxy) {
  mozAccessible* wrapper = GetNativeFromGeckoAccessible(aProxy);
  [wrapper expire];
  [wrapper release];
  aProxy->SetWrapper(0);

  if (aProxy->IsDoc()) {
    [MOXTextMarkerDelegate destroyForDoc:aProxy];
  }
}

void PlatformEvent(Accessible* aTarget, uint32_t aEventType) {
  // Ignore event that we don't escape below, they aren't yet supported.
  if (aEventType != nsIAccessibleEvent::EVENT_ALERT &&
      aEventType != nsIAccessibleEvent::EVENT_VALUE_CHANGE &&
      aEventType != nsIAccessibleEvent::EVENT_TEXT_VALUE_CHANGE &&
      aEventType != nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE &&
      aEventType != nsIAccessibleEvent::EVENT_REORDER &&
      aEventType != nsIAccessibleEvent::EVENT_LIVE_REGION_ADDED &&
      aEventType != nsIAccessibleEvent::EVENT_LIVE_REGION_REMOVED &&
      aEventType != nsIAccessibleEvent::EVENT_NAME_CHANGE &&
      aEventType != nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED) {
    return;
  }

  mozAccessible* wrapper = GetNativeFromGeckoAccessible(aTarget);
  if (wrapper) {
    [wrapper handleAccessibleEvent:aEventType];
  }
}

void PlatformStateChangeEvent(Accessible* aTarget, uint64_t aState,
                              bool aEnabled) {
  mozAccessible* wrapper = GetNativeFromGeckoAccessible(aTarget);
  if (wrapper) {
    [wrapper stateChanged:aState isEnabled:aEnabled];
  }
}

void PlatformFocusEvent(Accessible* aTarget,
                        const LayoutDeviceIntRect& aCaretRect) {
  if (mozAccessible* wrapper = GetNativeFromGeckoAccessible(aTarget)) {
    [wrapper handleAccessibleEvent:nsIAccessibleEvent::EVENT_FOCUS];
  }
}

void PlatformCaretMoveEvent(Accessible* aTarget, int32_t aOffset,
                            bool aIsSelectionCollapsed, int32_t aGranularity,
                            const LayoutDeviceIntRect& aCaretRect) {
  mozAccessible* wrapper = GetNativeFromGeckoAccessible(aTarget);
  MOXTextMarkerDelegate* delegate = [MOXTextMarkerDelegate
      getOrCreateForDoc:nsAccUtils::DocumentFor(aTarget)];
  [delegate setCaretOffset:aTarget at:aOffset moveGranularity:aGranularity];
  if (aIsSelectionCollapsed) {
    // If selection is collapsed, invalidate selection.
    [delegate setSelectionFrom:aTarget at:aOffset to:aTarget at:aOffset];
  }

  if (wrapper) {
    if (mozTextAccessible* textAcc =
            static_cast<mozTextAccessible*>([wrapper moxEditableAncestor])) {
      [textAcc
          handleAccessibleEvent:nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED];
    } else {
      [wrapper
          handleAccessibleEvent:nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED];
    }
  }
}

void PlatformTextChangeEvent(Accessible* aTarget, const nsAString& aStr,
                             int32_t aStart, uint32_t aLen, bool aIsInsert,
                             bool aFromUser) {
  Accessible* acc = aTarget;
  // If there is a text input ancestor, use it as the event source.
  while (acc && GetTypeFromRole(acc->Role()) != [mozTextAccessible class]) {
    acc = acc->Parent();
  }
  mozAccessible* wrapper = GetNativeFromGeckoAccessible(acc ? acc : aTarget);
  [wrapper handleAccessibleTextChangeEvent:nsCocoaUtils::ToNSString(aStr)
                                  inserted:aIsInsert
                               inContainer:aTarget
                                        at:aStart];
}

void PlatformShowHideEvent(Accessible*, Accessible*, bool, bool) {}

void PlatformSelectionEvent(Accessible* aTarget, Accessible* aWidget,
                            uint32_t aEventType) {
  mozAccessible* wrapper = GetNativeFromGeckoAccessible(aWidget);
  if (wrapper) {
    [wrapper handleAccessibleEvent:aEventType];
  }
}

void PlatformTextSelectionChangeEvent(Accessible* aTarget,
                                      const nsTArray<TextRange>& aSelection) {
  if (aSelection.Length()) {
    MOXTextMarkerDelegate* delegate = [MOXTextMarkerDelegate
        getOrCreateForDoc:nsAccUtils::DocumentFor(aTarget)];
    // Cache the selection.
    [delegate setSelectionFrom:aSelection[0].StartContainer()
                            at:aSelection[0].StartOffset()
                            to:aSelection[0].EndContainer()
                            at:aSelection[0].EndOffset()];
  }

  mozAccessible* wrapper = GetNativeFromGeckoAccessible(aTarget);
  if (wrapper) {
    [wrapper
        handleAccessibleEvent:nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED];
  }
}

void PlatformRoleChangedEvent(Accessible* aTarget, const a11y::role& aRole,
                              uint8_t aRoleMapEntryIndex) {
  if (mozAccessible* wrapper = GetNativeFromGeckoAccessible(aTarget)) {
    [wrapper handleRoleChanged:aRole];
  }
}

}  // namespace a11y
}  // namespace mozilla

@interface GeckoNSApplication (a11y)
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute;
@end

@implementation GeckoNSApplication (a11y)

- (NSAccessibilityRole)accessibilityRole {
  // For ATs that don't request `AXEnhancedUserInterface` we need to enable
  // accessibility when a role is fetched. Not ideal, but this is needed
  // for such services as Voice Control.
  if (!mozilla::a11y::sA11yShouldBeEnabled) {
    [self accessibilitySetValue:@YES forAttribute:@"AXEnhancedUserInterface"];
  }
  return [super accessibilityRole];
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  if ([attribute isEqualToString:@"AXEnhancedUserInterface"]) {
    mozilla::a11y::sA11yShouldBeEnabled = ([value intValue] == 1);
    if (sA11yShouldBeEnabled) {
      // If accessibility should be enabled, log the appropriate client
      nsAutoString client;
      if ([[NSWorkspace sharedWorkspace]
              respondsToSelector:@selector(isVoiceOverEnabled)] &&
          [[NSWorkspace sharedWorkspace] isVoiceOverEnabled]) {
        client.Assign(u"VoiceOver"_ns);
      } else if ([[NSWorkspace sharedWorkspace]
                     respondsToSelector:@selector(isSwitchControlEnabled)] &&
                 [[NSWorkspace sharedWorkspace] isSwitchControlEnabled]) {
        client.Assign(u"SwitchControl"_ns);
      } else {
        // This is more complicated than the NSWorkspace queries above
        // because (a) there is no "full keyboard access" query for NSWorkspace
        // and (b) the [NSApplication fullKeyboardAccessEnabled] query checks
        // the pre-Monterey version of full keyboard access, which is not what
        // we're looking for here. For more info, see bug 1772375 comment 7.
        Boolean exists;
        int val = CFPreferencesGetAppIntegerValue(
            CFSTR("FullKeyboardAccessEnabled"),
            CFSTR("com.apple.Accessibility"), &exists);
        if (exists && val == 1) {
          client.Assign(u"FullKeyboardAccess"_ns);
        } else {
          val = CFPreferencesGetAppIntegerValue(
              CFSTR("CommandAndControlEnabled"),
              CFSTR("com.apple.Accessibility"), &exists);
          if (exists && val == 1) {
            client.Assign(u"VoiceControl"_ns);
          } else {
            client.Assign(u"Unknown"_ns);
          }
        }
      }

#if defined(MOZ_TELEMETRY_REPORTING)
      mozilla::Telemetry::ScalarSet(
          mozilla::Telemetry::ScalarID::A11Y_INSTANTIATORS, client);
#endif  // defined(MOZ_TELEMETRY_REPORTING)
      CrashReporter::AnnotateCrashReport(
          CrashReporter::Annotation::AccessibilityClient,
          NS_ConvertUTF16toUTF8(client));
    }
  }

  return [super accessibilitySetValue:value forAttribute:attribute];
}

@end
