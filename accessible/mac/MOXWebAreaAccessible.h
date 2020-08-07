/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"

namespace mozilla {
namespace a11y {

class Pivot;
class PivotRule;

}
}

@interface MOXWebAreaAccessible : mozAccessible
// overrides
- (NSURL*)moxURL;

// overrides
- (NSNumber*)moxLoaded;

// overrides
- (NSNumber*)moxLoadingProgress;

// override
- (NSArray*)moxUIElementsForSearchPredicate:(NSDictionary*)searchPredicate;

// override
- (NSNumber*)moxUIElementCountForSearchPredicate:(NSDictionary*)searchPredicate;

// overrides
- (void)handleAccessibleEvent:(uint32_t)eventType;

@end

@interface MOXSearchInfo : NSObject {
  // The gecko accessible of the web area, we need a reference
  // to set the pivot's root. This is a weak ref.
  mozilla::a11y::AccessibleOrProxy mWebArea;

  // The gecko accessible we should start searching from.
  // This is a weak ref.
  mozilla::a11y::AccessibleOrProxy mStartElem;

  // The amount of matches we should return
  int mResultLimit;

  // The array of search keys to use during this search
  NSMutableArray* mSearchKeys;

  // Set to YES if we should search forward, NO if backward
  BOOL mSearchForward;

  // Set to YES if we should match on immediate descendants only, NO otherwise
  BOOL mImmediateDescendantsOnly;
}

- (id)initWithParameters:(NSDictionary*)params andRoot:(mozilla::a11y::AccessibleOrProxy)root;

- (NSMutableArray*)getMatchesForRule:(mozilla::a11y::PivotRule&)rule;

- (NSArray*)performSearch;

- (void)dealloc;

@end
