/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MOXWebAreaAccessible.h"

#include "nsCocoaUtils.h"
#include "DocAccessibleParent.h"

using namespace mozilla::a11y;

@implementation MOXRootGroup

- (id)initWithParent:(MOXWebAreaAccessible*)parent {
  // The parent is always a MOXWebAreaAccessible
  mParent = parent;
  return [super init];
}

- (NSString*)moxRole {
  return NSAccessibilityGroupRole;
}

- (NSString*)moxRoleDescription {
  return NSAccessibilityRoleDescription(NSAccessibilityGroupRole, nil);
}

- (id<mozAccessible>)moxParent {
  return mParent;
}

- (NSArray*)moxChildren {
  // Reparent the children of the web area here.
  return [mParent rootGroupChildren];
}

- (NSString*)moxIdentifier {
  // This is mostly for testing purposes to assert that this is the generated
  // root group.
  return @"root-group";
}

- (id)moxHitTest:(NSPoint)point {
  return [mParent moxHitTest:point];
}

- (NSValue*)moxPosition {
  return [mParent moxPosition];
}

- (NSValue*)moxSize {
  return [mParent moxSize];
}

- (BOOL)disableChild:(id)child {
  return NO;
}

- (void)expire {
  mParent = nil;
  [super expire];
}

- (BOOL)isExpired {
  MOZ_ASSERT((mParent == nil) == mIsExpired);

  return [super isExpired];
}

@end

@implementation MOXWebAreaAccessible

- (NSURL*)moxURL {
  if ([self isExpired]) {
    return nil;
  }

  nsAutoString url;
  if (mGeckoAccessible.IsAccessible()) {
    MOZ_ASSERT(mGeckoAccessible.AsAccessible()->IsDoc());
    DocAccessible* acc = mGeckoAccessible.AsAccessible()->AsDoc();
    acc->URL(url);
  } else {
    ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
    proxy->URL(url);
  }

  if (url.IsEmpty()) {
    return nil;
  }

  return [NSURL URLWithString:nsCocoaUtils::ToNSString(url)];
}

- (NSNumber*)moxLoaded {
  if ([self isExpired]) {
    return nil;
  }
  // We are loaded if we aren't busy or stale
  return @([self stateWithMask:(states::BUSY & states::STALE)] == 0);
}

// overrides
- (NSNumber*)moxLoadingProgress {
  if ([self isExpired]) {
    return nil;
  }

  if ([self stateWithMask:states::STALE] != 0) {
    // We expose stale state until the document is ready (DOM is loaded and tree
    // is constructed) so we indicate load hasn't started while this state is
    // present.
    return @0.0;
  }

  if ([self stateWithMask:states::BUSY] != 0) {
    // We expose state busy until the document and all its subdocuments are
    // completely loaded, so we indicate partial loading here
    return @0.5;
  }

  // if we are not busy and not stale, we are loaded
  return @1.0;
}

- (NSArray*)moxLinkUIElements {
  NSDictionary* searchPredicate = @{
    @"AXSearchKey" : @"AXLinkSearchKey",
    @"AXImmediateDescendantsOnly" : @NO,
    @"AXResultsLimit" : @(-1),
    @"AXDirection" : @"AXDirectionNext",
  };

  return [self moxUIElementsForSearchPredicate:searchPredicate];
}

- (void)handleAccessibleEvent:(uint32_t)eventType {
  switch (eventType) {
    case nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE:
      [self moxPostNotification:
                NSAccessibilityFocusedUIElementChangedNotification];
      if ((mGeckoAccessible.IsProxy() && mGeckoAccessible.AsProxy()->IsDoc() &&
           mGeckoAccessible.AsProxy()->AsDoc()->IsTopLevel()) ||
          (mGeckoAccessible.IsAccessible() &&
           !mGeckoAccessible.AsAccessible()->IsRoot() &&
           mGeckoAccessible.AsAccessible()
               ->AsDoc()
               ->ParentDocument()
               ->IsRoot())) {
        // we fire an AXLoadComplete event on top-level documents only
        [self moxPostNotification:@"AXLoadComplete"];
      } else {
        // otherwise the doc belongs to an iframe (IsTopLevelInContentProcess)
        // and we fire AXLayoutComplete instead
        [self moxPostNotification:@"AXLayoutComplete"];
      }
      break;
  }

  [super handleAccessibleEvent:eventType];
}

- (NSArray*)rootGroupChildren {
  // This method is meant to expose the doc's children to the root group.
  return [super moxChildren];
}

- (NSArray*)moxUnignoredChildren {
  if (id rootGroup = [self rootGroup]) {
    return @[ [self rootGroup] ];
  }

  // There is no root group, expose the children here directly.
  return [super moxUnignoredChildren];
}

- (id)rootGroup {
  NSArray* children = [super moxUnignoredChildren];
  if ([children count] == 1 &&
      [[[children firstObject] moxUnignoredChildren] count] != 0) {
    // We only need a root group if our document has multiple children or one
    // child that is a leaf.
    return nil;
  }

  if (!mRootGroup) {
    mRootGroup = [[MOXRootGroup alloc] initWithParent:self];
  }

  return mRootGroup;
}

- (void)expire {
  [mRootGroup expire];
  [super expire];
}

- (void)dealloc {
  // This object can only be dealoced after the gecko accessible wrapper
  // reference is released, and that happens after expire is called.
  MOZ_ASSERT([self isExpired]);
  [mRootGroup release];

  [super dealloc];
}

@end
