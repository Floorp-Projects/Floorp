/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#include "DocAccessible.h"

#import "MOXTextMarkerDelegate.h"

#include "mozAccessible.h"
#include "mozilla/Preferences.h"
#include "nsISelectionListener.h"

using namespace mozilla::a11y;

#define PREF_ACCESSIBILITY_MAC_DEBUG "accessibility.mac.debug"

static nsTHashMap<nsPtrHashKey<mozilla::a11y::Accessible>,
                  MOXTextMarkerDelegate*>
    sDelegates;

@implementation MOXTextMarkerDelegate

+ (id)getOrCreateForDoc:(mozilla::a11y::Accessible*)aDoc {
  MOZ_ASSERT(aDoc);

  MOXTextMarkerDelegate* delegate = sDelegates.Get(aDoc);
  if (!delegate) {
    delegate = [[MOXTextMarkerDelegate alloc] initWithDoc:aDoc];
    sDelegates.InsertOrUpdate(aDoc, delegate);
    [delegate retain];
  }

  return delegate;
}

+ (void)destroyForDoc:(mozilla::a11y::Accessible*)aDoc {
  MOZ_ASSERT(aDoc);

  MOXTextMarkerDelegate* delegate = sDelegates.Get(aDoc);
  if (delegate) {
    sDelegates.Remove(aDoc);
    [delegate release];
  }
}

- (id)initWithDoc:(Accessible*)aDoc {
  MOZ_ASSERT(aDoc, "Cannot init MOXTextDelegate with null");
  if ((self = [super init])) {
    mGeckoDocAccessible = aDoc;
  }

  mCaretMoveGranularity = nsISelectionListener::NO_AMOUNT;

  return self;
}

- (void)dealloc {
  [self invalidateSelection];
  [super dealloc];
}

- (void)setSelectionFrom:(Accessible*)startContainer
                      at:(int32_t)startOffset
                      to:(Accessible*)endContainer
                      at:(int32_t)endOffset {
  GeckoTextMarkerRange selection(GeckoTextMarker(startContainer, startOffset),
                                 GeckoTextMarker(endContainer, endOffset));

  // We store it as an AXTextMarkerRange because it is a safe
  // way to keep a weak reference - when we need to use the
  // range we can convert it back to a GeckoTextMarkerRange
  // and check that it's valid.
  mSelection = selection.CreateAXTextMarkerRange();
  CFRetain(mSelection);
}

- (void)setCaretOffset:(mozilla::a11y::Accessible*)container
                    at:(int32_t)offset
       moveGranularity:(int32_t)granularity {
  GeckoTextMarker caretMarker(container, offset);

  mPrevCaret = mCaret;
  mCaret = caretMarker.CreateAXTextMarker();
  mCaretMoveGranularity = granularity;

  CFRetain(mCaret);
}

