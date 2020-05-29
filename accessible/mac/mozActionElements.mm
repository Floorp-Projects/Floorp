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
                        NSAccessibilityRequiredAttribute, NSAccessibilityHasPopupAttribute,
                        NSAccessibilityPopupValueAttribute,
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

  if ([attribute isEqualToString:NSAccessibilityHasPopupAttribute]) {
    return [NSNumber numberWithBool:[self hasPopup]];
  }

  if ([attribute isEqualToString:NSAccessibilityPopupValueAttribute]) {
    if ([self hasPopup]) {
      return utils::GetAccAttr(self, "haspopup");
    } else {
      return nil;
    }
  }

  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)hasPopup {
  return ([self stateWithMask:states::HASPOPUP] != 0);
}

@end

@implementation mozPopupButtonAccessible
- (NSString*)title {
  // Popup buttons don't have titles.
  return @"";
}

- (NSArray*)accessibilityAttributeNames {
  static NSMutableArray* supportedAttributes = nil;
  if (!supportedAttributes) {
    supportedAttributes = [[super accessibilityAttributeNames] mutableCopy];
    // We need to remove AXHasPopup from a AXPopupButton for it to be reported
    // as a popup button. Otherwise VO reports it as a button that has a popup
    // which is not consistent with Safari.
    [supportedAttributes removeObject:NSAccessibilityHasPopupAttribute];
    [supportedAttributes addObject:NSAccessibilityValueAttribute];
  }

  return supportedAttributes;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([attribute isEqualToString:NSAccessibilityHasPopupAttribute]) {
    // We need to null on AXHasPopup for it to be reported as a popup button.
    // Otherwise VO reports it as a button that has a popup which is not
    // consistent with Safari.
    return nil;
  }

  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSArray*)children {
  if ([self stateWithMask:states::EXPANDED] == 0) {
    // If the popup button is collapsed don't return its children.
    return @[];
  }

  return [super children];
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

- (BOOL)ignoreWithParent:(mozAccessible*)parent {
  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    if (acc->IsContent() && acc->GetContent()->IsXULElement(nsGkAtoms::menulist)) {
      // The way select elements work is that content has a COMBOBOX accessible, when it is clicked
      // it expands a COMBOBOX in our top-level main XUL window. The latter COMBOBOX is a stand-in
      // for the content one while it is expanded.
      // XXX: VO focus behaves weirdly if we expose the dummy XUL COMBOBOX in the tree.
      // We are only interested in its menu child.
      return YES;
    }
  }

  return [super ignoreWithParent:parent];
}

@end

@implementation mozCheckboxAccessible

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
  if ([self isExpired]) {
    return 0;
  }

  // By default this calls -[[mozAccessible children] count].
  // Since we don't cache mChildren. This is faster.
  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    return mGeckoAccessible.ChildCount() ? 1 : 0;
  }

  return [super accessibilityArrayAttributeCount:attribute];
}

- (NSArray*)children {
  if (!mGeckoAccessible.AsAccessible()) return nil;

  nsDeckFrame* deckFrame = do_QueryFrame(mGeckoAccessible.AsAccessible()->GetFrame());
  nsIFrame* selectedFrame = deckFrame ? deckFrame->GetSelectedBox() : nullptr;

  Accessible* selectedAcc = nullptr;
  if (selectedFrame) {
    nsINode* node = selectedFrame->GetContent();
    selectedAcc = mGeckoAccessible.AsAccessible()->Document()->GetAccessible(node);
  }

  if (selectedAcc) {
    mozAccessible* curNative = GetNativeFromGeckoAccessible(selectedAcc);
    if (curNative) return [NSArray arrayWithObjects:GetObjectOrRepresentedView(curNative), nil];
  }

  return nil;
}

@end

@implementation mozIncrementableAccessible

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

  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    double newVal = acc->CurValue() + (acc->Step() * factor);
    double min = acc->MinValue();
    double max = acc->MaxValue();
    if ((IsNaN(min) || newVal >= min) && (IsNaN(max) || newVal <= max)) {
      acc->SetCurValue(newVal);
    }
  } else if (ProxyAccessible* proxy = mGeckoAccessible.AsProxy()) {
    double newVal = proxy->CurValue() + (proxy->Step() * factor);
    double min = proxy->MinValue();
    double max = proxy->MaxValue();
    // Because min and max are not required attributes, we first check
    // if the value is undefined. If this check fails,
    // the value is defined, and we we verify our new value falls
    // within the bound (inclusive).
    if ((IsNaN(min) || newVal >= min) && (IsNaN(max) || newVal <= max)) {
      proxy->SetCurValue(newVal);
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
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

@end
