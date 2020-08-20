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

- (void)dealloc {
  [self invalidateSelection];
  [super dealloc];
}

- (void)setSelectionFrom:(AccessibleOrProxy)startContainer
                      at:(int32_t)startOffset
                      to:(AccessibleOrProxy)endContainer
                      at:(int32_t)endOffset {
  GeckoTextMarkerRange selection(GeckoTextMarker(startContainer, startOffset),
                                 GeckoTextMarker(endContainer, endOffset));

  // We store it as an AXTextMarkerRange because it is a safe
  // way to keep a weak reference - when we need to use the
  // range we can convert it back to a GeckoTextMarkerRange
  // and check that it's valid.
  mSelection = [selection.CreateAXTextMarkerRange() retain];
}

- (void)invalidateSelection {
  [mSelection release];
  mSelection = nil;
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

- (id)moxSelectedTextMarkerRange {
  return mSelection && GeckoTextMarkerRange(mGeckoDocAccessible, mSelection).IsValid() ? mSelection
                                                                                       : nil;
}

- (NSString*)moxStringForTextMarkerRange:(id)textMarkerRange {
  if (mGeckoDocAccessible.IsAccessible()) {
    if (!mGeckoDocAccessible.AsAccessible()->AsDoc()->HasLoadState(
            DocAccessible::eTreeConstructed)) {
      // If the accessible tree is still being constructed the text tree
      // is not in a traversable state yet.
      return @"";
    }
  } else {
    if (mGeckoDocAccessible.AsProxy()->State() & states::STALE) {
      // In the proxy case we don't have access to load state,
      // so we need to use the less granular generic STALE state
      // this state also includes DOM unloaded, which isn't ideal.
      // Since we really only care if the a11y tree is loaded.
      return @"";
    }
  }

  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible, textMarkerRange);
  if (!range.IsValid()) {
    return @"";
  }

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

- (id)moxStartTextMarkerForTextMarkerRange:(id)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible, textMarkerRange);

  return range.IsValid() ? range.mStart.CreateAXTextMarker() : nil;
}

- (id)moxEndTextMarkerForTextMarkerRange:(id)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible, textMarkerRange);

  return range.IsValid() ? range.mEnd.CreateAXTextMarker() : nil;
}

- (id)moxLeftWordTextMarkerRangeForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.LeftWordRange().CreateAXTextMarkerRange();
}

- (id)moxRightWordTextMarkerRangeForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.RightWordRange().CreateAXTextMarkerRange();
}

- (id)moxNextTextMarkerForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  if (!geckoTextMarker.Next()) {
    return nil;
  }

  return geckoTextMarker.CreateAXTextMarker();
}

- (id)moxPreviousTextMarkerForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  if (!geckoTextMarker.Previous()) {
    return nil;
  }

  return geckoTextMarker.CreateAXTextMarker();
}

- (NSAttributedString*)moxAttributedStringForTextMarkerRange:(id)textMarkerRange {
  return [[[NSAttributedString alloc]
      initWithString:[self moxStringForTextMarkerRange:textMarkerRange]] autorelease];
}

- (NSValue*)moxBoundsForTextMarkerRange:(id)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible, textMarkerRange);
  if (!range.IsValid()) {
    return nil;
  }

  return range.Bounds();
}

@end
