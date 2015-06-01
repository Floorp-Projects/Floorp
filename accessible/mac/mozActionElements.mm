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

- (NSArray*)accessibilityAttributeNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  static NSArray *attributes = nil;
  if (!attributes) {
    attributes = [[NSArray alloc] initWithObjects:NSAccessibilityParentAttribute, // required
                                                  NSAccessibilityRoleAttribute, // required
                                                  NSAccessibilityRoleDescriptionAttribute,
                                                  NSAccessibilityPositionAttribute, // required
                                                  NSAccessibilitySizeAttribute, // required
                                                  NSAccessibilityWindowAttribute, // required
                                                  NSAccessibilityPositionAttribute, // required
                                                  NSAccessibilityTopLevelUIElementAttribute, // required
                                                  NSAccessibilityHelpAttribute,
                                                  NSAccessibilityEnabledAttribute, // required
                                                  NSAccessibilityFocusedAttribute, // required
                                                  NSAccessibilityTitleAttribute, // required
                                                  NSAccessibilityChildrenAttribute,
                                                  NSAccessibilityDescriptionAttribute,
#if DEBUG
                                                  @"AXMozDescription",
#endif
                                                  nil];
  }
  return attributes;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    if ([self hasPopup])
      return [self children];
    return nil;
  }

  if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute]) {
    if ([self isTab])
      return utils::LocalizedString(NS_LITERAL_STRING("tab"));

    return NSAccessibilityRoleDescription([self role], nil);
  }

  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)accessibilityIsIgnored
{
  return ![self getGeckoAccessible];
}

- (NSArray*)accessibilityActionNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([self isEnabled]) {
    if ([self hasPopup])
      return [NSArray arrayWithObjects:NSAccessibilityPressAction,
              NSAccessibilityShowMenuAction,
              nil];
    return [NSArray arrayWithObject:NSAccessibilityPressAction];
  }
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString*)accessibilityActionDescription:(NSString*)action
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([action isEqualToString:NSAccessibilityPressAction]) {
    if ([self isTab])
      return utils::LocalizedString(NS_LITERAL_STRING("switch"));

    return @"press button"; // XXX: localize this later?
  }

  if ([self hasPopup]) {
    if ([action isEqualToString:NSAccessibilityShowMenuAction])
      return @"show menu";
  }

  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)accessibilityPerformAction:(NSString*)action
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if ([self isEnabled] && [action isEqualToString:NSAccessibilityPressAction]) {
    // TODO: this should bring up the menu, but currently doesn't.
    //       once msaa and atk have merged better, they will implement
    //       the action needed to show the menu.
    [self click];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)click
{
  // both buttons and checkboxes have only one action. we should really stop using arbitrary
  // arrays with actions, and define constants for these actions.
  [self getGeckoAccessible]->DoAction(0);
}

- (BOOL)isTab
{
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  return (accWrap && (accWrap->Role() == roles::PAGETAB));
}

- (BOOL)hasPopup
{
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  return accWrap && (accWrap->NativeState() & mozilla::a11y::states::HASPOPUP);
}

@end

@implementation mozCheckboxAccessible

- (NSString*)accessibilityActionDescription:(NSString*)action 
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([action isEqualToString:NSAccessibilityPressAction]) {
    if ([self isChecked] != kUnchecked)
      return @"uncheck checkbox"; // XXX: localize this later?
    
    return @"check checkbox"; // XXX: localize this later?
  }
  
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (int)isChecked
{
  uint64_t state = [self getGeckoAccessible]->NativeState();

  // check if we're checked or in a mixed state
  if (state & states::CHECKED) {
    return (state & states::MIXED) ? kMixed : kChecked;
  }
  
  return kUnchecked;
}

- (id)value
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [NSNumber numberWithInt:[self isChecked]];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

@end

@implementation mozTabsAccessible

- (void)dealloc
{
  [mTabs release];

  [super dealloc];
}

- (NSArray*)accessibilityAttributeNames
{
  // standard attributes that are shared and supported by root accessible (AXMain) elements.
  static NSMutableArray* attributes = nil;
  
  if (!attributes) {
    attributes = [[super accessibilityAttributeNames] mutableCopy];
    [attributes addObject:NSAccessibilityContentsAttribute];
    [attributes addObject:NSAccessibilityTabsAttribute];
  }
  
  return attributes;  
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{  
  if ([attribute isEqualToString:NSAccessibilityContentsAttribute])
    return [super children];
  if ([attribute isEqualToString:NSAccessibilityTabsAttribute])
    return [self tabs];
  
  return [super accessibilityAttributeValue:attribute];  
}

/**
 * Returns the selected tab (the mozAccessible)
 */
- (id)value
{
  if (![self getGeckoAccessible])
    return nil;

  Accessible* accessible = [self getGeckoAccessible]->GetSelectedItem(0);
  if (!accessible)
    return nil;

  mozAccessible* nativeAcc = nil;
  accessible->GetNativeInterface((void**)&nativeAcc);

  return nativeAcc;
}

/**
 * Return the mozAccessibles that are the tabs.
 */
- (id)tabs
{
  if (mTabs)
    return mTabs;

  NSArray* children = [self children];
  NSEnumerator* enumerator = [children objectEnumerator];
  mTabs = [[NSMutableArray alloc] init];
  
  id obj;
  while ((obj = [enumerator nextObject]))
    if ([obj isTab])
      [mTabs addObject:obj];

  return mTabs;
}

- (void)invalidateChildren
{
  [super invalidateChildren];

  [mTabs release];
  mTabs = nil;
}

@end

@implementation mozPaneAccessible

- (NSUInteger)accessibilityArrayAttributeCount:(NSString*)attribute
{
  if (![self getGeckoAccessible])
    return 0;

  // By default this calls -[[mozAccessible children] count].
  // Since we don't cache mChildren. This is faster.
  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
    return [self getGeckoAccessible]->ChildCount() ? 1 : 0;

  return [super accessibilityArrayAttributeCount:attribute];
}

- (NSArray*)children
{
  if (![self getGeckoAccessible])
    return nil;

  nsDeckFrame* deckFrame = do_QueryFrame([self getGeckoAccessible]->GetFrame());
  nsIFrame* selectedFrame = deckFrame ? deckFrame->GetSelectedBox() : nullptr;

  Accessible* selectedAcc = nullptr;
  if (selectedFrame) {
    nsINode* node = selectedFrame->GetContent();
    selectedAcc = [self getGeckoAccessible]->Document()->GetAccessible(node);
  }

  if (selectedAcc) {
    mozAccessible *curNative = GetNativeFromGeckoAccessible(selectedAcc);
    if (curNative)
      return [NSArray arrayWithObjects:GetObjectOrRepresentedView(curNative), nil];
  }

  return nil;
}

@end
