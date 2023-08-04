/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "GeckoTextMarker.h"

#import "MacUtils.h"

#include "AccAttributes.h"
#include "DocAccessible.h"
#include "DocAccessibleParent.h"
#include "nsCocoaUtils.h"
#include "HyperTextAccessible.h"
#include "States.h"
#include "nsAccUtils.h"

namespace mozilla {
namespace a11y {

struct TextMarkerData {
  TextMarkerData(uintptr_t aDoc, uintptr_t aID, int32_t aOffset)
      : mDoc(aDoc), mID(aID), mOffset(aOffset) {}
  TextMarkerData() {}
  uintptr_t mDoc;
  uintptr_t mID;
  int32_t mOffset;
};

// GeckoTextMarker

GeckoTextMarker::GeckoTextMarker(Accessible* aAcc, int32_t aOffset) {
  HyperTextAccessibleBase* ht = aAcc->AsHyperTextBase();
  if (ht && aOffset != nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT &&
      aOffset <= static_cast<int32_t>(ht->CharacterCount())) {
    mPoint = aAcc->AsHyperTextBase()->ToTextLeafPoint(aOffset);
  } else {
    mPoint = TextLeafPoint(aAcc, aOffset);
  }
}

GeckoTextMarker GeckoTextMarker::MarkerFromAXTextMarker(
    Accessible* aDoc, AXTextMarkerRef aTextMarker) {
  MOZ_ASSERT(aDoc);
  if (!aTextMarker) {
    return GeckoTextMarker();
  }

  if (AXTextMarkerGetLength(aTextMarker) != sizeof(TextMarkerData)) {
    MOZ_ASSERT_UNREACHABLE("Malformed AXTextMarkerRef");
    return GeckoTextMarker();
  }

  TextMarkerData markerData;
  memcpy(&markerData, AXTextMarkerGetBytePtr(aTextMarker),
         sizeof(TextMarkerData));

  if (!utils::DocumentExists(aDoc, markerData.mDoc)) {
    return GeckoTextMarker();
  }

  Accessible* doc = reinterpret_cast<Accessible*>(markerData.mDoc);
  MOZ_ASSERT(doc->IsDoc());
  int32_t offset = markerData.mOffset;
  Accessible* acc = nullptr;
  if (doc->IsRemote()) {
    acc = doc->AsRemote()->AsDoc()->GetAccessible(markerData.mID);
  } else {
    acc = doc->AsLocal()->AsDoc()->GetAccessibleByUniqueID(
        reinterpret_cast<void*>(markerData.mID));
  }

  if (!acc) {
    return GeckoTextMarker();
  }

  return GeckoTextMarker(acc, offset);
}

GeckoTextMarker GeckoTextMarker::MarkerFromIndex(Accessible* aRoot,
                                                 int32_t aIndex) {
  TextLeafRange range(
      TextLeafPoint(aRoot, 0),
      TextLeafPoint(aRoot, nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT));
  int32_t index = aIndex;
  // Iterate through all segments until we exhausted the index sum
  // so we can find the segment the index lives in.
  for (TextLeafRange segment : range) {
    if (segment.End().mAcc->Role() == roles::LISTITEM_MARKER) {
      // XXX: MacOS expects bullets to be in the range's text, but not in
      // the calculated length!
      continue;
    }

    index -= segment.End().mOffset - segment.Start().mOffset;
    if (index <= 0) {
      // The index is in the current segment.
      return GeckoTextMarker(segment.Start().mAcc,
                             segment.End().mOffset + index);
    }
  }

  return GeckoTextMarker();
}

AXTextMarkerRef GeckoTextMarker::CreateAXTextMarker() {
  if (!IsValid()) {
    return nil;
  }

  Accessible* doc = nsAccUtils::DocumentFor(mPoint.mAcc);
  TextMarkerData markerData(reinterpret_cast<uintptr_t>(doc), mPoint.mAcc->ID(),
                            mPoint.mOffset);
  AXTextMarkerRef cf_text_marker = AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&markerData),
      sizeof(TextMarkerData));

