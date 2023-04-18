/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "CachedTextMarker.h"

#import "MacUtils.h"

#include "AccAttributes.h"
#include "nsCocoaUtils.h"
#include "HyperTextAccessible.h"
#include "States.h"
#include "nsAccUtils.h"

namespace mozilla {
namespace a11y {

// CachedTextMarker

CachedTextMarker::CachedTextMarker(Accessible* aAcc, int32_t aOffset) {
  HyperTextAccessibleBase* ht = aAcc->AsHyperTextBase();
  if (ht && aOffset != nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT &&
      aOffset < static_cast<int32_t>(ht->CharacterCount())) {
    mPoint = aAcc->AsHyperTextBase()->ToTextLeafPoint(aOffset);
  } else {
    mPoint = TextLeafPoint(aAcc, aOffset);
  }
}

CachedTextMarker CachedTextMarker::MarkerFromIndex(Accessible* aRoot,
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
      return CachedTextMarker(segment.Start().mAcc,
                              segment.End().mOffset + index);
    }
  }

  return CachedTextMarker();
}

bool CachedTextMarker::Next() {
  TextLeafPoint next =
      mPoint.FindBoundary(nsIAccessibleText::BOUNDARY_CHAR, eDirNext,
                          TextLeafPoint::BoundaryFlags::eIgnoreListItemMarker);

  if (next && next != mPoint) {
    mPoint = next;
    return true;
  }

  return false;
}

