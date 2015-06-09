/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#import "mozAccessible.h"

// our protocol that we implement (so cocoa widgets can talk to us)
#import "mozAccessibleProtocol.h"

/*
  The root accessible. There is one per window.
  Created by the RootAccessibleWrap.
*/
@interface mozRootAccessible : mozAccessible
{
  // the mozView that we're representing.
  // all outside communication goes through the mozView.
  // in reality, it's just piping all calls to us, and we're
  // doing its dirty work!
  //
  // whenever someone asks who we are (e.g., a child asking
  // for its parent, or our parent asking for its child), we'll
  // respond the mozView. it is absolutely necessary for third-
  // party tools that we do this!
  //
  // /hwaara
  id <mozView, mozAccessible> mParallelView; // weak ref
}
@end