  return (__bridge AXTextMarkerRef)[(__bridge id)(cf_text_marker)autorelease];
}

bool GeckoTextMarker::Next() {
  TextLeafPoint next =
      mPoint.FindBoundary(nsIAccessibleText::BOUNDARY_CHAR, eDirNext,
                          TextLeafPoint::BoundaryFlags::eIgnoreListItemMarker);

  if (next && next != mPoint) {
    mPoint = next;
    return true;
  }

  return false;
}

bool GeckoTextMarker::Previous() {
  TextLeafPoint prev =
      mPoint.FindBoundary(nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious,
                          TextLeafPoint::BoundaryFlags::eIgnoreListItemMarker);
  if (prev && mPoint != prev) {
    mPoint = prev;
    return true;
  }

  return false;
}

/**
 * Return true if the given point is inside editable content.
 */
static bool IsPointInEditable(const TextLeafPoint& aPoint) {
  if (aPoint.mAcc) {
    if (aPoint.mAcc->State() & states::EDITABLE) {
      return true;
    }

    Accessible* parent = aPoint.mAcc->Parent();
    if (parent && (parent->State() & states::EDITABLE)) {
      return true;
    }
  }

  return false;
}

GeckoTextMarkerRange GeckoTextMarker::LeftWordRange() const {
  bool includeCurrentInStart = !mPoint.IsParagraphStart(true);
  if (includeCurrentInStart) {
    TextLeafPoint prevChar =
        mPoint.FindBoundary(nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious);
    if (!prevChar.IsSpace()) {
      includeCurrentInStart = false;
    }
  }

  TextLeafPoint start = mPoint.FindBoundary(
      nsIAccessibleText::BOUNDARY_WORD_START, eDirPrevious,
      includeCurrentInStart
          ? (TextLeafPoint::BoundaryFlags::eIncludeOrigin |
             TextLeafPoint::BoundaryFlags::eStopInEditable |
             TextLeafPoint::BoundaryFlags::eIgnoreListItemMarker)
          : (TextLeafPoint::BoundaryFlags::eStopInEditable |
             TextLeafPoint::BoundaryFlags::eIgnoreListItemMarker));

  TextLeafPoint end;
  if (start == mPoint) {
    end = start.FindBoundary(nsIAccessibleText::BOUNDARY_WORD_END, eDirPrevious,
                             TextLeafPoint::BoundaryFlags::eStopInEditable);
  }

  if (start != mPoint || end == start) {
    end = start.FindBoundary(nsIAccessibleText::BOUNDARY_WORD_END, eDirNext,
                             TextLeafPoint::BoundaryFlags::eStopInEditable);
    if (end < mPoint && IsPointInEditable(end) && !IsPointInEditable(mPoint)) {
      start = end;
      end = mPoint;
    }
  }

  return GeckoTextMarkerRange(start < end ? start : end,
                              start < end ? end : start);
}

GeckoTextMarkerRange GeckoTextMarker::RightWordRange() const {
  TextLeafPoint prevChar =
      mPoint.FindBoundary(nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious,
                          TextLeafPoint::BoundaryFlags::eStopInEditable);

  if (prevChar != mPoint && mPoint.IsParagraphStart(true)) {
    return GeckoTextMarkerRange(mPoint, mPoint);
  }

  TextLeafPoint end =
      mPoint.FindBoundary(nsIAccessibleText::BOUNDARY_WORD_END, eDirNext,
                          TextLeafPoint::BoundaryFlags::eStopInEditable);

  if (end == mPoint) {
    // No word to the right of this point.
    return GeckoTextMarkerRange(mPoint, mPoint);
  }

  TextLeafPoint start =
      end.FindBoundary(nsIAccessibleText::BOUNDARY_WORD_START, eDirPrevious,
                       TextLeafPoint::BoundaryFlags::eStopInEditable);

  if (start.FindBoundary(nsIAccessibleText::BOUNDARY_WORD_END, eDirNext,
                         TextLeafPoint::BoundaryFlags::eStopInEditable) <
      mPoint) {
    // Word end is inside of an input to the left of this.
    return GeckoTextMarkerRange(mPoint, mPoint);
  }

  if (mPoint < start) {
    end = start;
    start = mPoint;
  }

  return GeckoTextMarkerRange(start < end ? start : end,
                              start < end ? end : start);
}