bool CachedTextMarker::Previous() {
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

CachedTextMarkerRange CachedTextMarker::LeftWordRange() const {
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

  return CachedTextMarkerRange(start < end ? start : end,
                               start < end ? end : start);
}

CachedTextMarkerRange CachedTextMarker::RightWordRange() const {
  TextLeafPoint prevChar =
      mPoint.FindBoundary(nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious,
                          TextLeafPoint::BoundaryFlags::eStopInEditable);

  if (prevChar != mPoint && mPoint.IsParagraphStart(true)) {
    return CachedTextMarkerRange(mPoint, mPoint);
  }

  TextLeafPoint end =
      mPoint.FindBoundary(nsIAccessibleText::BOUNDARY_WORD_END, eDirNext,
                          TextLeafPoint::BoundaryFlags::eStopInEditable);

  if (end == mPoint) {
    // No word to the right of this point.
    return CachedTextMarkerRange(mPoint, mPoint);
  }

  TextLeafPoint start =
      end.FindBoundary(nsIAccessibleText::BOUNDARY_WORD_START, eDirPrevious,
                       TextLeafPoint::BoundaryFlags::eStopInEditable);

  if (start.FindBoundary(nsIAccessibleText::BOUNDARY_WORD_END, eDirNext,
                         TextLeafPoint::BoundaryFlags::eStopInEditable) <
      mPoint) {
    // Word end is inside of an input to the left of this.
    return CachedTextMarkerRange(mPoint, mPoint);
  }

  if (mPoint < start) {
    end = start;
    start = mPoint;
  }

  return CachedTextMarkerRange(start < end ? start : end,
                               start < end ? end : start);
}

CachedTextMarkerRange CachedTextMarker::LeftLineRange() const {
  TextLeafPoint start = mPoint.FindBoundary(
      nsIAccessibleText::BOUNDARY_LINE_START, eDirPrevious,
      TextLeafPoint::BoundaryFlags::eStopInEditable |
          TextLeafPoint::BoundaryFlags::eIgnoreListItemMarker);
  TextLeafPoint end =
      start.FindBoundary(nsIAccessibleText::BOUNDARY_LINE_END, eDirNext,
                         TextLeafPoint::BoundaryFlags::eStopInEditable);

  return CachedTextMarkerRange(start, end);
}

CachedTextMarkerRange CachedTextMarker::RightLineRange() const {
  TextLeafPoint end =
      mPoint.FindBoundary(nsIAccessibleText::BOUNDARY_LINE_END, eDirNext,
                          TextLeafPoint::BoundaryFlags::eStopInEditable);
  TextLeafPoint start =
      end.FindBoundary(nsIAccessibleText::BOUNDARY_LINE_START, eDirPrevious,
                       TextLeafPoint::BoundaryFlags::eStopInEditable);

  return CachedTextMarkerRange(start, end);
}

CachedTextMarkerRange CachedTextMarker::ParagraphRange() const {
  // XXX: WebKit gets trapped in inputs. Maybe we shouldn't?
  TextLeafPoint end =
      mPoint.FindBoundary(nsIAccessibleText::BOUNDARY_PARAGRAPH, eDirNext,
                          TextLeafPoint::BoundaryFlags::eStopInEditable);
  TextLeafPoint start =
      end.FindBoundary(nsIAccessibleText::BOUNDARY_PARAGRAPH, eDirPrevious,
                       TextLeafPoint::BoundaryFlags::eStopInEditable);

  return CachedTextMarkerRange(start, end);
}

CachedTextMarkerRange CachedTextMarker::StyleRange() const {
  if (mPoint.mOffset == 0) {
    // If the marker is on the boundary between two leafs, MacOS expects the
    // previous leaf.
    TextLeafPoint prev = mPoint.FindBoundary(
        nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious,
        TextLeafPoint::BoundaryFlags::eIgnoreListItemMarker);
    if (prev != mPoint) {
      return CachedTextMarker(prev).StyleRange();
    }
  }

  TextLeafPoint start(mPoint.mAcc, 0);
  TextLeafPoint end(mPoint.mAcc, nsAccUtils::TextLength(mPoint.mAcc));
  return CachedTextMarkerRange(start, end);
}

Accessible* CachedTextMarker::Leaf() {
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

// CachedTextMarkerRange

CachedTextMarkerRange::CachedTextMarkerRange(Accessible* aAccessible) {
  mRange = TextLeafRange(
      TextLeafPoint(aAccessible, 0),
      TextLeafPoint(aAccessible, nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT));
}

NSString* CachedTextMarkerRange::Text() const {
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

NSAttributedString* CachedTextMarkerRange::AttributedText() const {
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
  RefPtr<AccAttributes> currentRun = nullptr;
  Accessible* runAcc = range.Start().mAcc;
  for (TextLeafRange segment : range) {
    TextLeafPoint start = segment.Start();
    if (start.mAcc->IsTextField() && start.mAcc->ChildCount() == 0) {
      continue;
    }
    if (!currentRun) {
      // This is the first segment that isn't an empty input.
      currentRun = start.GetTextAttributes();
    }
    TextLeafPoint attributesNext;
    do {
      attributesNext = start.FindTextAttrsStart(eDirNext, false);
      if (attributesNext == start) {
        // XXX: FindTextAttrsStart should not return the same point.
        break;
      }
      RefPtr<AccAttributes> attributes = start.GetTextAttributes();
      MOZ_ASSERT(attributes);
      if (attributes && !attributes->Equal(currentRun)) {
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

int32_t CachedTextMarkerRange::Length() const {
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

NSValue* CachedTextMarkerRange::Bounds() const {
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

void CachedTextMarkerRange::Select() const { mRange.SetSelection(0); }

bool CachedTextMarkerRange::Crop(Accessible* aContainer) {
  TextLeafPoint containerStart(aContainer, 0);
  TextLeafPoint containerEnd(aContainer,
                             nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT);

  if (mRange.End() < containerStart || containerEnd < mRange.Start()) {
    // The range ends before the container, or starts after it.
    return false;
  }

  if (mRange.Start() < containerStart) {
    // If range start is before container start, adjust range start to
    // start of container.
    mRange.SetStart(containerStart);
  }

  if (containerEnd < mRange.End()) {
    // If range end is after container end, adjust range end to end of
    // container.
    mRange.SetEnd(containerEnd);
  }

  return true;
}
}  // namespace a11y
}  // namespace mozilla
