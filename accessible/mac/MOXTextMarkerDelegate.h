/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#import "MOXAccessibleProtocol.h"
#import "GeckoTextMarker.h"

@interface MOXTextMarkerDelegate : NSObject <MOXTextMarkerSupport> {
  mozilla::a11y::Accessible* mGeckoDocAccessible;
  AXTextMarkerRangeRef mSelection;
  AXTextMarkerRef mCaret;
  AXTextMarkerRef mPrevCaret;
  int32_t mCaretMoveGranularity;
}

+ (id)getOrCreateForDoc:(mozilla::a11y::Accessible*)aDoc;

+ (void)destroyForDoc:(mozilla::a11y::Accessible*)aDoc;

- (id)initWithDoc:(mozilla::a11y::Accessible*)aDoc;

- (void)dealloc;

- (void)setSelectionFrom:(mozilla::a11y::Accessible*)startContainer
                      at:(int32_t)startOffset
                      to:(mozilla::a11y::Accessible*)endContainer
                      at:(int32_t)endOffset;

- (void)setCaretOffset:(mozilla::a11y::Accessible*)container
                    at:(int32_t)offset
       moveGranularity:(int32_t)granularity;

- (NSDictionary*)selectionChangeInfo;

- (void)invalidateSelection;

- (mozilla::a11y::GeckoTextMarkerRange)selection;

// override
- (AXTextMarkerRef)moxStartTextMarker;

// override
- (AXTextMarkerRef)moxEndTextMarker;

// override
- (AXTextMarkerRangeRef)moxSelectedTextMarkerRange;

// override
- (NSNumber*)moxLengthForTextMarkerRange:(AXTextMarkerRangeRef)textMarkerRange;

// override
- (NSString*)moxStringForTextMarkerRange:(AXTextMarkerRangeRef)textMarkerRange;

// override
- (AXTextMarkerRangeRef)moxTextMarkerRangeForUnorderedTextMarkers:
    (NSArray*)textMarkers;

// override
- (AXTextMarkerRef)moxStartTextMarkerForTextMarkerRange:
    (AXTextMarkerRangeRef)textMarkerRange;

// override
- (AXTextMarkerRef)moxEndTextMarkerForTextMarkerRange:
    (AXTextMarkerRangeRef)textMarkerRange;

// override
- (AXTextMarkerRangeRef)moxLeftWordTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker;

// override
- (AXTextMarkerRangeRef)moxRightWordTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker;

// override
- (AXTextMarkerRangeRef)moxLineTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker;

// override
- (AXTextMarkerRangeRef)moxLeftLineTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker;

// override
- (AXTextMarkerRangeRef)moxRightLineTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker;

// override
- (AXTextMarkerRangeRef)moxParagraphTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker;

// override
- (AXTextMarkerRangeRef)moxStyleTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker;

// override
- (AXTextMarkerRef)moxNextTextMarkerForTextMarker:(AXTextMarkerRef)textMarker;

// override
- (AXTextMarkerRef)moxPreviousTextMarkerForTextMarker:
    (AXTextMarkerRef)textMarker;

// override
- (NSAttributedString*)moxAttributedStringForTextMarkerRange:
    (AXTextMarkerRangeRef)textMarkerRange;

// override
- (NSValue*)moxBoundsForTextMarkerRange:(AXTextMarkerRangeRef)textMarkerRange;

// override
- (id)moxUIElementForTextMarker:(AXTextMarkerRef)textMarker;

// override
- (AXTextMarkerRangeRef)moxTextMarkerRangeForUIElement:(id)element;

// override
- (NSString*)moxMozDebugDescriptionForTextMarker:(AXTextMarkerRef)textMarker;

// override
- (void)moxSetSelectedTextMarkerRange:(AXTextMarkerRangeRef)textMarkerRange;

@end

namespace mozilla {
namespace a11y {

enum AXTextEditType {
  AXTextEditTypeUnknown,
  AXTextEditTypeDelete,
  AXTextEditTypeInsert,
  AXTextEditTypeTyping,
  AXTextEditTypeDictation,
  AXTextEditTypeCut,
  AXTextEditTypePaste,
  AXTextEditTypeAttributesChange
};

enum AXTextStateChangeType {
  AXTextStateChangeTypeUnknown,
  AXTextStateChangeTypeEdit,
  AXTextStateChangeTypeSelectionMove,
  AXTextStateChangeTypeSelectionExtend
};

enum AXTextSelectionDirection {
  AXTextSelectionDirectionUnknown,
  AXTextSelectionDirectionBeginning,
  AXTextSelectionDirectionEnd,
  AXTextSelectionDirectionPrevious,
  AXTextSelectionDirectionNext,
  AXTextSelectionDirectionDiscontiguous
};

enum AXTextSelectionGranularity {
  AXTextSelectionGranularityUnknown,
  AXTextSelectionGranularityCharacter,
  AXTextSelectionGranularityWord,
  AXTextSelectionGranularityLine,
  AXTextSelectionGranularitySentence,
  AXTextSelectionGranularityParagraph,
  AXTextSelectionGranularityPage,
  AXTextSelectionGranularityDocument,
  AXTextSelectionGranularityAll
};
}  // namespace a11y
}  // namespace mozilla