GeckoTextMarkerRange GeckoTextMarker::LineRange() const {
  TextLeafPoint start = mPoint.FindBoundary(
      nsIAccessibleText::BOUNDARY_LINE_START, eDirPrevious,
      TextLeafPoint::BoundaryFlags::eStopInEditable |
          TextLeafPoint::BoundaryFlags::eIgnoreListItemMarker |
          TextLeafPoint::BoundaryFlags::eIncludeOrigin);
  // If this is a blank line containing only a line feed, the start boundary
  // is the same as the end boundary. We do not want to walk to the end of the
  // next line.
  TextLeafPoint end =
      start.IsLineFeedChar()
          ? start
          : start.FindBoundary(nsIAccessibleText::BOUNDARY_LINE_END, eDirNext,
                               TextLeafPoint::BoundaryFlags::eStopInEditable);

  return GeckoTextMarkerRange(start, end);
}

GeckoTextMarkerRange GeckoTextMarker::LeftLineRange() const {
  TextLeafPoint start = mPoint.FindBoundary(
      nsIAccessibleText::BOUNDARY_LINE_START, eDirPrevious,
      TextLeafPoint::BoundaryFlags::eStopInEditable |
          TextLeafPoint::BoundaryFlags::eIgnoreListItemMarker);
  TextLeafPoint end =
      start.FindBoundary(nsIAccessibleText::BOUNDARY_LINE_END, eDirNext,
                         TextLeafPoint::BoundaryFlags::eStopInEditable);

  return GeckoTextMarkerRange(start, end);
}

GeckoTextMarkerRange GeckoTextMarker::RightLineRange() const {
  TextLeafPoint end =
      mPoint.FindBoundary(nsIAccessibleText::BOUNDARY_LINE_END, eDirNext,
                          TextLeafPoint::BoundaryFlags::eStopInEditable);
  TextLeafPoint start =
      end.FindBoundary(nsIAccessibleText::BOUNDARY_LINE_START, eDirPrevious,
                       TextLeafPoint::BoundaryFlags::eStopInEditable);

  return GeckoTextMarkerRange(start, end);
}

GeckoTextMarkerRange GeckoTextMarker::ParagraphRange() const {
  // XXX: WebKit gets trapped in inputs. Maybe we shouldn't?
  TextLeafPoint end =
      mPoint.FindBoundary(nsIAccessibleText::BOUNDARY_PARAGRAPH, eDirNext,
                          TextLeafPoint::BoundaryFlags::eStopInEditable);
  TextLeafPoint start =
      end.FindBoundary(nsIAccessibleText::BOUNDARY_PARAGRAPH, eDirPrevious,
                       TextLeafPoint::BoundaryFlags::eStopInEditable);

  return GeckoTextMarkerRange(start, end);
}

GeckoTextMarkerRange GeckoTextMarker::StyleRange() const {
  if (mPoint.mOffset == 0) {
    // If the marker is on the boundary between two leafs, MacOS expects the
    // previous leaf.
    TextLeafPoint prev = mPoint.FindBoundary(
        nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious,
        TextLeafPoint::BoundaryFlags::eIgnoreListItemMarker);
    if (prev != mPoint) {
      return GeckoTextMarker(prev).StyleRange();
    }
  }

  TextLeafPoint start(mPoint.mAcc, 0);
  TextLeafPoint end(mPoint.mAcc, nsAccUtils::TextLength(mPoint.mAcc));
  return GeckoTextMarkerRange(start, end);
}

