/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MUIAccessible.h"

// our protocol that we implement (so uikit widgets can talk to us)
#import "mozilla/a11y/MUIRootAccessibleProtocol.h"

/*
  The root accessible. It acts as a delegate to the UIKit child view.
*/
@interface MUIRootAccessible : MUIAccessible <MUIRootAccessibleProtocol> {
  id<MUIRootAccessibleProtocol> mParallelView;  // weak ref
}

// override
- (id)initWithAccessible:(mozilla::a11y::Accessible*)aAcc;

// override
- (BOOL)hasRepresentedView;

// override
- (id)representedView;

@end
