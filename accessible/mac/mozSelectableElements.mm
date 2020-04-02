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
- (id)selectableChildren {
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
- (id)selectedChildren {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSMutableArray* selectedChildren = [[NSMutableArray alloc] init];
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    AutoTArray<Accessible*, 10> selectedItems;
    accWrap->SelectedItems(&selectedItems);
    for (uint64_t i = 0; i < selectedItems.Length(); i++) {
      mozAccessible* nativeChild;
      selectedItems.ElementAt(i)->GetNativeInterface((void**)&nativeChild);
      if (nativeChild) {
        [selectedChildren addObject:nativeChild];
      }
    }
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    AutoTArray<ProxyAccessible*, 10> selectedItems;
    proxy->SelectedItems(&selectedItems);
    for (uint64_t i = 0; i < selectedItems.Length(); i++) {
      if (mozAccessible* nativeChild = GetNativeFromProxy(selectedItems.ElementAt(i))) {
        [selectedChildren addObject:nativeChild];
      }
    }
  }

  return selectedChildren;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)value {
  // The value of a selectable is its selected child. In the case
  // of multiple selections this will return the first one.
  return [[self selectedChildren] firstObject];
}

@end

@implementation mozSelectableChildAccessible

- (id)value {
  return [NSNumber numberWithBool:[self stateWithMask:states::SELECTED] != 0];
}

@end

@implementation mozTabGroupAccessible

- (NSArray*)accessibilityAttributeNames {
  // standard attributes that are shared and supported by root accessible (AXMain) elements.
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

@end