Accessible* GeckoTextMarker::Leaf() {
  MOZ_ASSERT(mPoint.mAcc);
  Accessible* acc = mPoint.mAcc;
  if (mPoint.mOffset == 0) {
    // If the marker is on the boundary between two leafs, MacOS expects the
    // previous leaf.
    TextLeafPoint prev = mPoint.FindBoundary(
        nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious,
        TextLeafPoint::BoundaryFlags::eIgnoreListItemMarker);
    acc = prev.mAcc;
  }

  Accessible* parent = acc->Parent();
  return parent && nsAccUtils::MustPrune(parent) ? parent : acc;
}

// GeckoTextMarkerRange

GeckoTextMarkerRange::GeckoTextMarkerRange(Accessible* aAccessible) {
  mRange = TextLeafRange(
      TextLeafPoint(aAccessible, 0),
      TextLeafPoint(aAccessible, nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT));
}

GeckoTextMarkerRange GeckoTextMarkerRange::MarkerRangeFromAXTextMarkerRange(
    Accessible* aDoc, AXTextMarkerRangeRef aTextMarkerRange) {
  if (!aTextMarkerRange ||
      CFGetTypeID(aTextMarkerRange) != AXTextMarkerRangeGetTypeID()) {
    return GeckoTextMarkerRange();
  }

  AXTextMarkerRef start_marker(
      AXTextMarkerRangeCopyStartMarker(aTextMarkerRange));
  AXTextMarkerRef end_marker(AXTextMarkerRangeCopyEndMarker(aTextMarkerRange));

  GeckoTextMarker start =
      GeckoTextMarker::MarkerFromAXTextMarker(aDoc, start_marker);
  GeckoTextMarker end =
      GeckoTextMarker::MarkerFromAXTextMarker(aDoc, end_marker);

  CFRelease(start_marker);
  CFRelease(end_marker);

  return GeckoTextMarkerRange(start, end);
}

AXTextMarkerRangeRef GeckoTextMarkerRange::CreateAXTextMarkerRange() {
  if (!IsValid()) {
    return nil;
  }

  GeckoTextMarker start = GeckoTextMarker(mRange.Start());
  GeckoTextMarker end = GeckoTextMarker(mRange.End());

  AXTextMarkerRangeRef cf_text_marker_range =
      AXTextMarkerRangeCreate(kCFAllocatorDefault, start.CreateAXTextMarker(),
                              end.CreateAXTextMarker());

  return (__bridge AXTextMarkerRangeRef)[(__bridge id)(
      cf_text_marker_range)autorelease];
}

NSString* GeckoTextMarkerRange::Text() const {
  if (mRange.Start() == mRange.End()) {
    return @"";
  }

  if ((mRange.Start().mAcc == mRange.End().mAcc) &&
      (mRange.Start().mAcc->ChildCount() == 0) &&
      (mRange.Start().mAcc->State() & states::EDITABLE)) {
    return @"";
  }

  nsAutoString text;
  TextLeafPoint prev = mRange.Start().FindBoundary(
      nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious);
  TextLeafRange range =
      prev != mRange.Start() && prev.mAcc->Role() == roles::LISTITEM_MARKER
          ? TextLeafRange(TextLeafPoint(prev.mAcc, 0), mRange.End())
          : mRange;

  for (TextLeafRange segment : range) {
    TextLeafPoint start = segment.Start();
    if (start.mAcc->IsTextField() && start.mAcc->ChildCount() == 0) {
      continue;
    }

    start.mAcc->AppendTextTo(text, start.mOffset,
                             segment.End().mOffset - start.mOffset);
  }

  return nsCocoaUtils::ToNSString(text);
}

