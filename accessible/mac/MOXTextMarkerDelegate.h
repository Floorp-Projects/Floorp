/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#import "MOXAccessibleProtocol.h"

#include "AccessibleOrProxy.h"

@interface MOXTextMarkerDelegate : NSObject <MOXTextMarkerSupport> {
  mozilla::a11y::AccessibleOrProxy mGeckoDocAccessible;
  id mSelection;
}

+ (id)getOrCreateForDoc:(mozilla::a11y::AccessibleOrProxy)aDoc;

+ (void)destroyForDoc:(mozilla::a11y::AccessibleOrProxy)aDoc;

- (id)initWithDoc:(mozilla::a11y::AccessibleOrProxy)aDoc;

- (void)dealloc;

- (void)setSelectionFrom:(mozilla::a11y::AccessibleOrProxy)startContainer
                      at:(int32_t)startOffset
                      to:(mozilla::a11y::AccessibleOrProxy)endContainer
                      at:(int32_t)endOffset;

- (void)invalidateSelection;

// override
- (id)moxStartTextMarker;

// override
- (id)moxEndTextMarker;

// override
- (id)moxSelectedTextMarkerRange;

// override
- (NSNumber*)moxLengthForTextMarkerRange:(id)textMarkerRange;

// override
- (NSString*)moxStringForTextMarkerRange:(id)textMarkerRange;

// override
- (id)moxTextMarkerRangeForUnorderedTextMarkers:(NSArray*)textMarkers;

// override
- (id)moxStartTextMarkerForTextMarkerRange:(id)textMarkerRange;

// override
- (id)moxEndTextMarkerForTextMarkerRange:(id)textMarkerRange;

// override
- (id)moxLeftWordTextMarkerRangeForTextMarker:(id)textMarker;

// override
- (id)moxRightWordTextMarkerRangeForTextMarker:(id)textMarker;

// override
- (id)moxLineTextMarkerRangeForTextMarker:(id)textMarker;

// override
- (id)moxLeftLineTextMarkerRangeForTextMarker:(id)textMarker;

// override
- (id)moxRightLineTextMarkerRangeForTextMarker:(id)textMarker;

// override
- (id)moxNextTextMarkerForTextMarker:(id)textMarker;

// override
- (id)moxPreviousTextMarkerForTextMarker:(id)textMarker;

// override
- (NSAttributedString*)moxAttributedStringForTextMarkerRange:
    (id)textMarkerRange;

// override
- (NSValue*)moxBoundsForTextMarkerRange:(id)textMarkerRange;

// override
- (id)moxUIElementForTextMarker:(id)textMarker;

// override
- (id)moxTextMarkerRangeForUIElement:(id)element;

// override
- (void)moxSetSelectedTextMarkerRange:(id)textMarkerRange;

@end
