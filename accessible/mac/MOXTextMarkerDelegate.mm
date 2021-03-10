/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "mozilla/Preferences.h"

#import "MOXTextMarkerDelegate.h"

using namespace mozilla::a11y;

#define PREF_ACCESSIBILITY_MAC_DEBUG "accessibility.mac.debug"

static nsTHashMap<nsUint64HashKey, MOXTextMarkerDelegate*> sDelegates;

@implementation MOXTextMarkerDelegate

+ (id)getOrCreateForDoc:(mozilla::a11y::AccessibleOrProxy)aDoc {
  MOZ_ASSERT(!aDoc.IsNull());

  MOXTextMarkerDelegate* delegate = sDelegates.Get(aDoc.Bits());
  if (!delegate) {
    delegate = [[MOXTextMarkerDelegate alloc] initWithDoc:aDoc];
    sDelegates.InsertOrUpdate(aDoc.Bits(), delegate);
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

- (void)setCaretOffset:(mozilla::a11y::AccessibleOrProxy)container
                    at:(int32_t)offset {
  GeckoTextMarker caretMarker(container, offset);

  mPrevCaret = mCaret;
  mCaret = [caretMarker.CreateAXTextMarker() retain];
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
  NSMutableDictionary* info = [@{
    @"AXSelectedTextMarkerRange" : selectedGeckoRange.IsValid() ? mSelection
                                                                : [NSNull null],
    @"AXTextStateChangeType" : @(stateChangeType),
  } mutableCopy];

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
  uint32_t deltaLength =
      GeckoTextMarkerRange(isForward ? prevCaretMarker : caretMarker,
                           isForward ? caretMarker : prevCaretMarker)
          .Length();

  // Determine selection direction with marker comparison.
  // If the delta between the two markers is more than one, consider it
  // a word. Not accurate, but good enough for VO.
  [info addEntriesFromDictionary:@{
    @"AXTextSelectionDirection" : isForward
        ? @(AXTextSelectionDirectionNext)
        : @(AXTextSelectionDirectionPrevious),
    @"AXTextSelectionGranularity" : deltaLength == 1
        ? @(AXTextSelectionGranularityCharacter)
        : @(AXTextSelectionGranularityWord)
  }];

  return info;
}

- (void)invalidateSelection {
  [mSelection release];
  [mCaret release];
  [mPrevCaret release];
  mSelection = nil;
}

- (mozilla::a11y::GeckoTextMarkerRange)selection {
  return mozilla::a11y::GeckoTextMarkerRange(mGeckoDocAccessible, mSelection);
}

- (id)moxStartTextMarker {
  GeckoTextMarker geckoTextPoint(mGeckoDocAccessible, 0);
  return geckoTextPoint.CreateAXTextMarker();
}

- (id)moxEndTextMarker {
  uint32_t characterCount =
      mGeckoDocAccessible.IsProxy()
          ? mGeckoDocAccessible.AsProxy()->CharacterCount()
          : mGeckoDocAccessible.AsAccessible()
                ->Document()
                ->AsHyperText()
                ->CharacterCount();
  GeckoTextMarker geckoTextPoint(mGeckoDocAccessible, characterCount);
  return geckoTextPoint.CreateAXTextMarker();
}

- (id)moxSelectedTextMarkerRange {
  return mSelection &&
                 GeckoTextMarkerRange(mGeckoDocAccessible, mSelection).IsValid()
             ? mSelection
             : nil;
}

- (NSString*)moxStringForTextMarkerRange:(id)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);
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

  if (!p1.IsValid() || !p2.IsValid()) {
    // If either marker is invalid, return nil.
    return nil;
  }

  bool ordered = p1 < p2;
  GeckoTextMarkerRange range(ordered ? p1 : p2, ordered ? p2 : p1);

  return range.CreateAXTextMarkerRange();
}

- (id)moxStartTextMarkerForTextMarkerRange:(id)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);

  return range.IsValid() ? range.mStart.CreateAXTextMarker() : nil;
}

