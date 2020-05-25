/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozSelectableElements.h"

@implementation mozSelectableAccessible

- (NSArray*)accessibilityAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  static NSMutableArray* attributes = nil;

  if (!attributes) {
    attributes = [[super accessibilityAttributeNames] mutableCopy];
    [attributes addObject:NSAccessibilitySelectedChildrenAttribute];
  }

  return attributes;
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if ([attribute isEqualToString:NSAccessibilitySelectedChildrenAttribute]) {
    return YES;
  }

  return [super accessibilityIsAttributeSettable:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if ([attribute isEqualToString:NSAccessibilitySelectedChildrenAttribute] &&
      [value isKindOfClass:[NSArray class]]) {
    for (id child in [self selectableChildren]) {
      BOOL selected = [value indexOfObjectIdenticalTo:child] != NSNotFound;
      [child setSelected:selected];
    }
  } else {
    [super accessibilitySetValue:value forAttribute:attribute];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  if ([attribute isEqualToString:NSAccessibilitySelectedChildrenAttribute]) {
    return [self selectedChildren];
  }

  return [super accessibilityAttributeValue:attribute];
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

/**
 * Return the mozAccessibles that are selectable.
 */
- (NSArray*)selectableChildren {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [[self children]
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(mozAccessible* child,
                                                                        NSDictionary* bindings) {
        return [child isKindOfClass:[mozSelectableChildAccessible class]];
      }]];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

/**
 * Return the mozAccessibles that are actually selected.
 */
- (NSArray*)selectedChildren {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [[self children]
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(mozAccessible* child,
                                                                        NSDictionary* bindings) {
        // Return mozSelectableChildAccessibles that have are selected (truthy value).
        return [child isKindOfClass:[mozSelectableChildAccessible class]] &&
               [(mozSelectableChildAccessible*)child isSelected];
      }]];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

@end

@implementation mozSelectableChildAccessible

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  if ([attribute isEqualToString:NSAccessibilitySelectedAttribute]) {
    return [NSNumber numberWithBool:[self isSelected]];
  }

  return [super accessibilityAttributeValue:attribute];
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if ([attribute isEqualToString:NSAccessibilitySelectedAttribute]) {
    return YES;
  }

  return [super accessibilityIsAttributeSettable:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if ([attribute isEqualToString:NSAccessibilitySelectedAttribute]) {
    [self setSelected:[value boolValue]];
  } else {
    [super accessibilitySetValue:value forAttribute:attribute];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)isSelected {
  return [self stateWithMask:states::SELECTED] != 0;
}

- (void)setSelected:(BOOL)selected {
  // Get SELECTABLE and UNAVAILABLE state.
  uint64_t state = [self stateWithMask:(states::SELECTABLE | states::UNAVAILABLE)];
  if ((state & states::SELECTABLE) == 0 || (state & states::UNAVAILABLE) != 0) {
    // The object is either not selectable or is unavailable. Don't do anything.
    return;
  }

  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    accWrap->SetSelected(selected);
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    proxy->SetSelected(selected);
  }

  // We need to invalidate the state because the accessibility service
  // may check the selected attribute synchornously and not wait for
  // selection events.
  [self invalidateState];
}

@end

@implementation mozTabGroupAccessible

- (NSArray*)accessibilityAttributeNames {
  static NSMutableArray* attributes = nil;

  if (!attributes) {
    attributes = [[super accessibilityAttributeNames] mutableCopy];
    [attributes addObject:NSAccessibilityContentsAttribute];
    [attributes addObject:NSAccessibilityTabsAttribute];
  }

  return attributes;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityContentsAttribute]) return [super children];
  if ([attribute isEqualToString:NSAccessibilityTabsAttribute]) return [self selectableChildren];

  return [super accessibilityAttributeValue:attribute];
}

- (id)value {
  // The value of a tab group is its selected child. In the case
  // of multiple selections this will return the first one.
  return [[self selectedChildren] firstObject];
}

@end

@implementation mozTabAccessible

