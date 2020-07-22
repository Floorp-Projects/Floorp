/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozActionElements.h"

#import "MacUtils.h"
#include "Accessible-inl.h"
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
    return utils::GetAccAttr(self, "haspopup");
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

- (BOOL)moxIgnoreWithParent:(mozAccessible*)parent {
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

  return [super moxIgnoreWithParent:parent];
}

@end

@implementation mozRadioButtonAccessible

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  if ([self isExpired]) {
    return nil;
  }

  if ([attribute isEqualToString:NSAccessibilityLinkedUIElementsAttribute]) {
    if (HTMLRadioButtonAccessible* radioAcc =
            (HTMLRadioButtonAccessible*)mGeckoAccessible.AsAccessible()) {
      NSMutableArray* radioSiblings = [NSMutableArray new];
      Relation rel = radioAcc->RelationByType(RelationType::MEMBER_OF);
      Accessible* tempAcc;
      while ((tempAcc = rel.Next())) {
        [radioSiblings addObject:GetNativeFromGeckoAccessible(tempAcc)];
      }
      return radioSiblings;
    } else {
      ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
      nsTArray<ProxyAccessible*> accs = proxy->RelationByType(RelationType::MEMBER_OF);
      return utils::ConvertToNSArray(accs);
    }
  }

  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
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

- (id)moxValue {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [NSNumber numberWithInt:[self isChecked]];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

@end

@implementation mozPaneAccessible

- (NSArray*)moxChildren {
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

@end
