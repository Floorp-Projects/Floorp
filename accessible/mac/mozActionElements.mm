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
  if (!mGeckoAccessible->AsLocal()) return nil;

  nsDeckFrame* deckFrame =
      do_QueryFrame(mGeckoAccessible->AsLocal()->GetFrame());
  nsIFrame* selectedFrame = deckFrame ? deckFrame->GetSelectedBox() : nullptr;

  LocalAccessible* selectedAcc = nullptr;
  if (selectedFrame) {
    nsINode* node = selectedFrame->GetContent();
    selectedAcc = mGeckoAccessible->AsLocal()->Document()->GetAccessible(node);
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

- (id)moxValue {
  return [NSNumber numberWithDouble:mGeckoAccessible->CurValue()];
}

- (NSString*)moxValueDescription {
  nsAutoString valueDesc;
  mGeckoAccessible->Value(valueDesc);
  return nsCocoaUtils::ToNSString(valueDesc);
}
- (id)moxMinValue {
  return [NSNumber numberWithDouble:mGeckoAccessible->MinValue()];
}

- (id)moxMaxValue {
  return [NSNumber numberWithDouble:mGeckoAccessible->MaxValue()];
}

- (void)moxSetValue:(id)value {
  [self setValue:([value doubleValue])];
}

- (void)moxPerformIncrement {
  [self changeValueBySteps:1];
}

- (void)moxPerformDecrement {
  [self changeValueBySteps:-1];
}

- (NSString*)moxOrientation {
  RefPtr<AccAttributes> attributes = mGeckoAccessible->Attributes();
  if (attributes) {
    nsAutoString result;
    attributes->GetAttribute(nsGkAtoms::aria_orientation, result);
    if (result.Equals(u"horizontal"_ns)) {
      return NSAccessibilityHorizontalOrientationValue;
    } else if (result.Equals(u"vertical"_ns)) {
      return NSAccessibilityVerticalOrientationValue;
    }
  }

  return NSAccessibilityUnknownOrientationValue;
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
  MOZ_ASSERT(mGeckoAccessible, "mGeckoAccessible is null");

  double newValue =
      mGeckoAccessible->CurValue() + (mGeckoAccessible->Step() * factor);
  [self setValue:(newValue)];
}

/*
 * Updates the accessible's current value to the specified value
 */
- (void)setValue:(double)value {
  MOZ_ASSERT(mGeckoAccessible, "mGeckoAccessible is null");

  double min = mGeckoAccessible->MinValue();
  double max = mGeckoAccessible->MaxValue();

  if ((IsNaN(min) || value >= min) && (IsNaN(max) || value <= max)) {
    if (LocalAccessible* acc = mGeckoAccessible->AsLocal()) {
      acc->SetCurValue(value);
    } else {
      RemoteAccessible* proxy = mGeckoAccessible->AsRemote();
      proxy->SetCurValue(value);
    }
  }
}

@end
