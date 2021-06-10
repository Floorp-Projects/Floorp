/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozActionElements.h"

#import "MacUtils.h"
#include "LocalAccessible-inl.h"
#include "DocAccessible.h"
#include "XULTabAccessible.h"
#include "HTMLFormControlAccessible.h"

#include "nsDeckFrame.h"
#include "nsObjCExceptions.h"

using namespace mozilla::a11y;

enum CheckboxValue {
  // these constants correspond to the values in the OS
  kUnchecked = 0,
  kChecked = 1,
  kMixed = 2
};

@implementation mozButtonAccessible

- (NSNumber*)moxHasPopup {
  return @([self stateWithMask:states::HASPOPUP] != 0);
}

- (NSString*)moxPopupValue {
  if ([self stateWithMask:states::HASPOPUP] != 0) {
    return utils::GetAccAttr(self, nsGkAtoms::aria_haspopup);
  }

  return nil;
}

@end

@implementation mozPopupButtonAccessible

- (NSString*)moxTitle {
  // Popup buttons don't have titles.
  return @"";
}

- (BOOL)moxBlockSelector:(SEL)selector {
  if (selector == @selector(moxHasPopup)) {
    return YES;
  }

  return [super moxBlockSelector:selector];
}

- (NSArray*)moxChildren {
  if ([self stateWithMask:states::EXPANDED] == 0) {
    // If the popup button is collapsed don't return its children.
    return @[];
  }

  return [super moxChildren];
}

- (void)stateChanged:(uint64_t)state isEnabled:(BOOL)enabled {
  [super stateChanged:state isEnabled:enabled];

  if (state == states::EXPANDED) {
    // If the EXPANDED state is updated, fire AXMenu events on the
    // popups child which is the actual menu.
    if (mozAccessible* popup = (mozAccessible*)[self childAt:0]) {
      [popup moxPostNotification:(enabled ? @"AXMenuOpened" : @"AXMenuClosed")];
    }
  }
}

@end

@implementation mozRadioButtonAccessible

- (NSArray*)moxLinkedUIElements {
  return [[self getRelationsByType:RelationType::MEMBER_OF]
      arrayByAddingObjectsFromArray:[super moxLinkedUIElements]];
}

@end

@implementation mozCheckboxAccessible

- (int)isChecked {
  // check if we're checked or in a mixed state
  uint64_t state =
      [self stateWithMask:(states::CHECKED | states::PRESSED | states::MIXED)];
  if (state & (states::CHECKED | states::PRESSED)) {
    return kChecked;
  }

  if (state & states::MIXED) {
    return kMixed;
  }

  return kUnchecked;
}

- (id)moxValue {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return [NSNumber numberWithInt:[self isChecked]];

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)stateChanged:(uint64_t)state isEnabled:(BOOL)enabled {
  [super stateChanged:state isEnabled:enabled];

  if (state & (states::CHECKED | states::PRESSED | states::MIXED)) {
    [self moxPostNotification:NSAccessibilityValueChangedNotification];
  }
}

@end

@implementation mozPaneAccessible

- (NSArray*)moxChildren {
  if (!mGeckoAccessible.AsAccessible()) return nil;

  nsDeckFrame* deckFrame =
      do_QueryFrame(mGeckoAccessible.AsAccessible()->GetFrame());
  nsIFrame* selectedFrame = deckFrame ? deckFrame->GetSelectedBox() : nullptr;

  LocalAccessible* selectedAcc = nullptr;
  if (selectedFrame) {
    nsINode* node = selectedFrame->GetContent();
    selectedAcc =
        mGeckoAccessible.AsAccessible()->Document()->GetAccessible(node);
  }

  if (selectedAcc) {
    mozAccessible* curNative = GetNativeFromGeckoAccessible(selectedAcc);
    if (curNative)
      return
          [NSArray arrayWithObjects:GetObjectOrRepresentedView(curNative), nil];
  }

  return nil;
}

@end

@implementation mozIncrementableAccessible

- (void)moxSetValue:(id)value {
  [self setValue:([value doubleValue])];
}

- (void)moxPerformIncrement {
  [self changeValueBySteps:1];
}

- (void)moxPerformDecrement {
  [self changeValueBySteps:-1];
}

- (void)handleAccessibleEvent:(uint32_t)eventType {
  switch (eventType) {
    case nsIAccessibleEvent::EVENT_TEXT_VALUE_CHANGE:
    case nsIAccessibleEvent::EVENT_VALUE_CHANGE:
      [self moxPostNotification:NSAccessibilityValueChangedNotification];
      break;
    default:
      [super handleAccessibleEvent:eventType];
      break;
  }
}

/*
 * Updates the accessible's current value by factor and step.
 *
 * factor: A signed integer representing the number of times to
 *    apply step to the current value. A positive value will increment,
 *    while a negative one will decrement.
 * step: An unsigned integer specified by the webauthor and indicating the
 *    amount by which to increment/decrement the current value.
 */
- (void)changeValueBySteps:(int)factor {
  MOZ_ASSERT(!mGeckoAccessible.IsNull(), "mGeckoAccessible is null");

  if (LocalAccessible* acc = mGeckoAccessible.AsAccessible()) {
    double newValue = acc->CurValue() + (acc->Step() * factor);
    [self setValue:(newValue)];
  } else {
    RemoteAccessible* proxy = mGeckoAccessible.AsProxy();
    double newValue = proxy->CurValue() + (proxy->Step() * factor);
    [self setValue:(newValue)];
  }
}

/*
 * Updates the accessible's current value to the specified value
 */
- (void)setValue:(double)value {
  MOZ_ASSERT(!mGeckoAccessible.IsNull(), "mGeckoAccessible is null");

  if (LocalAccessible* acc = mGeckoAccessible.AsAccessible()) {
    double min = acc->MinValue();
    double max = acc->MaxValue();
    // Because min and max are not required attributes, we first check
    // if the value is undefined. If this check fails,
    // the value is defined, and we verify our new value falls
    // within the bound (inclusive).
    if ((IsNaN(min) || value >= min) && (IsNaN(max) || value <= max)) {
      acc->SetCurValue(value);
    }
  } else {
    RemoteAccessible* proxy = mGeckoAccessible.AsProxy();
    double min = proxy->MinValue();
    double max = proxy->MaxValue();
    // As above, check if the value is within bounds.
    if ((IsNaN(min) || value >= min) && (IsNaN(max) || value <= max)) {
      proxy->SetCurValue(value);
    }
  }
}

@end
