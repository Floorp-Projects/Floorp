/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"
#include "Pivot.h"

using namespace mozilla::a11y;

@interface MOXSearchInfo : NSObject {
  // The MOX accessible of the web area, we need a reference
  // to set the pivot's root. This is a weak ref.
  MOXAccessibleBase* mRoot;

  // The MOX accessible we should start searching from.
  // This is a weak ref.
  MOXAccessibleBase* mStartElem;

  // The amount of matches we should return
  int mResultLimit;

  // The array of search keys to use during this search
  NSMutableArray* mSearchKeys;

  // Set to YES if we should search forward, NO if backward
  BOOL mSearchForward;

  // Set to YES if we should match on immediate descendants only, NO otherwise
  BOOL mImmediateDescendantsOnly;

  NSString* mSearchText;
}

- (id)initWithParameters:(NSDictionary*)params andRoot:(MOXAccessibleBase*)root;

- (NSArray*)performSearch;

- (void)dealloc;

@end
