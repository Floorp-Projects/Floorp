/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#import "MOXAccessibleProtocol.h"

#include "AccessibleOrProxy.h"

@interface MOXTextMarkerDelegate : NSObject <MOXTextMarkerSupport> {
  mozilla::a11y::AccessibleOrProxy mGeckoDocAccessible;
}

+ (id)getOrCreateForDoc:(mozilla::a11y::AccessibleOrProxy)aDoc;

+ (void)destroyForDoc:(mozilla::a11y::AccessibleOrProxy)aDoc;

- (id)initWithDoc:(mozilla::a11y::AccessibleOrProxy)aDoc;

// override
- (id)moxStartTextMarker;

// override
- (id)moxEndTextMarker;

// override
- (NSNumber*)moxLengthForTextMarkerRange:(id)textMarkerRange;

// override
- (NSString*)moxStringForTextMarkerRange:(id)textMarkerRange;

// override
- (id)moxTextMarkerRangeForUnorderedTextMarkers:(NSArray*)textMarkers;

@end