// This returns an info object to pass with AX SelectedTextChanged events.
// It uses the current and previous caret position to make decisions
// regarding which attributes to add to the info object.
- (NSDictionary*)selectionChangeInfo {
  GeckoTextMarkerRange selectedGeckoRange =
      GeckoTextMarkerRange(mGeckoDocAccessible, mSelection);

  int32_t stateChangeType = selectedGeckoRange.mStart == selectedGeckoRange.mEnd
                                ? AXTextStateChangeTypeSelectionMove
                                : AXTextStateChangeTypeSelectionExtend;

  // This is the base info object, includes the selected marker range and
  // the change type depending on the collapsed state of the selection.
  NSMutableDictionary* info = [[@{
    @"AXSelectedTextMarkerRange" : selectedGeckoRange.IsValid()
        ? (__bridge id)mSelection
        : [NSNull null],
    @"AXTextStateChangeType" : @(stateChangeType),
  } mutableCopy] autorelease];

  GeckoTextMarker caretMarker(mGeckoDocAccessible, mCaret);
  GeckoTextMarker prevCaretMarker(mGeckoDocAccessible, mPrevCaret);

  if (!caretMarker.IsValid()) {
    // If the current caret is invalid, stop here and return base info.
    return info;
  }

  mozAccessible* caretEditable =
      [GetNativeFromGeckoAccessible(caretMarker.mContainer)
          moxEditableAncestor];

  if (!caretEditable && stateChangeType == AXTextStateChangeTypeSelectionMove) {
    // If we are not in an editable, VO expects AXTextStateSync to be present
    // and true.
    info[@"AXTextStateSync"] = @YES;
  }

  if (!prevCaretMarker.IsValid() || caretMarker == prevCaretMarker) {
    // If we have no stored previous marker, stop here.
    return info;
  }

  mozAccessible* prevCaretEditable =
      [GetNativeFromGeckoAccessible(prevCaretMarker.mContainer)
          moxEditableAncestor];

  if (prevCaretEditable != caretEditable) {
    // If the caret goes in or out of an editable, consider the
    // move direction "discontiguous".
    info[@"AXTextSelectionDirection"] =
        @(AXTextSelectionDirectionDiscontiguous);
    if ([[caretEditable moxFocused] boolValue]) {
      // If the caret is in a new focused editable, VO expects this attribute to
      // be present and to be true.
      info[@"AXTextSelectionChangedFocus"] = @YES;
    }

    return info;
  }

  bool isForward = prevCaretMarker < caretMarker;
  int direction = isForward ? AXTextSelectionDirectionNext
                            : AXTextSelectionDirectionPrevious;

  int32_t granularity = AXTextSelectionGranularityUnknown;
  switch (mCaretMoveGranularity) {
    case nsISelectionListener::CHARACTER_AMOUNT:
    case nsISelectionListener::CLUSTER_AMOUNT:
      granularity = AXTextSelectionGranularityCharacter;
      break;
    case nsISelectionListener::WORD_AMOUNT:
    case nsISelectionListener::WORDNOSPACE_AMOUNT:
      granularity = AXTextSelectionGranularityWord;
      break;
    case nsISelectionListener::LINE_AMOUNT:
      granularity = AXTextSelectionGranularityLine;
      break;
    case nsISelectionListener::BEGINLINE_AMOUNT:
      direction = AXTextSelectionDirectionBeginning;
      granularity = AXTextSelectionGranularityLine;
      break;
    case nsISelectionListener::ENDLINE_AMOUNT:
      direction = AXTextSelectionDirectionEnd;
      granularity = AXTextSelectionGranularityLine;
      break;
    case nsISelectionListener::PARAGRAPH_AMOUNT:
      granularity = AXTextSelectionGranularityParagraph;
      break;
    default:
      break;
  }

  // Determine selection direction with marker comparison.
  // If the delta between the two markers is more than one, consider it
  // a word. Not accurate, but good enough for VO.
  [info addEntriesFromDictionary:@{
    @"AXTextSelectionDirection" : @(direction),
    @"AXTextSelectionGranularity" : @(granularity)
  }];

  return info;
}

- (void)invalidateSelection {
  CFRelease(mSelection);
  CFRelease(mCaret);
  CFRelease(mPrevCaret);
  mSelection = nil;
}

- (mozilla::a11y::GeckoTextMarkerRange)selection {
  return mozilla::a11y::GeckoTextMarkerRange(mGeckoDocAccessible, mSelection);
}

- (AXTextMarkerRef)moxStartTextMarker {
  GeckoTextMarker geckoTextPoint(mGeckoDocAccessible, 0);
  return geckoTextPoint.CreateAXTextMarker();
}

- (AXTextMarkerRef)moxEndTextMarker {
  uint32_t characterCount =
      mGeckoDocAccessible->IsRemote()
          ? mGeckoDocAccessible->AsRemote()->CharacterCount()
          : mGeckoDocAccessible->AsLocal()
                ->Document()
                ->AsHyperText()
                ->CharacterCount();
  GeckoTextMarker geckoTextPoint(mGeckoDocAccessible, characterCount);
  return geckoTextPoint.CreateAXTextMarker();
}

- (AXTextMarkerRangeRef)moxSelectedTextMarkerRange {
  return mSelection &&
                 GeckoTextMarkerRange(mGeckoDocAccessible, mSelection).IsValid()
             ? mSelection
             : nil;
}

- (NSString*)moxStringForTextMarkerRange:(AXTextMarkerRangeRef)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);
  if (!range.IsValid()) {
    return @"";
  }

  return range.Text();
}

- (NSNumber*)moxLengthForTextMarkerRange:(AXTextMarkerRangeRef)textMarkerRange {
  return @([[self moxStringForTextMarkerRange:textMarkerRange] length]);
}

- (AXTextMarkerRangeRef)moxTextMarkerRangeForUnorderedTextMarkers:
    (NSArray*)textMarkers {
  if ([textMarkers count] != 2) {
    // Don't allow anything but a two member array.
    return nil;
  }

  GeckoTextMarker p1(mGeckoDocAccessible,
                     (__bridge AXTextMarkerRef)textMarkers[0]);
  GeckoTextMarker p2(mGeckoDocAccessible,
                     (__bridge AXTextMarkerRef)textMarkers[1]);

  if (!p1.IsValid() || !p2.IsValid()) {
    // If either marker is invalid, return nil.
    return nil;
  }

  bool ordered = p1 < p2;
  GeckoTextMarkerRange range(ordered ? p1 : p2, ordered ? p2 : p1);

  return range.CreateAXTextMarkerRange();
}