- (id)moxEndTextMarkerForTextMarkerRange:(id)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);

  return range.IsValid() ? range.mEnd.CreateAXTextMarker() : nil;
}

- (id)moxLeftWordTextMarkerRangeForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eLeftWord)
      .CreateAXTextMarkerRange();
}

- (id)moxRightWordTextMarkerRangeForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eRightWord)
      .CreateAXTextMarkerRange();
}

- (id)moxLineTextMarkerRangeForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eLine).CreateAXTextMarkerRange();
}

- (id)moxLeftLineTextMarkerRangeForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eLeftLine)
      .CreateAXTextMarkerRange();
}

- (id)moxRightLineTextMarkerRangeForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eRightLine)
      .CreateAXTextMarkerRange();
}

- (id)moxParagraphTextMarkerRangeForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eParagraph)
      .CreateAXTextMarkerRange();
}

// override
- (id)moxStyleTextMarkerRangeForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.Range(EWhichRange::eStyle).CreateAXTextMarkerRange();
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

- (NSAttributedString*)moxAttributedStringForTextMarkerRange:
    (id)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);
  if (!range.IsValid()) {
    return nil;
  }

  return range.AttributedText();
}

- (NSValue*)moxBoundsForTextMarkerRange:(id)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);
  if (!range.IsValid()) {
    return nil;
  }

  return range.Bounds();
}

- (NSNumber*)moxIndexForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  GeckoTextMarkerRange range(GeckoTextMarker(mGeckoDocAccessible, 0),
                             geckoTextMarker);

  return @(range.Length());
}

- (id)moxTextMarkerForIndex:(NSNumber*)index {
  GeckoTextMarker geckoTextMarker = GeckoTextMarker::MarkerFromIndex(
      mGeckoDocAccessible, [index integerValue]);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  return geckoTextMarker.CreateAXTextMarker();
}

- (id)moxUIElementForTextMarker:(id)textMarker {
  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return nil;
  }

  AccessibleOrProxy leaf = geckoTextMarker.Leaf();
  if (leaf.IsNull()) {
    return nil;
  }

  return GetNativeFromGeckoAccessible(leaf);
}

- (id)moxTextMarkerRangeForUIElement:(id)element {
  if (![element isKindOfClass:[mozAccessible class]]) {
    return nil;
  }

  GeckoTextMarkerRange range([element geckoAccessible]);
  return range.CreateAXTextMarkerRange();
}

- (NSString*)moxMozDebugDescriptionForTextMarker:(id)textMarker {
  if (!Preferences::GetBool(PREF_ACCESSIBILITY_MAC_DEBUG)) {
    return nil;
  }

  GeckoTextMarker geckoTextMarker(mGeckoDocAccessible, textMarker);
  if (!geckoTextMarker.IsValid()) {
    return @"<GeckoTextMarker 0x0 [0]>";
  }

  return [NSString stringWithFormat:@"<GeckoTextMarker 0x%lx [%d]>",
                                    geckoTextMarker.mContainer.Bits(),
                                    geckoTextMarker.mOffset];
}

- (NSString*)moxMozDebugDescriptionForTextMarkerRange:(id)textMarkerRange {
  if (!Preferences::GetBool(PREF_ACCESSIBILITY_MAC_DEBUG)) {
    return nil;
  }

  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);
  if (!range.IsValid()) {
    return @"<GeckoTextMarkerRange 0x0 [0] - 0x0 [0]>";
  }

  return [NSString
      stringWithFormat:@"<GeckoTextMarkerRange 0x%lx [%d] - 0x%lx [%d]>",
                       range.mStart.mContainer.Bits(), range.mStart.mOffset,
                       range.mEnd.mContainer.Bits(), range.mEnd.mOffset];
}

- (void)moxSetSelectedTextMarkerRange:(id)textMarkerRange {
  mozilla::a11y::GeckoTextMarkerRange range(mGeckoDocAccessible,
                                            textMarkerRange);
  if (range.IsValid()) {
    range.Select();
  }
}

@end
