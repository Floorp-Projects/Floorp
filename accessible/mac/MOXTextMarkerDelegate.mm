/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#import "MOXTextMarkerDelegate.h"

using namespace mozilla::a11y;

static nsDataHashtable<nsUint64HashKey, MOXTextMarkerDelegate*> sDelegates;

@implementation MOXTextMarkerDelegate

+ (id)getOrCreateForDoc:(mozilla::a11y::AccessibleOrProxy)aDoc {
  MOZ_ASSERT(!aDoc.IsNull());

  MOXTextMarkerDelegate* delegate = sDelegates.Get(aDoc.Bits());
  if (!delegate) {
    delegate = [[MOXTextMarkerDelegate alloc] initWithDoc:aDoc];
    sDelegates.Put(aDoc.Bits(), delegate);
    [delegate retain];
  }

  return delegate;
}

+ (void)destroyForDoc:(mozilla::a11y::AccessibleOrProxy)aDoc {
  MOZ_ASSERT(!aDoc.IsNull());

  MOXTextMarkerDelegate* delegate = sDelegates.Get(aDoc.Bits());
  if (delegate) {
    sDelegates.Remove(aDoc.Bits());
    [delegate release];
  }
}

- (id)initWithDoc:(AccessibleOrProxy)aDoc {
  MOZ_ASSERT(!aDoc.IsNull(), "Cannot init MOXTextDelegate with null");
  if ((self = [super init])) {
    mGeckoDocAccessible = aDoc;
  }

  return self;
}

- (id)moxStartTextMarker {
  return nil;
}

- (id)moxEndTextMarker {
  return nil;
}

- (NSString*)moxStringForTextMarkerRange:(id)textMarkerRange {
  return nil;
}

- (NSNumber*)moxLengthForTextMarkerRange:(id)textMarkerRange {
  return nil;
}

- (id)moxTextMarkerRangeForUnorderedTextMarkers:(NSArray*)textMarkers {
  return nil;
}

@end
