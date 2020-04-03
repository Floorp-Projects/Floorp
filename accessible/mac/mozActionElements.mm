/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozActionElements.h"

#import "MacUtils.h"
#include "Accessible-inl.h"
#include "DocAccessible.h"
#include "XULTabAccessible.h"

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

- (NSArray*)accessibilityAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  static NSArray* attributes = nil;
  if (!attributes) {
    attributes = [[NSArray alloc]
        initWithObjects:NSAccessibilityParentAttribute,  // required
                        NSAccessibilityRoleAttribute,    // required
                        NSAccessibilityRoleDescriptionAttribute,
                        NSAccessibilityPositionAttribute,           // required
                        NSAccessibilitySizeAttribute,               // required
                        NSAccessibilityWindowAttribute,             // required
                        NSAccessibilityPositionAttribute,           // required
                        NSAccessibilityTopLevelUIElementAttribute,  // required
                        NSAccessibilityHelpAttribute,
                        NSAccessibilityEnabledAttribute,  // required
                        NSAccessibilityFocusedAttribute,  // required
                        NSAccessibilityTitleAttribute,    // required
                        NSAccessibilityChildrenAttribute, NSAccessibilityDescriptionAttribute,
#if DEBUG
                        @"AXMozDescription",
#endif
                        nil];
  }
  return attributes;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    if ([self hasPopup]) return [self children];
    return nil;
  }

  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)accessibilityIsIgnored {
  return ![self getGeckoAccessible] && ![self getProxyAccessible];
}

- (BOOL)hasPopup {
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    return accWrap->NativeState() & states::HASPOPUP;

  if (ProxyAccessible* proxy = [self getProxyAccessible])
    return proxy->NativeState() & states::HASPOPUP;

  return false;
}

@end

@implementation mozCheckboxAccessible

- (NSString*)accessibilityActionDescription:(NSString*)action {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([action isEqualToString:NSAccessibilityPressAction]) {
    if ([self isChecked] != kUnchecked) return @"uncheck checkbox";  // XXX: localize this later?

    return @"check checkbox";  // XXX: localize this later?
  }

  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (int)isChecked {
  // check if we're checked or in a mixed state
  uint64_t state = [self stateWithMask:(states::CHECKED | states::PRESSED | states::MIXED)];
  if (state & (states::CHECKED | states::PRESSED)) {
    return kChecked;
  }

  if (state & states::MIXED) {
    return kMixed;
  }

  return kUnchecked;
}

- (id)value {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [NSNumber numberWithInt:[self isChecked]];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

@end

@implementation mozPaneAccessible

- (NSUInteger)accessibilityArrayAttributeCount:(NSString*)attribute {
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  ProxyAccessible* proxy = [self getProxyAccessible];
  if (!accWrap && !proxy) return 0;

  // By default this calls -[[mozAccessible children] count].
  // Since we don't cache mChildren. This is faster.
  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    if (accWrap) return accWrap->ChildCount() ? 1 : 0;

    return proxy->ChildrenCount() ? 1 : 0;
  }

  return [super accessibilityArrayAttributeCount:attribute];
}

- (NSArray*)children {
  if (![self getGeckoAccessible]) return nil;

  nsDeckFrame* deckFrame = do_QueryFrame([self getGeckoAccessible]->GetFrame());
  nsIFrame* selectedFrame = deckFrame ? deckFrame->GetSelectedBox() : nullptr;

  Accessible* selectedAcc = nullptr;
  if (selectedFrame) {
    nsINode* node = selectedFrame->GetContent();
    selectedAcc = [self getGeckoAccessible]->Document() -> GetAccessible(node);
  }

  if (selectedAcc) {
    mozAccessible* curNative = GetNativeFromGeckoAccessible(selectedAcc);
    if (curNative) return [NSArray arrayWithObjects:GetObjectOrRepresentedView(curNative), nil];
  }

  return nil;
}

@end

@implementation mozSliderAccessible

- (NSArray*)accessibilityActionNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  NSArray* actions = [super accessibilityActionNames];

  static NSArray* sliderAttrs = nil;
  if (!sliderAttrs) {
    NSMutableArray* tempArray = [NSMutableArray new];
    [tempArray addObject:NSAccessibilityIncrementAction];
    [tempArray addObject:NSAccessibilityDecrementAction];
    sliderAttrs = [[NSArray alloc] initWithArray:tempArray];
    [tempArray release];
  }

  return [actions arrayByAddingObjectsFromArray:sliderAttrs];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)accessibilityPerformAction:(NSString*)action {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if ([action isEqualToString:NSAccessibilityIncrementAction]) {
    [self changeValueBySteps:1];
  } else if ([action isEqualToString:NSAccessibilityDecrementAction]) {
    [self changeValueBySteps:-1];
  } else {
    [super accessibilityPerformAction:action];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

/*
 * Updates the accessible's current value by (factor * step).
 * If incrementing factor should be positive, if decrementing
 * factor should be negative.
 */

- (void)changeValueBySteps:(int)factor {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    double newVal = accWrap->CurValue() + (accWrap->Step() * factor);
    if (newVal >= accWrap->MinValue() && newVal <= accWrap->MaxValue()) {
      accWrap->SetCurValue(newVal);
    }
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    double newVal = proxy->CurValue() + (proxy->Step() * factor);
    if (newVal >= proxy->MinValue() && newVal <= proxy->MaxValue()) {
      proxy->SetCurValue(newVal);
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)firePlatformEvent:(uint32_t)eventType {
  switch (eventType) {
    case nsIAccessibleEvent::EVENT_VALUE_CHANGE:
      [self postNotification:NSAccessibilityValueChangedNotification];
      break;
    default:
      [super firePlatformEvent:eventType];
      break;
  }
}

@end