- (NSString*)roleDescription {
  return utils::LocalizedString(NS_LITERAL_STRING("tab"));
}

- (NSString*)accessibilityActionDescription:(NSString*)action {
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    return utils::LocalizedString(NS_LITERAL_STRING("switch"));
  }

  return [super accessibilityActionDescription:action];
}

- (id)value {
  // Retuens 1 if item is selected, 0 if not.
  return [NSNumber numberWithBool:[self isSelected]];
}

@end

@implementation mozListboxAccessible

- (BOOL)ignoreChild:(mozAccessible*)child {
  if (!child || child->mRole == roles::GROUPING) {
    return YES;
  }

  return [super ignoreChild:child];
}

- (BOOL)disableChild:(mozAccessible*)child {
  return ![child isKindOfClass:[mozSelectableChildAccessible class]];
}

- (NSArray*)accessibilityAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  static NSMutableArray* attributes = nil;

  if (!attributes) {
    attributes = [[super accessibilityAttributeNames] mutableCopy];
    // VoiceOver uses the availability of AXOrientation to make
    // an object an interaction group. The actual return value does
    // not matter.
    [attributes addObject:NSAccessibilityOrientationAttribute];
  }

  return attributes;
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

@end

@implementation mozOptionAccessible

- (NSString*)title {
  return @"";
}

- (id)value {
  // Swap title and value of option so it behaves more like a AXStaticText.
  return [super title];
}

@end

@implementation mozMenuAccessible

- (NSString*)title {
  return @"";
}

- (NSString*)accessibilityLabel {
  return @"";
}

- (void)postNotification:(NSString*)notification {
  [super postNotification:notification];

  if ([notification isEqualToString:@"AXMenuOpened"]) {
    mIsOpened = YES;
  } else if ([notification isEqualToString:@"AXMenuClosed"]) {
    mIsOpened = NO;
  }
}

- (void)expire {
  if (mIsOpened) {
    // VO needs to receive a menu closed event when the menu goes away.
    // If the menu is being destroyed, send a menu closed event first.
    [self postNotification:@"AXMenuClosed"];
  }

  [super expire];
}

@end

@implementation mozMenuItemAccessible

- (NSString*)accessibilityLabel {
  return @"";
}

- (NSArray*)accessibilityAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  static NSMutableArray* attributes = nil;

  if (!attributes) {
    attributes = [[super accessibilityAttributeNames] mutableCopy];
    [attributes addObject:@"AXMenuItemMarkChar"];
  }

  return attributes;
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([attribute isEqualToString:@"AXMenuItemMarkChar"]) {
    AccessibleWrap* accWrap = [self getGeckoAccessible];
    if (accWrap && accWrap->IsContent() &&
        accWrap->GetContent()->IsXULElement(nsGkAtoms::menuitem)) {
      // We need to provide a marker character. This is the visible "√" you see
      // on dropdown menus. In our a11y tree this is a single child text node
      // of the menu item.
      // We do this only with XUL menuitems that conform to the native theme, and not
      // with aria menu items that might have a pseudo element or something.
      if (accWrap->ChildCount() == 1 && accWrap->FirstChild()->Role() == roles::STATICTEXT) {
        nsAutoString marker;
        accWrap->FirstChild()->Name(marker);
        if (marker.Length() == 1) {
          return nsCocoaUtils::ToNSString(marker);
        }
      }
    }

    return nil;
  }

  return [super accessibilityAttributeValue:attribute];
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)isSelected {
  // Our focused state is equivelent to native selected states for menus.
  return [self stateWithMask:states::FOCUSED] != 0;
}

- (void)handleAccessibleEvent:(uint32_t)eventType {
  switch (eventType) {
    case nsIAccessibleEvent::EVENT_FOCUS:
      // Our focused state is equivelent to native selected states for menus.
      mozAccessible* parent = (mozAccessible*)[self parent];
      [parent postNotification:NSAccessibilitySelectedChildrenChangedNotification];
      break;
  }

  [super handleAccessibleEvent:eventType];
}

@end