static void AppendTextToAttributedString(
    NSMutableAttributedString* aAttributedString, Accessible* aAccessible,
    const nsString& aString, AccAttributes* aAttributes) {
  NSAttributedString* substr = [[[NSAttributedString alloc]
      initWithString:nsCocoaUtils::ToNSString(aString)
          attributes:utils::StringAttributesFromAccAttributes(
                         aAttributes, aAccessible)] autorelease];

  [aAttributedString appendAttributedString:substr];
}

NSAttributedString* GeckoTextMarkerRange::AttributedText() const {
  NSMutableAttributedString* str =
      [[[NSMutableAttributedString alloc] init] autorelease];

  if (mRange.Start() == mRange.End()) {
    return str;
  }

  if ((mRange.Start().mAcc == mRange.End().mAcc) &&
      (mRange.Start().mAcc->ChildCount() == 0) &&
      (mRange.Start().mAcc->IsTextField())) {
    return str;
  }

  TextLeafPoint prev = mRange.Start().FindBoundary(
      nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious);
  TextLeafRange range =
      prev != mRange.Start() && prev.mAcc->Role() == roles::LISTITEM_MARKER
          ? TextLeafRange(TextLeafPoint(prev.mAcc, 0), mRange.End())
          : mRange;

  nsAutoString text;
  RefPtr<AccAttributes> currentRun = range.Start().GetTextAttributes();
  Accessible* runAcc = range.Start().mAcc;
  for (TextLeafRange segment : range) {
    TextLeafPoint start = segment.Start();
    TextLeafPoint attributesNext;
    do {
      if (start.mAcc->IsText()) {
        attributesNext = start.FindTextAttrsStart(eDirNext, false);
      } else {
        // If this segment isn't a text leaf, but another kind of inline element
        // like a control, just consider this full segment one "attributes run".
        attributesNext = segment.End();
      }
      if (attributesNext == start) {
        // XXX: FindTextAttrsStart should not return the same point.
        break;
      }
      RefPtr<AccAttributes> attributes = start.GetTextAttributes();
      if (!currentRun || !attributes || !attributes->Equal(currentRun)) {
        // If currentRun is null this is a non-text control and we will
        // append a run with no text or attributes, just an AXAttachment
        // referencing this accessible.
        AppendTextToAttributedString(str, runAcc, text, currentRun);
        text.Truncate();
        currentRun = attributes;
        runAcc = start.mAcc;
      }
      TextLeafPoint end =
          attributesNext < segment.End() ? attributesNext : segment.End();
      start.mAcc->AppendTextTo(text, start.mOffset,
                               end.mOffset - start.mOffset);
      start = attributesNext;

    } while (attributesNext < segment.End());
  }

  if (!text.IsEmpty()) {
    AppendTextToAttributedString(str, runAcc, text, currentRun);
  }

  return str;
}

int32_t GeckoTextMarkerRange::Length() const {
  int32_t length = 0;
  for (TextLeafRange segment : mRange) {
    if (segment.End().mAcc->Role() == roles::LISTITEM_MARKER) {
      // XXX: MacOS expects bullets to be in the range's text, but not in
      // the calculated length!
      continue;
    }
    length += segment.End().mOffset - segment.Start().mOffset;
  }

  return length;
}

NSValue* GeckoTextMarkerRange::Bounds() const {
  LayoutDeviceIntRect rect = mRange ? mRange.Bounds() : LayoutDeviceIntRect();

  NSScreen* mainView = [[NSScreen screens] objectAtIndex:0];
  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor(mainView);
  NSRect r =
      NSMakeRect(static_cast<CGFloat>(rect.x) / scaleFactor,
                 [mainView frame].size.height -
                     static_cast<CGFloat>(rect.y + rect.height) / scaleFactor,
                 static_cast<CGFloat>(rect.width) / scaleFactor,
                 static_cast<CGFloat>(rect.height) / scaleFactor);

  return [NSValue valueWithRect:r];
}

void GeckoTextMarkerRange::Select() const { mRange.SetSelection(0); }

}  // namespace a11y
}  // namespace mozilla
