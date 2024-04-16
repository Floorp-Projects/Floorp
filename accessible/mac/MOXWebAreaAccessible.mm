/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MOXWebAreaAccessible.h"

#import "MOXSearchInfo.h"
#import "MacUtils.h"

#include "nsAccUtils.h"
#include "nsCocoaUtils.h"
#include "DocAccessible.h"
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
  if ([[self moxSubrole] isEqualToString:@"AXLandmarkApplication"]) {
    return utils::LocalizedString(u"application"_ns);
  }

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

- (NSString*)moxSubrole {
  // Steal the subrole internally mapped to the web area.
  return [mParent moxSubrole];
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

- (NSArray*)moxUIElementsForSearchPredicate:(NSDictionary*)searchPredicate {
  MOXSearchInfo* search =
      [[[MOXSearchInfo alloc] initWithParameters:searchPredicate
                                         andRoot:self] autorelease];

  return [search performSearch];
}

- (NSNumber*)moxUIElementCountForSearchPredicate:
    (NSDictionary*)searchPredicate {
  return [NSNumber
      numberWithDouble:[[self moxUIElementsForSearchPredicate:searchPredicate]
                           count]];
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

- (NSString*)moxRole {
  // The OS role is AXWebArea regardless of the gecko role
  // (APPLICATION or DOCUMENT).
  // If the web area has a role of APPLICATION, its root group will
  // reflect that in a subrole/description.
  return @"AXWebArea";
}

- (NSString*)moxRoleDescription {
  // The role description is "HTML Content" regardless of the gecko role
  // (APPLICATION or DOCUMENT)
  return utils::LocalizedString(u"htmlContent"_ns);
}

- (NSURL*)moxURL {
  if ([self isExpired]) {
    return nil;
  }

  nsAutoString url;
  MOZ_ASSERT(mGeckoAccessible->IsDoc());
  nsAccUtils::DocumentURL(mGeckoAccessible, url);

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
      MOZ_ASSERT(mGeckoAccessible->IsRemote() ||
                     mGeckoAccessible->AsLocal()->IsRoot() ||
                     mGeckoAccessible->AsLocal()->AsDoc()->ParentDocument(),
                 "Non-root doc without a parent!");
      if ((mGeckoAccessible->IsRemote() &&
           mGeckoAccessible->AsRemote()->IsDoc() &&
           mGeckoAccessible->AsRemote()->AsDoc()->IsTopLevel()) ||
          (mGeckoAccessible->IsLocal() &&
           !mGeckoAccessible->AsLocal()->IsRoot() &&
           mGeckoAccessible->AsLocal()->AsDoc()->ParentDocument() &&
           mGeckoAccessible->AsLocal()->AsDoc()->ParentDocument()->IsRoot())) {
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
    return @[ rootGroup ];
  }

  // There is no root group, expose the children here directly.
  return [super moxUnignoredChildren];
}

- (BOOL)moxBlockSelector:(SEL)selector {
  if (selector == @selector(moxSubrole)) {
    // Never expose a subrole for a web area.
    return YES;
  }

  if (selector == @selector(moxElementBusy)) {
    // Don't confuse aria-busy with a document's busy state.
    return YES;
  }

  return [super moxBlockSelector:selector];
}

- (void)moxPostNotification:(NSString*)notification {
  if (![notification isEqualToString:@"AXElementBusyChanged"]) {
    // Suppress AXElementBusyChanged since it uses gecko's BUSY state
    // to tell VoiceOver about aria-busy changes. We use that state
    // differently in documents.
    [super moxPostNotification:notification];
  }
}

- (id)rootGroup {
  NSArray* children = [super moxUnignoredChildren];
  if (mRole == roles::DOCUMENT && [children count] == 1 &&
      [[[children firstObject] moxUnignoredChildren] count] != 0) {
    // We only need a root group if our document:
    // (1) has multiple children, or
    // (2) a one child that is a leaf, or
    // (3) has a role other than the default document role
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
