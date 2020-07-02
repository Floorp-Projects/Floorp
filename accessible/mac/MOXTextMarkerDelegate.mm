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
  GeckoTextMarker geckoTextPoint(mGeckoDocAccessible, 0);
  return geckoTextPoint.CreateAXTextMarker();
}

- (id)moxEndTextMarker {
  uint32_t characterCount =
      mGeckoDocAccessible.IsProxy()
          ? mGeckoDocAccessible.AsProxy()->CharacterCount()
          : mGeckoDocAccessible.AsAccessible()->Document()->AsHyperText()->CharacterCount();
  GeckoTextMarker geckoTextPoint(mGeckoDocAccessible, characterCount);
  return geckoTextPoint.CreateAXTextMarker();
}

- (NSString*)moxStringForTextMarkerRange:(id)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible, textMarkerRange);
  return range.Text();
}

- (NSNumber*)moxLengthForTextMarkerRange:(id)textMarkerRange {
  return @([[self moxStringForTextMarkerRange:textMarkerRange] length]);
}

- (id)moxTextMarkerRangeForUnorderedTextMarkers:(NSArray*)textMarkers {
  if ([textMarkers count] != 2) {
    // Don't allow anything but a two member array.
    return nil;
  }

  GeckoTextMarker p1(mGeckoDocAccessible, textMarkers[0]);
  GeckoTextMarker p2(mGeckoDocAccessible, textMarkers[1]);

  bool ordered = p1 < p2;
  GeckoTextMarkerRange range(ordered ? p1 : p2, ordered ? p2 : p1);

  return range.CreateAXTextMarkerRange();
}

@end