- (AXTextMarkerRef)moxStartTextMarkerForTextMarkerRange:
    (AXTextMarkerRangeRef)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);

  return range.IsValid() ? range.mStart.CreateAXTextMarker() : nil;
}

- (AXTextMarkerRef)moxEndTextMarkerForTextMarkerRange:
    (AXTextMarkerRangeRef)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);

  return range.IsValid() ? range.mEnd.CreateAXTextMarker() : nil;
}

- (AXTextMarkerRangeRef)moxLeftWordTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eLeftWord)
      .CreateAXTextMarkerRange();
}

- (AXTextMarkerRangeRef)moxRightWordTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eRightWord)
      .CreateAXTextMarkerRange();
}

- (AXTextMarkerRangeRef)moxLineTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eLine).CreateAXTextMarkerRange();
}

- (AXTextMarkerRangeRef)moxLeftLineTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eLeftLine)
      .CreateAXTextMarkerRange();
}

- (AXTextMarkerRangeRef)moxRightLineTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eRightLine)
      .CreateAXTextMarkerRange();
}

- (AXTextMarkerRangeRef)moxParagraphTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eParagraph)
      .CreateAXTextMarkerRange();
}

// override
- (AXTextMarkerRangeRef)moxStyleTextMarkerRangeForTextMarker:
    (AXTextMarkerRef)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eStyle).CreateAXTextMarkerRange();
}

- (AXTextMarkerRef)moxNextTextMarkerForTextMarker:(AXTextMarkerRef)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  if (!geckoTextMarker.Next()) {
    return nil;
  }

  return geckoTextMarker.CreateAXTextMarker();
}

- (AXTextMarkerRef)moxPreviousTextMarkerForTextMarker:
    (AXTextMarkerRef)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  if (!geckoTextMarker.Previous()) {
    return nil;
  }

  return geckoTextMarker.CreateAXTextMarker();
}

- (NSAttributedString*)moxAttributedStringForTextMarkerRange:
    (AXTextMarkerRangeRef)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);
  if (!range.IsValid()) {
    return nil;
  }

  return range.AttributedText();
}

- (NSValue*)moxBoundsForTextMarkerRange:(AXTextMarkerRangeRef)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);
  if (!range.IsValid()) {
    return nil;
  }

  return range.Bounds();
}

- (NSNumber*)moxIndexForTextMarker:(AXTextMarkerRef)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  GeckoTextMarkerRange range(GeckoTextMarker(mGeckoDocAccessible, 0),
                             geckoTextMarker);

  return @(range.Length());
}

- (AXTextMarkerRef)moxTextMarkerForIndex:(NSNumber*)index {
  GeckoTextMarker geckoTextMarker = GeckoTextMarker::MarkerFromIndex(
      mGeckoDocAccessible, [index integerValue]);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.CreateAXTextMarker();
}

- (id)moxUIElementForTextMarker:(AXTextMarkerRef)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  Accessible* leaf = geckoTextMarker.Leaf();
  if (!leaf) {
    return nil;
  }

  return GetNativeFromGeckoAccessible(leaf);
}

- (AXTextMarkerRangeRef)moxTextMarkerRangeForUIElement:(id)element {
  if (![element isKindOfClass:[mozAccessible class]]) {
    return nil;
  }

  GeckoTextMarkerRange range((Accessible*)[element geckoAccessible]);
  return range.CreateAXTextMarkerRange();
}

- (NSString*)moxMozDebugDescriptionForTextMarker:(AXTextMarkerRef)textMarker {
  if (!mozilla::Preferences::GetBool(PREF_ACCESSIBILITY_MAC_DEBUG)) {
    return nil;
  }

  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return @"<GeckoTextMarker 0x0 [0]>";
  }

  return [NSString stringWithFormat:@"<GeckoTextMarker %p [%d]>",
                                    geckoTextMarker.mContainer,
                                    geckoTextMarker.mOffset];
}

- (NSString*)moxMozDebugDescriptionForTextMarkerRange:
    (AXTextMarkerRangeRef)textMarkerRange {
  if (!mozilla::Preferences::GetBool(PREF_ACCESSIBILITY_MAC_DEBUG)) {
    return nil;
  }

  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);
  if (!range.IsValid()) {
    return @"<GeckoTextMarkerRange 0x0 [0] - 0x0 [0]>";
  }

  return
      [NSString stringWithFormat:@"<GeckoTextMarkerRange %p [%d] - %p [%d]>",
                                 range.mStart.mContainer, range.mStart.mOffset,
                                 range.mEnd.mContainer, range.mEnd.mOffset];
}

- (void)moxSetSelectedTextMarkerRange:(AXTextMarkerRangeRef)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);
  if (range.IsValid()) {
    range.Select();
  }
}

@end
