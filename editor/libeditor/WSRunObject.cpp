/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WSRunObject.h"

#include "HTMLEditUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/mozalloc.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/RangeUtils.h"
#include "mozilla/SelectionState.h"
#include "mozilla/StaticPrefs_dom.h"     // for StaticPrefs::dom_*
#include "mozilla/StaticPrefs_editor.h"  // for StaticPrefs::editor_*
#include "mozilla/dom/AncestorIterator.h"

#include "nsAString.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsISupportsImpl.h"
#include "nsRange.h"
#include "nsString.h"
#include "nsTextFragment.h"

namespace mozilla {

using namespace dom;

using ChildBlockBoundary = HTMLEditUtils::ChildBlockBoundary;

const char16_t kNBSP = 160;

template WSRunScanner::WSRunScanner(const HTMLEditor* aHTMLEditor,
                                    const EditorDOMPoint& aScanStartPoint,
                                    const EditorDOMPoint& aScanEndPoint);
template WSRunScanner::WSRunScanner(const HTMLEditor* aHTMLEditor,
                                    const EditorRawDOMPoint& aScanStartPoint,
                                    const EditorRawDOMPoint& aScanEndPoint);
template WSRunScanner::WSRunScanner(const HTMLEditor* aHTMLEditor,
                                    const EditorDOMPointInText& aScanStartPoint,
                                    const EditorDOMPointInText& aScanEndPoint);
template WSRunObject::WSRunObject(HTMLEditor& aHTMLEditor,
                                  const EditorDOMPoint& aScanStartPoint,
                                  const EditorDOMPoint& aScanEndPoint);
template WSRunObject::WSRunObject(HTMLEditor& aHTMLEditor,
                                  const EditorRawDOMPoint& aScanStartPoint,
                                  const EditorRawDOMPoint& aScanEndPoint);
template WSRunObject::WSRunObject(HTMLEditor& aHTMLEditor,
                                  const EditorDOMPointInText& aScanStartPoint,
                                  const EditorDOMPointInText& aScanEndPoint);
template WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPoint& aPoint) const;
template WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorRawDOMPoint& aPoint) const;
template WSScanResult WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPoint& aPoint) const;
template WSScanResult WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom(
    const EditorRawDOMPoint& aPoint) const;
template nsresult WSRunObject::NormalizeWhiteSpacesAround(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aScanStartPoint);
template nsresult WSRunObject::NormalizeWhiteSpacesAround(
    HTMLEditor& aHTMLEditor, const EditorRawDOMPoint& aScanStartPoint);
template nsresult WSRunObject::NormalizeWhiteSpacesAround(
    HTMLEditor& aHTMLEditor, const EditorDOMPointInText& aScanStartPoint);

template <typename PT, typename CT>
WSRunScanner::WSRunScanner(const HTMLEditor* aHTMLEditor,
                           const EditorDOMPointBase<PT, CT>& aScanStartPoint,
                           const EditorDOMPointBase<PT, CT>& aScanEndPoint)
    : mScanStartPoint(aScanStartPoint),
      mScanEndPoint(aScanEndPoint),
      mEditingHost(aHTMLEditor->GetActiveEditingHost()),
      mPRE(mScanStartPoint.IsInContentNode() &&
           EditorUtils::IsContentPreformatted(
               *mScanStartPoint.ContainerAsContent())),
      mHTMLEditor(aHTMLEditor) {
  MOZ_ASSERT(
      *nsContentUtils::ComparePoints(aScanStartPoint.ToRawRangeBoundary(),
                                     aScanEndPoint.ToRawRangeBoundary()) <= 0);
  DebugOnly<nsresult> rvIgnored = GetWSNodes();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "WSRunScanner::GetWSNodes() failed, but ignored");
}

template <typename PT, typename CT>
WSRunObject::WSRunObject(HTMLEditor& aHTMLEditor,
                         const EditorDOMPointBase<PT, CT>& aScanStartPoint,
                         const EditorDOMPointBase<PT, CT>& aScanEndPoint)
    : WSRunScanner(&aHTMLEditor, aScanStartPoint, aScanEndPoint),
      mHTMLEditor(aHTMLEditor) {}

// static
nsresult WSRunObject::DeleteInvisibleASCIIWhiteSpaces(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint) {
  MOZ_ASSERT(aPoint.IsSet());

  WSRunObject wsRunObject(aHTMLEditor, aPoint);
  nsresult rv = wsRunObject.DeleteInvisibleASCIIWhiteSpacesInternal();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "WSRunObject::DeleteInvisibleASCIIWhiteSpacesInternal() failed");
  return rv;
}

// static
nsresult WSRunObject::PrepareToJoinBlocks(HTMLEditor& aHTMLEditor,
                                          Element& aLeftBlockElement,
                                          Element& aRightBlockElement) {
  WSRunObject leftWSObj(aHTMLEditor,
                        EditorRawDOMPoint::AtEndOf(aLeftBlockElement));
  WSRunObject rightWSObj(aHTMLEditor,
                         EditorRawDOMPoint(&aRightBlockElement, 0));

  nsresult rv = leftWSObj.PrepareToDeleteRangePriv(&rightWSObj);
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "WSRunObject::PrepareToDeleteRangePriv() failed");
  return rv;
}

nsresult WSRunObject::PrepareToDeleteRange(HTMLEditor& aHTMLEditor,
                                           EditorDOMPoint* aStartPoint,
                                           EditorDOMPoint* aEndPoint) {
  MOZ_ASSERT(aStartPoint);
  MOZ_ASSERT(aEndPoint);

  if (NS_WARN_IF(!aStartPoint->IsSet()) || NS_WARN_IF(!aEndPoint->IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoTrackDOMPoint trackerStart(aHTMLEditor.RangeUpdaterRef(), aStartPoint);
  AutoTrackDOMPoint trackerEnd(aHTMLEditor.RangeUpdaterRef(), aEndPoint);

  WSRunObject leftWSObj(aHTMLEditor, *aStartPoint);
  WSRunObject rightWSObj(aHTMLEditor, *aEndPoint);

  nsresult rv = leftWSObj.PrepareToDeleteRangePriv(&rightWSObj);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "WSRunObject::PrepareToDeleteRangePriv() failed");
  return rv;
}

nsresult WSRunObject::PrepareToDeleteNode(HTMLEditor& aHTMLEditor,
                                          nsIContent* aContent) {
  if (NS_WARN_IF(!aContent)) {
    return NS_ERROR_INVALID_ARG;
  }

  EditorRawDOMPoint atContent(aContent);
  if (!atContent.IsSet()) {
    NS_WARNING("aContent was an orphan node");
    return NS_ERROR_INVALID_ARG;
  }

  WSRunObject leftWSObj(aHTMLEditor, atContent);
  WSRunObject rightWSObj(aHTMLEditor, atContent.NextPoint());

  nsresult rv = leftWSObj.PrepareToDeleteRangePriv(&rightWSObj);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "WSRunObject::PrepareToDeleteRangePriv() failed");
  return rv;
}

nsresult WSRunObject::PrepareToSplitAcrossBlocks(HTMLEditor& aHTMLEditor,
                                                 nsCOMPtr<nsINode>* aSplitNode,
                                                 int32_t* aSplitOffset) {
  if (NS_WARN_IF(!aSplitNode) || NS_WARN_IF(!*aSplitNode) ||
      NS_WARN_IF(!aSplitOffset)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(), aSplitNode,
                            aSplitOffset);

  WSRunObject wsObj(aHTMLEditor, MOZ_KnownLive(*aSplitNode), *aSplitOffset);

  nsresult rv = wsObj.PrepareToSplitAcrossBlocksPriv();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "WSRunObject::PrepareToSplitAcrossBlocksPriv() failed");
  return rv;
}

already_AddRefed<Element> WSRunObject::InsertBreak(
    Selection& aSelection, const EditorDOMPoint& aPointToInsert,
    nsIEditor::EDirection aSelect) {
  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return nullptr;
  }

  // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
  // meanwhile, the pre case is handled in HandleInsertText() in
  // HTMLEditSubActionHandler.cpp

  TextFragmentData textFragmentData(mStart, mEnd, mNBSPData, mPRE);
  const EditorDOMRange invisibleLeadingWhiteSpaceRangeOfNewLine =
      textFragmentData.GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(
          aPointToInsert);
  const EditorDOMRange invisibleTrailingWhiteSpaceRangeOfCurrentLine =
      textFragmentData.GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt(
          aPointToInsert);
  const Maybe<const VisibleWhiteSpacesData> visibleWhiteSpaces =
      !invisibleLeadingWhiteSpaceRangeOfNewLine.IsPositioned() ||
              !invisibleTrailingWhiteSpaceRangeOfCurrentLine.IsPositioned()
          ? textFragmentData.CreateVisibleWhiteSpacesData()
          : Nothing();
  const PointPosition pointPositionWithVisibleWhiteSpaces =
      visibleWhiteSpaces.isSome()
          ? visibleWhiteSpaces.ref().ComparePoint(aPointToInsert)
          : PointPosition::NotInSameDOMTree;

  EditorDOMPoint pointToInsert(aPointToInsert);
  {
    // Some scoping for AutoTrackDOMPoint.  This will track our insertion
    // point while we tweak any surrounding white-space
    AutoTrackDOMPoint tracker(mHTMLEditor.RangeUpdaterRef(), &pointToInsert);

    if (invisibleTrailingWhiteSpaceRangeOfCurrentLine.IsPositioned()) {
      if (!invisibleTrailingWhiteSpaceRangeOfCurrentLine.Collapsed()) {
        // XXX Why don't we remove all of the invisible white-spaces?
        MOZ_ASSERT(invisibleTrailingWhiteSpaceRangeOfCurrentLine.StartRef() ==
                   pointToInsert);
        nsresult rv =
            MOZ_KnownLive(mHTMLEditor)
                .DeleteTextAndTextNodesWithTransaction(
                    invisibleTrailingWhiteSpaceRangeOfCurrentLine.StartRef(),
                    invisibleTrailingWhiteSpaceRangeOfCurrentLine.EndRef());
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return nullptr;
        }
      }
    }
    // If new line will start with visible white-spaces, it needs to be start
    // with an NBSP.
    else if (pointPositionWithVisibleWhiteSpaces ==
                 PointPosition::StartOfFragment ||
             pointPositionWithVisibleWhiteSpaces ==
                 PointPosition::MiddleOfFragment) {
      EditorDOMPointInText atNextCharOfInsertionPoint =
          GetInclusiveNextEditableCharPoint(pointToInsert);
      if (atNextCharOfInsertionPoint.IsSet() &&
          !atNextCharOfInsertionPoint.IsEndOfContainer() &&
          atNextCharOfInsertionPoint.IsCharASCIISpace()) {
        EditorDOMPointInText atPreviousCharOfNextCharOfInsertionPoint =
            GetPreviousEditableCharPoint(atNextCharOfInsertionPoint);
        if (!atPreviousCharOfNextCharOfInsertionPoint.IsSet() ||
            atPreviousCharOfNextCharOfInsertionPoint.IsEndOfContainer() ||
            !atPreviousCharOfNextCharOfInsertionPoint.IsCharASCIISpace()) {
          // We are at start of non-nbsps.  Convert to a single nbsp.
          EditorDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
              GetEndOfCollapsibleASCIIWhiteSpaces(atNextCharOfInsertionPoint);
          nsresult rv = ReplaceASCIIWhiteSpacesWithOneNBSP(
              atNextCharOfInsertionPoint, endOfCollapsibleASCIIWhiteSpaces);
          if (NS_FAILED(rv)) {
            NS_WARNING(
                "WSRunObject::ReplaceASCIIWhiteSpacesWithOneNBSP() failed");
            return nullptr;
          }
        }
      }
    }

    if (invisibleLeadingWhiteSpaceRangeOfNewLine.IsPositioned()) {
      if (!invisibleLeadingWhiteSpaceRangeOfNewLine.Collapsed()) {
        // XXX Why don't we remove all of the invisible white-spaces?
        MOZ_ASSERT(invisibleLeadingWhiteSpaceRangeOfNewLine.EndRef() ==
                   pointToInsert);
        // XXX If the DOM tree has been changed above,
        //     invisibleLeadingWhiteSpaceRangeOfNewLine may be invalid now.
        //     So, we may do something wrong here.
        nsresult rv =
            MOZ_KnownLive(mHTMLEditor)
                .DeleteTextAndTextNodesWithTransaction(
                    invisibleLeadingWhiteSpaceRangeOfNewLine.StartRef(),
                    invisibleLeadingWhiteSpaceRangeOfNewLine.EndRef());
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "WSRunObject::DeleteTextAndTextNodesWithTransaction() failed");
          return nullptr;
        }
      }
    }
    // If the `<br>` element is put immediately after an NBSP, it should be
    // replaced with an ASCII white-space.
    else if (pointPositionWithVisibleWhiteSpaces ==
                 PointPosition::MiddleOfFragment ||
             pointPositionWithVisibleWhiteSpaces ==
                 PointPosition::EndOfFragment) {
      // XXX If the DOM tree has been changed above, pointToInsert` and/or
      //     `visibleWhiteSpaces` may be invalid.  So, we may do
      //     something wrong here.
      nsresult rv = MaybeReplacePreviousNBSPWithASCIIWhiteSpace(
          visibleWhiteSpaces.ref(), pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WSRunObject::MaybeReplacePreviousNBSPWithASCIIWhiteSpace() "
            "failed");
        return nullptr;
      }
    }
  }

  RefPtr<Element> newBRElement =
      MOZ_KnownLive(mHTMLEditor)
          .InsertBRElementWithTransaction(pointToInsert, aSelect);
  NS_WARNING_ASSERTION(newBRElement,
                       "HTMLEditor::InsertBRElementWithTransaction() failed");
  return newBRElement.forget();
}

nsresult WSRunObject::InsertText(Document& aDocument,
                                 const nsAString& aStringToInsert,
                                 EditorRawDOMPoint* aPointAfterInsertedString) {
  // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
  // meanwhile, the pre case is handled in HandleInsertText() in
  // HTMLEditSubActionHandler.cpp

  // MOOSE: for now, just getting the ws logic straight.  This implementation
  // is very slow.  Will need to replace edit rules impl with a more efficient
  // text sink here that does the minimal amount of searching/replacing/copying

  if (aStringToInsert.IsEmpty()) {
    if (aPointAfterInsertedString) {
      *aPointAfterInsertedString = mScanStartPoint;
    }
    return NS_OK;
  }

  TextFragmentData textFragmentDataAtStart(mStart, mEnd, mNBSPData, mPRE);
  const bool insertionPointEqualsOrIsBeforeStartOfText =
      mScanStartPoint.EqualsOrIsBefore(textFragmentDataAtStart.StartRef());
  TextFragmentData textFragmentDataAtEnd(textFragmentDataAtStart);
  if (mScanStartPoint != mScanEndPoint) {
    // If mScanStartPoint doesn't equal to mScanEndPoint, it will replace text
    // (e.g., at committing composition).  We need to handle white-spaces
    // at end of replaced range. So we want to know this white-space type
    // (trailing white-space etc) of this end point, not inserted (start) point,
    // so we need to re-scan white-space type.
    WSRunObject afterRunObject(MOZ_KnownLive(mHTMLEditor), mScanEndPoint);
    textFragmentDataAtEnd =
        TextFragmentData(afterRunObject.mStart, afterRunObject.mEnd,
                         afterRunObject.mNBSPData, afterRunObject.mPRE);
  }
  const bool insertionPointAfterEndOfText =
      textFragmentDataAtEnd.EndRef().EqualsOrIsBefore(mScanEndPoint);

  const EditorDOMRange invisibleLeadingWhiteSpaceRangeAtStart =
      textFragmentDataAtStart
          .GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(mScanStartPoint);
  const EditorDOMRange invisibleTrailingWhiteSpaceRangeAtEnd =
      textFragmentDataAtEnd.GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt(
          mScanEndPoint);
  const Maybe<const VisibleWhiteSpacesData> visibleWhiteSpacesAtStart =
      !invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned()
          ? textFragmentDataAtStart.CreateVisibleWhiteSpacesData()
          : Nothing();
  const PointPosition pointPositionWithVisibleWhiteSpacesAtStart =
      visibleWhiteSpacesAtStart.isSome()
          ? visibleWhiteSpacesAtStart.ref().ComparePoint(mScanStartPoint)
          : PointPosition::NotInSameDOMTree;
  const Maybe<const VisibleWhiteSpacesData> visibleWhiteSpacesAtEnd =
      !invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned()
          ? textFragmentDataAtEnd.CreateVisibleWhiteSpacesData()
          : Nothing();
  const PointPosition pointPositionWithVisibleWhiteSpacesAtEnd =
      visibleWhiteSpacesAtEnd.isSome()
          ? visibleWhiteSpacesAtEnd.ref().ComparePoint(mScanEndPoint)
          : PointPosition::NotInSameDOMTree;

  EditorDOMPoint pointToInsert(mScanStartPoint);
  nsAutoString theString(aStringToInsert);
  {
    // Some scoping for AutoTrackDOMPoint.  This will track our insertion
    // point while we tweak any surrounding white-space
    AutoTrackDOMPoint tracker(mHTMLEditor.RangeUpdaterRef(), &pointToInsert);

    if (invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned()) {
      if (!invisibleTrailingWhiteSpaceRangeAtEnd.Collapsed()) {
        // XXX Why don't we remove all of the invisible white-spaces?
        MOZ_ASSERT(invisibleTrailingWhiteSpaceRangeAtEnd.StartRef() ==
                   pointToInsert);
        nsresult rv = MOZ_KnownLive(mHTMLEditor)
                          .DeleteTextAndTextNodesWithTransaction(
                              invisibleTrailingWhiteSpaceRangeAtEnd.StartRef(),
                              invisibleTrailingWhiteSpaceRangeAtEnd.EndRef());
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return rv;
        }
      }
    }
    // Replace an NBSP at inclusive next character of replacing range to an
    // ASCII white-space if inserting into a visible white-space sequence.
    // XXX With modifying the inserting string later, this creates a line break
    //     opportunity after the inserting string, but this causes
    //     inconsistent result with inserting order.  E.g., type white-space
    //     n times with various order.
    else if (pointPositionWithVisibleWhiteSpacesAtEnd ==
                 PointPosition::StartOfFragment ||
             pointPositionWithVisibleWhiteSpacesAtEnd ==
                 PointPosition::MiddleOfFragment) {
      nsresult rv = MaybeReplaceInclusiveNextNBSPWithASCIIWhiteSpace(
          visibleWhiteSpacesAtEnd.ref(), pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WSRunObject::MaybeReplaceInclusiveNextNBSPWithASCIIWhiteSpace() "
            "failed");
        return rv;
      }
    }

    if (invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned()) {
      if (!invisibleLeadingWhiteSpaceRangeAtStart.Collapsed()) {
        // XXX Why don't we remove all of the invisible white-spaces?
        MOZ_ASSERT(invisibleLeadingWhiteSpaceRangeAtStart.EndRef() ==
                   pointToInsert);
        // XXX If the DOM tree has been changed above,
        //     invisibleLeadingWhiteSpaceRangeAtStart may be invalid now.
        //     So, we may do something wrong here.
        nsresult rv = MOZ_KnownLive(mHTMLEditor)
                          .DeleteTextAndTextNodesWithTransaction(
                              invisibleLeadingWhiteSpaceRangeAtStart.StartRef(),
                              invisibleLeadingWhiteSpaceRangeAtStart.EndRef());
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return rv;
        }
      }
    }
    // Replace an NBSP at previous character of insertion point to an ASCII
    // white-space if inserting into a visible white-space sequence.
    // XXX With modifying the inserting string later, this creates a line break
    //     opportunity before the inserting string, but this causes
    //     inconsistent result with inserting order.  E.g., type white-space
    //     n times with various order.
    else if (pointPositionWithVisibleWhiteSpacesAtStart ==
                 PointPosition::MiddleOfFragment ||
             pointPositionWithVisibleWhiteSpacesAtStart ==
                 PointPosition::EndOfFragment) {
      // XXX If the DOM tree has been changed above, pointToInsert` and/or
      //     `visibleWhiteSpaces` may be invalid.  So, we may do
      //     something wrong here.
      nsresult rv = MaybeReplacePreviousNBSPWithASCIIWhiteSpace(
          visibleWhiteSpacesAtStart.ref(), pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WSRunObject::MaybeReplacePreviousNBSPWithASCIIWhiteSpace() "
            "failed");
        return rv;
      }
    }

    // After this block, pointToInsert is modified by AutoTrackDOMPoint.
  }

  // Next up, tweak head and tail of string as needed.  First the head: there
  // are a variety of circumstances that would require us to convert a leading
  // ws char into an nbsp:

  if (nsCRT::IsAsciiSpace(theString[0])) {
    // If inserting string will follow some invisible leading white-spaces, the
    // string needs to start with an NBSP.
    if (invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned()) {
      theString.SetCharAt(kNBSP, 0);
    }
    // If inserting around visible white-spaces, check whether the previous
    // character of insertion point is an NBSP or an ASCII white-space.
    else if (pointPositionWithVisibleWhiteSpacesAtStart ==
                 PointPosition::MiddleOfFragment ||
             pointPositionWithVisibleWhiteSpacesAtStart ==
                 PointPosition::EndOfFragment) {
      EditorDOMPointInText atPreviousChar =
          GetPreviousEditableCharPoint(pointToInsert);
      if (atPreviousChar.IsSet() && !atPreviousChar.IsEndOfContainer() &&
          atPreviousChar.IsCharASCIISpace()) {
        theString.SetCharAt(kNBSP, 0);
      }
    }
    // If the insertion point is (was) before the start of text and it's
    // immediately after a hard line break, the first ASCII white-space should
    // be replaced with an NBSP for making it visible.
    else if (textFragmentDataAtStart.StartsFromHardLineBreak() &&
             insertionPointEqualsOrIsBeforeStartOfText) {
      theString.SetCharAt(kNBSP, 0);
    }
  }

  // Then the tail
  uint32_t lastCharIndex = theString.Length() - 1;

  if (nsCRT::IsAsciiSpace(theString[lastCharIndex])) {
    // If inserting string will be followed by some invisible trailing
    // white-spaces, the string needs to end with an NBSP.
    if (invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned()) {
      theString.SetCharAt(kNBSP, lastCharIndex);
    }
    // If inserting around visible white-spaces, check whether the inclusive
    // next character of end of replaced range is an NBSP or an ASCII
    // white-space.
    if (pointPositionWithVisibleWhiteSpacesAtStart ==
            PointPosition::StartOfFragment ||
        pointPositionWithVisibleWhiteSpacesAtStart ==
            PointPosition::MiddleOfFragment) {
      EditorDOMPointInText atNextChar =
          GetInclusiveNextEditableCharPoint(pointToInsert);
      if (atNextChar.IsSet() && !atNextChar.IsEndOfContainer() &&
          atNextChar.IsCharASCIISpace()) {
        theString.SetCharAt(kNBSP, lastCharIndex);
      }
    }
    // If the end of replacing range is (was) after the end of text and it's
    // immediately before block boundary, the last ASCII white-space should
    // be replaced with an NBSP for making it visible.
    else if (textFragmentDataAtEnd.EndsByBlockBoundary() &&
             insertionPointAfterEndOfText) {
      theString.SetCharAt(kNBSP, lastCharIndex);
    }
  }

  // Next, scan string for adjacent ws and convert to nbsp/space combos
  // MOOSE: don't need to convert tabs here since that is done by
  // WillInsertText() before we are called.  Eventually, all that logic will be
  // pushed down into here and made more efficient.
  bool prevWS = false;
  for (uint32_t i = 0; i <= lastCharIndex; i++) {
    if (nsCRT::IsAsciiSpace(theString[i])) {
      if (prevWS) {
        // i - 1 can't be negative because prevWS starts out false
        theString.SetCharAt(kNBSP, i - 1);
      } else {
        prevWS = true;
      }
    } else {
      prevWS = false;
    }
  }

  // XXX If the point is not editable, InsertTextWithTransaction() returns
  //     error, but we keep handling it.  But I think that it wastes the
  //     runtime cost.  So, perhaps, we should return error code which couldn't
  //     modify it and make each caller of this method decide whether it should
  //     keep or stop handling the edit action.
  nsresult rv =
      MOZ_KnownLive(mHTMLEditor)
          .InsertTextWithTransaction(aDocument, theString, pointToInsert,
                                     aPointAfterInsertedString);
  if (NS_SUCCEEDED(rv)) {
    return NS_OK;
  }

  NS_WARNING("HTMLEditor::InsertTextWithTransaction() failed, but ignored");

  // XXX Temporarily, set new insertion point to the original point.
  if (aPointAfterInsertedString) {
    *aPointAfterInsertedString = pointToInsert;
  }
  return NS_OK;
}

nsresult WSRunObject::DeleteWSBackward() {
  EditorDOMPointInText atPreviousCharOfStart =
      GetPreviousEditableCharPoint(mScanStartPoint);
  if (!atPreviousCharOfStart.IsSet() ||
      atPreviousCharOfStart.IsEndOfContainer()) {
    return NS_OK;
  }

  // Easy case, preformatted ws.
  if (mPRE) {
    if (!atPreviousCharOfStart.IsCharASCIISpace() &&
        !atPreviousCharOfStart.IsCharNBSP()) {
      return NS_OK;
    }
    nsresult rv =
        MOZ_KnownLive(mHTMLEditor)
            .DeleteTextAndTextNodesWithTransaction(
                atPreviousCharOfStart, atPreviousCharOfStart.NextPoint());
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  // Caller's job to ensure that previous char is really ws.  If it is normal
  // ws, we need to delete the whole run.
  if (atPreviousCharOfStart.IsCharASCIISpace()) {
    EditorDOMPoint startToDelete =
        GetFirstASCIIWhiteSpacePointCollapsedTo(atPreviousCharOfStart);
    EditorDOMPoint endToDelete =
        GetEndOfCollapsibleASCIIWhiteSpaces(atPreviousCharOfStart);
    nsresult rv = WSRunObject::PrepareToDeleteRange(
        MOZ_KnownLive(mHTMLEditor), &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING("WSRunObject::PrepareToDeleteRange() failed");
      return rv;
    }

    // finally, delete that ws
    rv = MOZ_KnownLive(mHTMLEditor)
             .DeleteTextAndTextNodesWithTransaction(startToDelete, endToDelete);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  if (atPreviousCharOfStart.IsCharNBSP()) {
    EditorDOMPoint startToDelete(atPreviousCharOfStart);
    EditorDOMPoint endToDelete(startToDelete.NextPoint());
    nsresult rv = WSRunObject::PrepareToDeleteRange(
        MOZ_KnownLive(mHTMLEditor), &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING("WSRunObject::PrepareToDeleteRange() failed");
      return rv;
    }

    // finally, delete that ws
    rv = MOZ_KnownLive(mHTMLEditor)
             .DeleteTextAndTextNodesWithTransaction(startToDelete, endToDelete);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  return NS_OK;
}

nsresult WSRunObject::DeleteWSForward() {
  EditorDOMPointInText atNextCharOfStart =
      GetInclusiveNextEditableCharPoint(mScanStartPoint);
  if (!atNextCharOfStart.IsSet() || atNextCharOfStart.IsEndOfContainer()) {
    return NS_OK;
  }

  // Easy case, preformatted ws.
  if (mPRE) {
    if (!atNextCharOfStart.IsCharASCIISpace() &&
        !atNextCharOfStart.IsCharNBSP()) {
      return NS_OK;
    }
    nsresult rv = MOZ_KnownLive(mHTMLEditor)
                      .DeleteTextAndTextNodesWithTransaction(
                          atNextCharOfStart, atNextCharOfStart.NextPoint());
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  // Caller's job to ensure that next char is really ws.  If it is normal ws,
  // we need to delete the whole run.
  if (atNextCharOfStart.IsCharASCIISpace()) {
    EditorDOMPoint startToDelete =
        GetFirstASCIIWhiteSpacePointCollapsedTo(atNextCharOfStart);
    EditorDOMPoint endToDelete =
        GetEndOfCollapsibleASCIIWhiteSpaces(atNextCharOfStart);
    nsresult rv = WSRunObject::PrepareToDeleteRange(
        MOZ_KnownLive(mHTMLEditor), &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING("WSRunObject::PrepareToDeleteRange() failed");
      return rv;
    }

    // Finally, delete that ws
    rv = MOZ_KnownLive(mHTMLEditor)
             .DeleteTextAndTextNodesWithTransaction(startToDelete, endToDelete);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  if (atNextCharOfStart.IsCharNBSP()) {
    EditorDOMPoint startToDelete(atNextCharOfStart);
    EditorDOMPoint endToDelete(startToDelete.NextPoint());
    nsresult rv = WSRunObject::PrepareToDeleteRange(
        MOZ_KnownLive(mHTMLEditor), &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING("WSRunObject::PrepareToDeleteRange() failed");
      return rv;
    }

    // Finally, delete that ws
    rv = MOZ_KnownLive(mHTMLEditor)
             .DeleteTextAndTextNodesWithTransaction(startToDelete, endToDelete);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  return NS_OK;
}

template <typename PT, typename CT>
WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSet());

  // If the range has visible text and start of the visible text is before
  // aPoint, return previous character in the text.
  Maybe<VisibleWhiteSpacesData> visibleWhiteSpaces =
      TextFragmentData(mStart, mEnd, mNBSPData, mPRE)
          .CreateVisibleWhiteSpacesData();
  if (visibleWhiteSpaces.isSome() &&
      visibleWhiteSpaces.ref().RawStartPoint().IsBefore(aPoint)) {
    EditorDOMPointInText atPreviousChar = GetPreviousEditableCharPoint(aPoint);
    // When it's a non-empty text node, return it.
    if (atPreviousChar.IsSet() && !atPreviousChar.IsContainerEmpty()) {
      MOZ_ASSERT(!atPreviousChar.IsEndOfContainer());
      return WSScanResult(atPreviousChar.NextPoint(),
                          atPreviousChar.IsCharASCIISpaceOrNBSP()
                              ? WSType::NormalWhiteSpaces
                              : WSType::NormalText);
    }
  }

  // Otherwise, return the start of the range.
  if (mStart.GetReasonContent() != mStart.PointRef().GetContainer()) {
    // In this case, mStart.PointRef().Offset() is not meaningful.
    return WSScanResult(mStart.GetReasonContent(), mStart.RawReason());
  }
  return WSScanResult(mStart.PointRef(), mStart.RawReason());
}

template <typename PT, typename CT>
WSScanResult WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSet());

  // If the range has visible text and aPoint equals or is before the end of the
  // visible text, return inclusive next character in the text.
  Maybe<VisibleWhiteSpacesData> visibleWhiteSpaces =
      TextFragmentData(mStart, mEnd, mNBSPData, mPRE)
          .CreateVisibleWhiteSpacesData();
  if (visibleWhiteSpaces.isSome() &&
      aPoint.EqualsOrIsBefore(visibleWhiteSpaces.ref().RawEndPoint())) {
    EditorDOMPointInText atNextChar = GetInclusiveNextEditableCharPoint(aPoint);
    // When it's a non-empty text node, return it.
    if (atNextChar.IsSet() && !atNextChar.IsContainerEmpty()) {
      return WSScanResult(
          atNextChar,
          !atNextChar.IsEndOfContainer() && atNextChar.IsCharASCIISpaceOrNBSP()
              ? WSType::NormalWhiteSpaces
              : WSType::NormalText);
    }
  }

  // Otherwise, return the end of the range.
  if (mEnd.GetReasonContent() != mEnd.PointRef().GetContainer()) {
    // In this case, mEnd.PointRef().Offset() is not meaningful.
    return WSScanResult(mEnd.GetReasonContent(), mEnd.RawReason());
  }
  return WSScanResult(mEnd.PointRef(), mEnd.RawReason());
}

// static
template <typename EditorDOMPointType>
nsresult WSRunObject::NormalizeWhiteSpacesAround(
    HTMLEditor& aHTMLEditor, const EditorDOMPointType& aScanStartPoint) {
  WSRunObject wsRunObject(aHTMLEditor, aScanStartPoint);
  // this routine examines a run of ws and tries to get rid of some unneeded
  // nbsp's, replacing them with regular ascii space if possible.  Keeping
  // things simple for now and just trying to fix up the trailing ws in the run.
  if (!wsRunObject.mNBSPData.FoundNBSP()) {
    // nothing to do!
    return NS_OK;
  }
  Maybe<VisibleWhiteSpacesData> visibleWhiteSpaces =
      TextFragmentData(wsRunObject.mStart, wsRunObject.mEnd,
                       wsRunObject.mNBSPData, wsRunObject.mPRE)
          .CreateVisibleWhiteSpacesData();
  if (visibleWhiteSpaces.isNothing()) {
    return NS_OK;
  }

  nsresult rv =
      wsRunObject.NormalizeWhiteSpacesAtEndOf(visibleWhiteSpaces.ref());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "WSRunObject::NormalizeWhiteSpacesAtEndOf() failed");
  return rv;
}

//--------------------------------------------------------------------------------------------
//   protected methods
//--------------------------------------------------------------------------------------------

nsIContent* WSRunScanner::GetEditableBlockParentOrTopmotEditableInlineContent(
    nsIContent* aContent) const {
  if (NS_WARN_IF(!aContent)) {
    return nullptr;
  }
  NS_ASSERTION(EditorUtils::IsEditableContent(*aContent, EditorType::HTML),
               "Given content is not editable");
  // XXX What should we do if scan range crosses block boundary?  Currently,
  //     it's not collapsed only when inserting composition string so that
  //     it's possible but shouldn't occur actually.
  nsIContent* editableBlockParentOrTopmotEditableInlineContent = nullptr;
  for (nsIContent* content : aContent->InclusiveAncestorsOfType<nsIContent>()) {
    if (!EditorUtils::IsEditableContent(*content, EditorType::HTML)) {
      break;
    }
    editableBlockParentOrTopmotEditableInlineContent = content;
    if (HTMLEditUtils::IsBlockElement(
            *editableBlockParentOrTopmotEditableInlineContent)) {
      break;
    }
  }
  return editableBlockParentOrTopmotEditableInlineContent;
}

nsresult WSRunScanner::GetWSNodes() {
  // collect up an array of nodes that are contiguous with the insertion point
  // and which contain only white-space.  Stop if you reach non-ws text or a new
  // block boundary.
  EditorDOMPoint start(mScanStartPoint), end(mScanStartPoint);
  nsIContent* scanStartContent = mScanStartPoint.GetContainerAsContent();
  if (NS_WARN_IF(!scanStartContent)) {
    // Meaning container of mScanStartPoint is a Document or DocumentFragment.
    // I.e., we're try to modify outside of root element.  We don't need to
    // support such odd case because web apps cannot append text nodes as
    // direct child of Document node.
    return NS_ERROR_FAILURE;
  }
  nsIContent* editableBlockParentOrTopmotEditableInlineContent =
      GetEditableBlockParentOrTopmotEditableInlineContent(scanStartContent);
  if (NS_WARN_IF(!editableBlockParentOrTopmotEditableInlineContent)) {
    // Meaning that the container of `mScanStartPoint` is not editable.
    editableBlockParentOrTopmotEditableInlineContent = scanStartContent;
  }

  mStart = BoundaryData::ScanWhiteSpaceStartFrom(
      mScanStartPoint, *editableBlockParentOrTopmotEditableInlineContent,
      mEditingHost, &mNBSPData);
  mEnd = BoundaryData::ScanWhiteSpaceEndFrom(
      mScanStartPoint, *editableBlockParentOrTopmotEditableInlineContent,
      mEditingHost, &mNBSPData);
  return NS_OK;
}

// static
template <typename EditorDOMPointType>
Maybe<WSRunScanner::BoundaryData>
WSRunScanner::BoundaryData::ScanWhiteSpaceStartInTextNode(
    const EditorDOMPointType& aPoint, NoBreakingSpaceData* aNBSPData) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_DIAGNOSTIC_ASSERT(aPoint.IsInTextNode());

  const nsTextFragment& textFragment = aPoint.ContainerAsText()->TextFragment();
  for (uint32_t i = std::min(aPoint.Offset(), textFragment.GetLength()); i;
       i--) {
    char16_t ch = textFragment.CharAt(i - 1);
    if (nsCRT::IsAsciiSpace(ch)) {
      continue;
    }

    if (ch == HTMLEditUtils::kNBSP) {
      if (aNBSPData) {
        aNBSPData->NotifyNBSP(
            EditorDOMPointInText(aPoint.ContainerAsText(), i - 1),
            NoBreakingSpaceData::Scanning::Backward);
      }
      continue;
    }

    return Some(BoundaryData(EditorDOMPoint(aPoint.ContainerAsText(), i),
                             *aPoint.ContainerAsText(), WSType::NormalText));
  }

  return Nothing();
}

// static
template <typename EditorDOMPointType>
WSRunScanner::BoundaryData WSRunScanner::BoundaryData::ScanWhiteSpaceStartFrom(
    const EditorDOMPointType& aPoint,
    const nsIContent& aEditableBlockParentOrTopmostEditableInlineContent,
    const Element* aEditingHost, NoBreakingSpaceData* aNBSPData) {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  // first look backwards to find preceding ws nodes
  if (aPoint.IsInTextNode() && !aPoint.IsStartOfContainer()) {
    Maybe<BoundaryData> startInTextNode =
        BoundaryData::ScanWhiteSpaceStartInTextNode(aPoint, aNBSPData);
    if (startInTextNode.isSome()) {
      return startInTextNode.ref();
    }
    // The text node does not have visible character, let's keep scanning
    // preceding nodes.
    return BoundaryData::ScanWhiteSpaceStartFrom(
        EditorDOMPoint(aPoint.ContainerAsText(), 0),
        aEditableBlockParentOrTopmostEditableInlineContent, aEditingHost,
        aNBSPData);
  }

  // we haven't found the start of ws yet.  Keep looking
  nsIContent* previousLeafContentOrBlock =
      HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
          aPoint, aEditableBlockParentOrTopmostEditableInlineContent,
          aEditingHost);
  if (!previousLeafContentOrBlock) {
    // no prior node means we exhausted
    // aEditableBlockParentOrTopmostEditableInlineContent
    // mReasonContent can be either a block element or any non-editable
    // content in this case.
    return BoundaryData(aPoint,
                        const_cast<nsIContent&>(
                            aEditableBlockParentOrTopmostEditableInlineContent),
                        WSType::CurrentBlockBoundary);
  }

  if (HTMLEditUtils::IsBlockElement(*previousLeafContentOrBlock)) {
    return BoundaryData(aPoint, *previousLeafContentOrBlock,
                        WSType::OtherBlockBoundary);
  }

  if (!previousLeafContentOrBlock->IsText() ||
      !previousLeafContentOrBlock->IsEditable()) {
    // it's a break or a special node, like <img>, that is not a block and
    // not a break but still serves as a terminator to ws runs.
    return BoundaryData(aPoint, *previousLeafContentOrBlock,
                        previousLeafContentOrBlock->IsHTMLElement(nsGkAtoms::br)
                            ? WSType::BRElement
                            : WSType::SpecialContent);
  }

  if (!previousLeafContentOrBlock->AsText()->TextFragment().GetLength()) {
    // Zero length text node. Set start point to it
    // so we can get past it!
    return BoundaryData::ScanWhiteSpaceStartFrom(
        EditorDOMPointInText(previousLeafContentOrBlock->AsText(), 0),
        aEditableBlockParentOrTopmostEditableInlineContent, aEditingHost,
        aNBSPData);
  }

  Maybe<BoundaryData> startInTextNode =
      BoundaryData::ScanWhiteSpaceStartInTextNode(
          EditorDOMPointInText::AtEndOf(*previousLeafContentOrBlock->AsText()),
          aNBSPData);
  if (startInTextNode.isSome()) {
    return startInTextNode.ref();
  }

  // The text node does not have visible character, let's keep scanning
  // preceding nodes.
  return BoundaryData::ScanWhiteSpaceStartFrom(
      EditorDOMPointInText(previousLeafContentOrBlock->AsText(), 0),
      aEditableBlockParentOrTopmostEditableInlineContent, aEditingHost,
      aNBSPData);
}

// static
template <typename EditorDOMPointType>
Maybe<WSRunScanner::BoundaryData>
WSRunScanner::BoundaryData::ScanWhiteSpaceEndInTextNode(
    const EditorDOMPointType& aPoint, NoBreakingSpaceData* aNBSPData) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_DIAGNOSTIC_ASSERT(aPoint.IsInTextNode());

  const nsTextFragment& textFragment = aPoint.ContainerAsText()->TextFragment();
  for (uint32_t i = aPoint.Offset(); i < textFragment.GetLength(); i++) {
    char16_t ch = textFragment.CharAt(i);
    if (nsCRT::IsAsciiSpace(ch)) {
      continue;
    }

    if (ch == HTMLEditUtils::kNBSP) {
      if (aNBSPData) {
        aNBSPData->NotifyNBSP(EditorDOMPointInText(aPoint.ContainerAsText(), i),
                              NoBreakingSpaceData::Scanning::Forward);
      }
      continue;
    }

    return Some(BoundaryData(EditorDOMPoint(aPoint.ContainerAsText(), i),
                             *aPoint.ContainerAsText(), WSType::NormalText));
  }

  return Nothing();
}

// static
template <typename EditorDOMPointType>
WSRunScanner::BoundaryData WSRunScanner::BoundaryData::ScanWhiteSpaceEndFrom(
    const EditorDOMPointType& aPoint,
    const nsIContent& aEditableBlockParentOrTopmostEditableInlineContent,
    const Element* aEditingHost, NoBreakingSpaceData* aNBSPData) {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (aPoint.IsInTextNode() && !aPoint.IsEndOfContainer()) {
    Maybe<BoundaryData> endInTextNode =
        BoundaryData::ScanWhiteSpaceEndInTextNode(aPoint, aNBSPData);
    if (endInTextNode.isSome()) {
      return endInTextNode.ref();
    }
    // The text node does not have visible character, let's keep scanning
    // following nodes.
    return BoundaryData::ScanWhiteSpaceEndFrom(
        EditorDOMPointInText::AtEndOf(*aPoint.ContainerAsText()),
        aEditableBlockParentOrTopmostEditableInlineContent, aEditingHost,
        aNBSPData);
  }

  // we haven't found the end of ws yet.  Keep looking
  nsIContent* nextLeafContentOrBlock =
      HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          aPoint, aEditableBlockParentOrTopmostEditableInlineContent,
          aEditingHost);
  if (!nextLeafContentOrBlock) {
    // no next node means we exhausted
    // aEditableBlockParentOrTopmostEditableInlineContent
    // mReasonContent can be either a block element or any non-editable
    // content in this case.
    return BoundaryData(aPoint,
                        const_cast<nsIContent&>(
                            aEditableBlockParentOrTopmostEditableInlineContent),
                        WSType::CurrentBlockBoundary);
  }

  if (HTMLEditUtils::IsBlockElement(*nextLeafContentOrBlock)) {
    // we encountered a new block.  therefore no more ws.
    return BoundaryData(aPoint, *nextLeafContentOrBlock,
                        WSType::OtherBlockBoundary);
  }

  if (!nextLeafContentOrBlock->IsText() ||
      !nextLeafContentOrBlock->IsEditable()) {
    // we encountered a break or a special node, like <img>,
    // that is not a block and not a break but still
    // serves as a terminator to ws runs.
    return BoundaryData(aPoint, *nextLeafContentOrBlock,
                        nextLeafContentOrBlock->IsHTMLElement(nsGkAtoms::br)
                            ? WSType::BRElement
                            : WSType::SpecialContent);
  }

  if (!nextLeafContentOrBlock->AsText()->TextFragment().GetLength()) {
    // Zero length text node. Set end point to it
    // so we can get past it!
    return BoundaryData::ScanWhiteSpaceEndFrom(
        EditorDOMPointInText(nextLeafContentOrBlock->AsText(), 0),
        aEditableBlockParentOrTopmostEditableInlineContent, aEditingHost,
        aNBSPData);
  }

  Maybe<BoundaryData> endInTextNode = BoundaryData::ScanWhiteSpaceEndInTextNode(
      EditorDOMPointInText(nextLeafContentOrBlock->AsText(), 0), aNBSPData);
  if (endInTextNode.isSome()) {
    return endInTextNode.ref();
  }

  // The text node does not have visible character, let's keep scanning
  // following nodes.
  return BoundaryData::ScanWhiteSpaceEndFrom(
      EditorDOMPointInText::AtEndOf(*nextLeafContentOrBlock->AsText()),
      aEditableBlockParentOrTopmostEditableInlineContent, aEditingHost,
      aNBSPData);
}

EditorDOMRange
WSRunScanner::TextFragmentData::GetInvisibleLeadingWhiteSpaceRange() const {
  if (mLeadingWhiteSpaceRange.isSome()) {
    return mLeadingWhiteSpaceRange.ref();
  }

  // If it's preformatted or not start of line, the range is not invisible
  // leading white-spaces.
  // TODO: We should check each text node style rather than WSRunScanner's
  //       scan start position's style.
  if (mIsPreformatted || !StartsFromHardLineBreak()) {
    mLeadingWhiteSpaceRange.emplace();
    return mLeadingWhiteSpaceRange.ref();
  }

  // If there is no NBSP, all of the given range is leading white-spaces.
  // Note that this result may be collapsed if there is no leading white-spaces.
  if (!mNBSPData.FoundNBSP()) {
    MOZ_ASSERT(mStart.PointRef().IsSet() || mEnd.PointRef().IsSet());
    mLeadingWhiteSpaceRange.emplace(mStart.PointRef(), mEnd.PointRef());
    return mLeadingWhiteSpaceRange.ref();
  }

  MOZ_ASSERT(mNBSPData.LastPointRef().IsSetAndValid());

  // Even if the first NBSP is the start, i.e., there is no invisible leading
  // white-space, return collapsed range.
  mLeadingWhiteSpaceRange.emplace(mStart.PointRef(), mNBSPData.FirstPointRef());
  return mLeadingWhiteSpaceRange.ref();
}

EditorDOMRange
WSRunScanner::TextFragmentData::GetInvisibleTrailingWhiteSpaceRange() const {
  if (mTrailingWhiteSpaceRange.isSome()) {
    return mTrailingWhiteSpaceRange.ref();
  }

  // If it's preformatted or not immediately before block boundary, the range is
  // not invisible trailing white-spaces.  Note that collapsible white-spaces
  // before a `<br>` element is visible.
  // TODO: We should check each text node style rather than WSRunScanner's
  //       scan start position's style.
  if (mIsPreformatted || !EndsByBlockBoundary()) {
    mTrailingWhiteSpaceRange.emplace();
    return mTrailingWhiteSpaceRange.ref();
  }

  // If there is no NBSP, all of the given range is trailing white-spaces.
  // Note that this result may be collapsed if there is no trailing white-
  // spaces.
  if (!mNBSPData.FoundNBSP()) {
    MOZ_ASSERT(mStart.PointRef().IsSet() || mEnd.PointRef().IsSet());
    mTrailingWhiteSpaceRange.emplace(mStart.PointRef(), mEnd.PointRef());
    return mTrailingWhiteSpaceRange.ref();
  }

  MOZ_ASSERT(mNBSPData.LastPointRef().IsSetAndValid());

  // If last NBSP is immediately before the end, there is no trailing white-
  // spaces.
  if (mEnd.PointRef().IsSet() &&
      mNBSPData.LastPointRef().GetContainer() ==
          mEnd.PointRef().GetContainer() &&
      mNBSPData.LastPointRef().Offset() == mEnd.PointRef().Offset() - 1) {
    mTrailingWhiteSpaceRange.emplace();
    return mTrailingWhiteSpaceRange.ref();
  }

  // Otherwise, the may be some trailing white-spaces.
  MOZ_ASSERT(!mNBSPData.LastPointRef().IsEndOfContainer());
  mTrailingWhiteSpaceRange.emplace(mNBSPData.LastPointRef().NextPoint(),
                                   mEnd.PointRef());
  return mTrailingWhiteSpaceRange.ref();
}

Maybe<WSRunScanner::VisibleWhiteSpacesData>
WSRunScanner::TextFragmentData::CreateVisibleWhiteSpacesData() const {
  VisibleWhiteSpacesData visibleWhiteSpaces;
  if (IsPreformattedOrSurrondedByVisibleContent()) {
    if (mStart.PointRef().IsSet()) {
      visibleWhiteSpaces.mStartNode = mStart.PointRef().GetContainer();
      visibleWhiteSpaces.mStartOffset = mStart.PointRef().Offset();
    }
    visibleWhiteSpaces.SetStartFrom(mStart.RawReason());
    if (mEnd.PointRef().IsSet()) {
      visibleWhiteSpaces.mEndNode = mEnd.PointRef().GetContainer();
      visibleWhiteSpaces.mEndOffset = mEnd.PointRef().Offset();
    }
    visibleWhiteSpaces.SetEndBy(mEnd.RawReason());
    return Some(visibleWhiteSpaces);
  }

  // If all of the range is invisible leading or trailing white-spaces,
  // there is no visible content.
  const EditorDOMRange leadingWhiteSpaceRange =
      GetInvisibleLeadingWhiteSpaceRange();
  const bool maybeHaveLeadingWhiteSpaces =
      leadingWhiteSpaceRange.StartRef().IsSet() ||
      leadingWhiteSpaceRange.EndRef().IsSet();
  if (maybeHaveLeadingWhiteSpaces &&
      leadingWhiteSpaceRange.StartRef() == mStart.PointRef() &&
      leadingWhiteSpaceRange.EndRef() == mEnd.PointRef()) {
    return Nothing();
  }
  const EditorDOMRange trailingWhiteSpaceRange =
      GetInvisibleTrailingWhiteSpaceRange();
  const bool maybeHaveTrailingWhiteSpaces =
      trailingWhiteSpaceRange.StartRef().IsSet() ||
      trailingWhiteSpaceRange.EndRef().IsSet();
  if (maybeHaveTrailingWhiteSpaces &&
      trailingWhiteSpaceRange.StartRef() == mStart.PointRef() &&
      trailingWhiteSpaceRange.EndRef() == mEnd.PointRef()) {
    return Nothing();
  }

  if (!StartsFromHardLineBreak()) {
    if (mStart.PointRef().IsSet()) {
      visibleWhiteSpaces.mStartNode = mStart.PointRef().GetContainer();
      visibleWhiteSpaces.mStartOffset = mStart.PointRef().Offset();
    }
    visibleWhiteSpaces.SetStartFrom(mStart.RawReason());
    if (!maybeHaveTrailingWhiteSpaces) {
      visibleWhiteSpaces.mEndNode = mEnd.PointRef().GetContainer();
      visibleWhiteSpaces.mEndOffset = mEnd.PointRef().Offset();
      visibleWhiteSpaces.SetEndBy(mEnd.RawReason());
      return Some(visibleWhiteSpaces);
    }
    if (trailingWhiteSpaceRange.StartRef().IsSet()) {
      visibleWhiteSpaces.mEndNode =
          trailingWhiteSpaceRange.StartRef().GetContainer();
      visibleWhiteSpaces.mEndOffset =
          trailingWhiteSpaceRange.StartRef().Offset();
    }
    visibleWhiteSpaces.SetEndByTrailingWhiteSpaces();
    return Some(visibleWhiteSpaces);
  }

  MOZ_ASSERT(StartsFromHardLineBreak());
  MOZ_ASSERT(maybeHaveLeadingWhiteSpaces);

  if (leadingWhiteSpaceRange.EndRef().IsSet()) {
    visibleWhiteSpaces.mStartNode =
        leadingWhiteSpaceRange.EndRef().GetContainer();
    visibleWhiteSpaces.mStartOffset = leadingWhiteSpaceRange.EndRef().Offset();
  }
  visibleWhiteSpaces.SetStartFromLeadingWhiteSpaces();
  if (!EndsByBlockBoundary()) {
    // then no trailing ws.  this normal run ends the overall ws run.
    if (mEnd.PointRef().IsSet()) {
      visibleWhiteSpaces.mEndNode = mEnd.PointRef().GetContainer();
      visibleWhiteSpaces.mEndOffset = mEnd.PointRef().Offset();
    }
    visibleWhiteSpaces.SetEndBy(mEnd.RawReason());
    return Some(visibleWhiteSpaces);
  }

  MOZ_ASSERT(EndsByBlockBoundary());

  if (!maybeHaveTrailingWhiteSpaces) {
    // normal ws runs right up to adjacent block (nbsp next to block)
    visibleWhiteSpaces.mEndNode = mEnd.PointRef().GetContainer();
    visibleWhiteSpaces.mEndOffset = mEnd.PointRef().Offset();
    visibleWhiteSpaces.SetEndBy(mEnd.RawReason());
    return Some(visibleWhiteSpaces);
  }

  if (trailingWhiteSpaceRange.StartRef().IsSet()) {
    visibleWhiteSpaces.mEndNode =
        trailingWhiteSpaceRange.StartRef().GetContainer();
    visibleWhiteSpaces.mEndOffset = trailingWhiteSpaceRange.StartRef().Offset();
  }
  visibleWhiteSpaces.SetEndByTrailingWhiteSpaces();
  return Some(visibleWhiteSpaces);
}

nsresult WSRunObject::PrepareToDeleteRangePriv(WSRunObject* aEndObject) {
  // this routine adjust white-space before *this* and after aEndObject
  // in preperation for the two areas to become adjacent after the
  // intervening content is deleted.  It's overly agressive right
  // now.  There might be a block boundary remaining between them after
  // the deletion, in which case these adjstments are unneeded (though
  // I don't think they can ever be harmful?)

  if (NS_WARN_IF(!aEndObject)) {
    return NS_ERROR_INVALID_ARG;
  }

  TextFragmentData textFragmentDataAtStart(mStart, mEnd, mNBSPData, mPRE);
  const bool deleteStartEqualsOrIsBeforeTextStart =
      mScanStartPoint.EqualsOrIsBefore(textFragmentDataAtStart.StartRef());
  TextFragmentData textFragmentDataAtEnd(aEndObject->mStart, aEndObject->mEnd,
                                         aEndObject->mNBSPData,
                                         aEndObject->mPRE);
  const bool deleteEndIsAfterTextEnd =
      textFragmentDataAtEnd.EndRef().EqualsOrIsBefore(
          aEndObject->mScanStartPoint);
  if (deleteStartEqualsOrIsBeforeTextStart && deleteEndIsAfterTextEnd) {
    return NS_OK;
  }

  const EditorDOMRange invisibleLeadingWhiteSpaceRangeAtStart =
      !deleteStartEqualsOrIsBeforeTextStart
          ? textFragmentDataAtStart
                .GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(
                    mScanStartPoint)
          : EditorDOMRange();
  const Maybe<const VisibleWhiteSpacesData>
      nonPreformattedVisibleWhiteSpacesAtStart =
          !deleteStartEqualsOrIsBeforeTextStart &&
                  !textFragmentDataAtStart.IsPreformatted() &&
                  !invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned()
              ? textFragmentDataAtStart.CreateVisibleWhiteSpacesData()
              : Nothing();
  const PointPosition
      pointPositionWithNonPreformattedVisibleWhiteSpacesAtStart =
          nonPreformattedVisibleWhiteSpacesAtStart.isSome()
              ? nonPreformattedVisibleWhiteSpacesAtStart.ref().ComparePoint(
                    mScanStartPoint)
              : PointPosition::NotInSameDOMTree;
  const EditorDOMRange invisibleTrailingWhiteSpaceRangeAtEnd =
      !deleteEndIsAfterTextEnd
          ? textFragmentDataAtEnd
                .GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt(
                    aEndObject->mScanStartPoint)
          : EditorDOMRange();
  const Maybe<const VisibleWhiteSpacesData>
      nonPreformattedVisibleWhiteSpacesAtEnd =
          !deleteEndIsAfterTextEnd && !textFragmentDataAtEnd.IsPreformatted() &&
                  !invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned()
              ? textFragmentDataAtEnd.CreateVisibleWhiteSpacesData()
              : Nothing();
  const PointPosition pointPositionWithNonPreformattedVisibleWhiteSpacesAtEnd =
      nonPreformattedVisibleWhiteSpacesAtEnd.isSome()
          ? nonPreformattedVisibleWhiteSpacesAtEnd.ref().ComparePoint(
                aEndObject->mScanStartPoint)
          : PointPosition::NotInSameDOMTree;
  const bool followingContentMayBecomeFirstVisibleContent =
      textFragmentDataAtStart.FollowingContentMayBecomeFirstVisibleContent(
          mScanStartPoint);
  const bool precedingContentMayBecomeInvisible =
      textFragmentDataAtEnd.PrecedingContentMayBecomeInvisible(
          aEndObject->mScanStartPoint);

  if (!deleteEndIsAfterTextEnd) {
    // If deleting range is followed by invisible trailing white-spaces, we need
    // to remove it for making them not visible.
    if (invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned()) {
      if (!invisibleTrailingWhiteSpaceRangeAtEnd.Collapsed()) {
        // XXX Why don't we remove all invisible white-spaces?
        MOZ_ASSERT(invisibleTrailingWhiteSpaceRangeAtEnd.StartRef() ==
                   aEndObject->mScanStartPoint);
        // mScanStartPoint will be referred bellow so that we need to keep
        // it a valid point.
        AutoEditorDOMPointChildInvalidator forgetChild(mScanStartPoint);
        nsresult rv = MOZ_KnownLive(mHTMLEditor)
                          .DeleteTextAndTextNodesWithTransaction(
                              invisibleTrailingWhiteSpaceRangeAtEnd.StartRef(),
                              invisibleTrailingWhiteSpaceRangeAtEnd.EndRef());
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return rv;
        }
      }
    }
    // If end of the deleting range is followed by visible white-spaces which
    // is not preformatted, we might need to replace the following ASCII
    // white-spaces with an NBSP.
    else if (pointPositionWithNonPreformattedVisibleWhiteSpacesAtEnd ==
                 PointPosition::StartOfFragment ||
             pointPositionWithNonPreformattedVisibleWhiteSpacesAtEnd ==
                 PointPosition::MiddleOfFragment) {
      // If start of deleting range follows white-spaces or end of delete
      // will be start of a line, the following text cannot start with an
      // ASCII white-space for keeping it visible.
      if (followingContentMayBecomeFirstVisibleContent) {
        EditorDOMPointInText nextCharOfStartOfEnd =
            aEndObject->GetInclusiveNextEditableCharPoint(
                aEndObject->mScanStartPoint);
        if (nextCharOfStartOfEnd.IsSet() &&
            !nextCharOfStartOfEnd.IsEndOfContainer() &&
            nextCharOfStartOfEnd.IsCharASCIISpace()) {
          // mScanStartPoint will be referred bellow so that we need to keep
          // it a valid point.
          AutoEditorDOMPointChildInvalidator forgetChild(mScanStartPoint);
          if (nextCharOfStartOfEnd.IsStartOfContainer() ||
              nextCharOfStartOfEnd.IsPreviousCharASCIISpace()) {
            nextCharOfStartOfEnd =
                GetFirstASCIIWhiteSpacePointCollapsedTo(nextCharOfStartOfEnd);
          }
          EditorDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
              GetEndOfCollapsibleASCIIWhiteSpaces(nextCharOfStartOfEnd);
          nsresult rv = aEndObject->ReplaceASCIIWhiteSpacesWithOneNBSP(
              nextCharOfStartOfEnd, endOfCollapsibleASCIIWhiteSpaces);
          if (NS_FAILED(rv)) {
            NS_WARNING(
                "WSRunObject::ReplaceASCIIWhiteSpacesWithOneNBSP() failed");
            return rv;
          }
        }
      }
    }
  }

  if (deleteStartEqualsOrIsBeforeTextStart) {
    return NS_OK;
  }

  // If deleting range follows invisible leading white-spaces, we need to
  // remove them for making them not visible.
  if (invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned()) {
    if (invisibleLeadingWhiteSpaceRangeAtStart.Collapsed()) {
      return NS_OK;
    }

    // XXX Why don't we remove all leading white-spaces?
    // XXX Different from the other similar methods, mScanStartPoint may be
    //     different from end of invisibleLeadingWhiteSpaceRangeAtStart.
    nsresult rv = MOZ_KnownLive(mHTMLEditor)
                      .DeleteTextAndTextNodesWithTransaction(
                          invisibleLeadingWhiteSpaceRangeAtStart.StartRef(),
                          mScanStartPoint);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  // If start of the deleting range follows visible white-spaces which is not
  // preformatted, we might need to replace previous ASCII white-spaces with
  // an NBSP.
  if (pointPositionWithNonPreformattedVisibleWhiteSpacesAtStart ==
          PointPosition::MiddleOfFragment ||
      pointPositionWithNonPreformattedVisibleWhiteSpacesAtStart ==
          PointPosition::EndOfFragment) {
    // If end of the deleting range is (was) followed by white-spaces or
    // previous character of start of deleting range will be immediately
    // before a block boundary, the text cannot ends with an ASCII white-space
    // for keeping it visible.
    if (precedingContentMayBecomeInvisible) {
      EditorDOMPointInText atPreviousCharOfStart =
          GetPreviousEditableCharPoint(mScanStartPoint);
      if (atPreviousCharOfStart.IsSet() &&
          !atPreviousCharOfStart.IsEndOfContainer() &&
          atPreviousCharOfStart.IsCharASCIISpace()) {
        if (atPreviousCharOfStart.IsStartOfContainer() ||
            atPreviousCharOfStart.IsPreviousCharASCIISpace()) {
          atPreviousCharOfStart =
              GetFirstASCIIWhiteSpacePointCollapsedTo(atPreviousCharOfStart);
        }
        EditorDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
            GetEndOfCollapsibleASCIIWhiteSpaces(atPreviousCharOfStart);
        nsresult rv = ReplaceASCIIWhiteSpacesWithOneNBSP(
            atPreviousCharOfStart, endOfCollapsibleASCIIWhiteSpaces);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "WSRunObject::ReplaceASCIIWhiteSpacesWithOneNBSP() failed");
          return rv;
        }
      }
    }
  }
  return NS_OK;
}

nsresult WSRunObject::PrepareToSplitAcrossBlocksPriv() {
  // used to prepare white-space sequence to be split across two blocks.
  // The main issue here is make sure white-spaces around the split point
  // doesn't end up becoming non-significant leading or trailing ws after
  // the split.
  Maybe<VisibleWhiteSpacesData> visibleWhiteSpaces =
      TextFragmentData(mStart, mEnd, mNBSPData, mPRE)
          .CreateVisibleWhiteSpacesData();
  if (visibleWhiteSpaces.isNothing()) {
    return NS_OK;  // No visible white-space sequence.
  }

  PointPosition pointPositionWithVisibleWhiteSpaces =
      visibleWhiteSpaces.ref().ComparePoint(mScanStartPoint);

  // XXX If we split white-space sequence, the following code modify the DOM
  //     tree twice.  This is not reasonable and the latter change may touch
  //     wrong position.  We should do this once.

  // If we insert block boundary to start or middle of the white-space sequence,
  // the character at the insertion point needs to be an NBSP.
  if (pointPositionWithVisibleWhiteSpaces == PointPosition::StartOfFragment ||
      pointPositionWithVisibleWhiteSpaces == PointPosition::MiddleOfFragment) {
    EditorDOMPointInText atNextCharOfStart =
        GetInclusiveNextEditableCharPoint(mScanStartPoint);
    if (atNextCharOfStart.IsSet() && !atNextCharOfStart.IsEndOfContainer() &&
        atNextCharOfStart.IsCharASCIISpace()) {
      // mScanStartPoint will be referred bellow so that we need to keep
      // it a valid point.
      AutoEditorDOMPointChildInvalidator forgetChild(mScanStartPoint);
      if (atNextCharOfStart.IsStartOfContainer() ||
          atNextCharOfStart.IsPreviousCharASCIISpace()) {
        atNextCharOfStart =
            GetFirstASCIIWhiteSpacePointCollapsedTo(atNextCharOfStart);
      }
      EditorDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
          GetEndOfCollapsibleASCIIWhiteSpaces(atNextCharOfStart);
      nsresult rv = ReplaceASCIIWhiteSpacesWithOneNBSP(
          atNextCharOfStart, endOfCollapsibleASCIIWhiteSpaces);
      if (NS_FAILED(rv)) {
        NS_WARNING("WSRunObject::ReplaceASCIIWhiteSpacesWithOneNBSP() failed");
        return rv;
      }
    }
  }

  // If we insert block boundary to middle of or end of the white-space
  // sequence, the previous character at the insertion point needs to be an
  // NBSP.
  if (pointPositionWithVisibleWhiteSpaces == PointPosition::MiddleOfFragment ||
      pointPositionWithVisibleWhiteSpaces == PointPosition::EndOfFragment) {
    EditorDOMPointInText atPreviousCharOfStart =
        GetPreviousEditableCharPoint(mScanStartPoint);
    if (atPreviousCharOfStart.IsSet() &&
        !atPreviousCharOfStart.IsEndOfContainer() &&
        atPreviousCharOfStart.IsCharASCIISpace()) {
      if (atPreviousCharOfStart.IsStartOfContainer() ||
          atPreviousCharOfStart.IsPreviousCharASCIISpace()) {
        atPreviousCharOfStart =
            GetFirstASCIIWhiteSpacePointCollapsedTo(atPreviousCharOfStart);
      }
      EditorDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
          GetEndOfCollapsibleASCIIWhiteSpaces(atPreviousCharOfStart);
      nsresult rv = ReplaceASCIIWhiteSpacesWithOneNBSP(
          atPreviousCharOfStart, endOfCollapsibleASCIIWhiteSpaces);
      if (NS_FAILED(rv)) {
        NS_WARNING("WSRunObject::ReplaceASCIIWhiteSpacesWithOneNBSP() failed");
        return rv;
      }
    }
  }
  return NS_OK;
}

template <typename PT, typename CT>
EditorDOMPointInText WSRunScanner::GetInclusiveNextEditableCharPoint(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (NS_WARN_IF(!aPoint.IsInContentNode()) ||
      NS_WARN_IF(!mScanStartPoint.IsInContentNode())) {
    return EditorDOMPointInText();
  }

  EditorRawDOMPoint point;
  if (nsIContent* child =
          aPoint.CanContainerHaveChildren() ? aPoint.GetChild() : nullptr) {
    nsIContent* leafContent = child->HasChildren()
                                  ? HTMLEditUtils::GetFirstLeafChild(
                                        *child, ChildBlockBoundary::Ignore)
                                  : child;
    if (NS_WARN_IF(!leafContent)) {
      return EditorDOMPointInText();
    }
    point.Set(leafContent, 0);
  } else {
    point = aPoint;
  }

  // If it points a character in a text node, return it.
  // XXX For the performance, this does not check whether the container
  //     is outside of our range.
  if (point.IsInTextNode() && point.GetContainer()->IsEditable() &&
      !point.IsEndOfContainer()) {
    return EditorDOMPointInText(point.ContainerAsText(), point.Offset());
  }

  if (point.GetContainer() == mEnd.GetReasonContent()) {
    return EditorDOMPointInText();
  }

  nsIContent* editableBlockParentOrTopmotEditableInlineContent =
      GetEditableBlockParentOrTopmotEditableInlineContent(
          mScanStartPoint.ContainerAsContent());
  if (NS_WARN_IF(!editableBlockParentOrTopmotEditableInlineContent)) {
    // Meaning that the container of `mScanStartPoint` is not editable.
    editableBlockParentOrTopmotEditableInlineContent =
        mScanStartPoint.ContainerAsContent();
  }

  for (nsIContent* nextContent =
           HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
               *point.ContainerAsContent(),
               *editableBlockParentOrTopmotEditableInlineContent, mEditingHost);
       nextContent;
       nextContent = HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
           *nextContent, *editableBlockParentOrTopmotEditableInlineContent,
           mEditingHost)) {
    if (!nextContent->IsText() || !nextContent->IsEditable()) {
      if (nextContent == mEnd.GetReasonContent()) {
        break;  // Reached end of current runs.
      }
      continue;
    }
    return EditorDOMPointInText(nextContent->AsText(), 0);
  }
  return EditorDOMPointInText();
}

template <typename PT, typename CT>
EditorDOMPointInText WSRunScanner::GetPreviousEditableCharPoint(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (NS_WARN_IF(!aPoint.IsInContentNode()) ||
      NS_WARN_IF(!mScanStartPoint.IsInContentNode())) {
    return EditorDOMPointInText();
  }

  EditorRawDOMPoint point;
  if (nsIContent* previousChild = aPoint.CanContainerHaveChildren()
                                      ? aPoint.GetPreviousSiblingOfChild()
                                      : nullptr) {
    nsIContent* leafContent =
        previousChild->HasChildren()
            ? HTMLEditUtils::GetLastLeafChild(*previousChild,
                                              ChildBlockBoundary::Ignore)
            : previousChild;
    if (NS_WARN_IF(!leafContent)) {
      return EditorDOMPointInText();
    }
    point.SetToEndOf(leafContent);
  } else {
    point = aPoint;
  }

  // If it points a character in a text node and it's not first character
  // in it, return its previous point.
  // XXX For the performance, this does not check whether the container
  //     is outside of our range.
  if (point.IsInTextNode() && point.GetContainer()->IsEditable() &&
      !point.IsStartOfContainer()) {
    return EditorDOMPointInText(point.ContainerAsText(), point.Offset() - 1);
  }

  if (point.GetContainer() == mStart.GetReasonContent()) {
    return EditorDOMPointInText();
  }

  nsIContent* editableBlockParentOrTopmotEditableInlineContent =
      GetEditableBlockParentOrTopmotEditableInlineContent(
          mScanStartPoint.ContainerAsContent());
  if (NS_WARN_IF(!editableBlockParentOrTopmotEditableInlineContent)) {
    // Meaning that the container of `mScanStartPoint` is not editable.
    editableBlockParentOrTopmotEditableInlineContent =
        mScanStartPoint.ContainerAsContent();
  }

  for (nsIContent* previousContent =
           HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
               *point.ContainerAsContent(),
               *editableBlockParentOrTopmotEditableInlineContent, mEditingHost);
       previousContent;
       previousContent =
           HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
               *previousContent,
               *editableBlockParentOrTopmotEditableInlineContent,
               mEditingHost)) {
    if (!previousContent->IsText() || !previousContent->IsEditable()) {
      if (previousContent == mStart.GetReasonContent()) {
        break;  // Reached start of current runs.
      }
      continue;
    }
    return EditorDOMPointInText(
        previousContent->AsText(),
        previousContent->AsText()->TextLength()
            ? previousContent->AsText()->TextLength() - 1
            : 0);
  }
  return EditorDOMPointInText();
}

EditorDOMPointInText WSRunScanner::GetEndOfCollapsibleASCIIWhiteSpaces(
    const EditorDOMPointInText& aPointAtASCIIWhiteSpace) const {
  MOZ_ASSERT(aPointAtASCIIWhiteSpace.IsSet());
  MOZ_ASSERT(!aPointAtASCIIWhiteSpace.IsEndOfContainer());
  MOZ_ASSERT(aPointAtASCIIWhiteSpace.IsCharASCIISpace());

  // If it's not the last character in the text node, let's scan following
  // characters in it.
  if (!aPointAtASCIIWhiteSpace.IsAtLastContent()) {
    Maybe<uint32_t> nextVisibleCharOffset =
        HTMLEditUtils::GetNextCharOffsetExceptASCIIWhiteSpaces(
            aPointAtASCIIWhiteSpace);
    if (nextVisibleCharOffset.isSome()) {
      // There is non-white-space character in it.
      return EditorDOMPointInText(aPointAtASCIIWhiteSpace.ContainerAsText(),
                                  nextVisibleCharOffset.value());
    }
  }

  // Otherwise, i.e., the text node ends with ASCII white-space, keep scanning
  // the following text nodes.
  // XXX Perhaps, we should stop scanning if there is non-editable and visible
  //     content.
  EditorDOMPointInText afterLastWhiteSpace =
      EditorDOMPointInText::AtEndOf(*aPointAtASCIIWhiteSpace.ContainerAsText());
  for (EditorDOMPointInText atEndOfPreviousTextNode = afterLastWhiteSpace;;) {
    EditorDOMPointInText atStartOfNextTextNode =
        GetInclusiveNextEditableCharPoint(atEndOfPreviousTextNode);
    if (!atStartOfNextTextNode.IsSet()) {
      // There is no more text nodes.  Return end of the previous text node.
      return afterLastWhiteSpace;
    }

    // We can ignore empty text nodes.
    if (atStartOfNextTextNode.IsContainerEmpty()) {
      atEndOfPreviousTextNode = atStartOfNextTextNode;
      continue;
    }

    // If next node starts with non-white-space character, return end of
    // previous text node.
    if (!atStartOfNextTextNode.IsCharASCIISpace()) {
      return afterLastWhiteSpace;
    }

    // Otherwise, scan the text node.
    Maybe<uint32_t> nextVisibleCharOffset =
        HTMLEditUtils::GetNextCharOffsetExceptASCIIWhiteSpaces(
            atStartOfNextTextNode);
    if (nextVisibleCharOffset.isSome()) {
      return EditorDOMPointInText(atStartOfNextTextNode.ContainerAsText(),
                                  nextVisibleCharOffset.value());
    }

    // The next text nodes ends with white-space too.  Try next one.
    afterLastWhiteSpace = atEndOfPreviousTextNode =
        EditorDOMPointInText::AtEndOf(*atStartOfNextTextNode.ContainerAsText());
  }
}

EditorDOMPointInText WSRunScanner::GetFirstASCIIWhiteSpacePointCollapsedTo(
    const EditorDOMPointInText& aPointAtASCIIWhiteSpace) const {
  MOZ_ASSERT(aPointAtASCIIWhiteSpace.IsSet());
  MOZ_ASSERT(!aPointAtASCIIWhiteSpace.IsEndOfContainer());
  MOZ_ASSERT(aPointAtASCIIWhiteSpace.IsCharASCIISpace());

  // If there is some characters before it, scan it in the text node first.
  if (!aPointAtASCIIWhiteSpace.IsStartOfContainer()) {
    uint32_t firstASCIIWhiteSpaceOffset =
        HTMLEditUtils::GetFirstASCIIWhiteSpaceOffsetCollapsedWith(
            aPointAtASCIIWhiteSpace);
    if (firstASCIIWhiteSpaceOffset) {
      // There is a non-white-space character in it.
      return EditorDOMPointInText(aPointAtASCIIWhiteSpace.ContainerAsText(),
                                  firstASCIIWhiteSpaceOffset);
    }
  }

  // Otherwise, i.e., the text node starts with ASCII white-space, keep scanning
  // the preceding text nodes.
  // XXX Perhaps, we should stop scanning if there is non-editable and visible
  //     content.
  EditorDOMPointInText atLastWhiteSpace =
      EditorDOMPointInText(aPointAtASCIIWhiteSpace.ContainerAsText(), 0);
  for (EditorDOMPointInText atStartOfPreviousTextNode = atLastWhiteSpace;;) {
    EditorDOMPointInText atLastCharOfNextTextNode =
        GetPreviousEditableCharPoint(atStartOfPreviousTextNode);
    if (!atLastCharOfNextTextNode.IsSet()) {
      // There is no more text nodes.  Return end of last text node.
      return atLastWhiteSpace;
    }

    // We can ignore empty text nodes.
    if (atLastCharOfNextTextNode.IsContainerEmpty()) {
      atStartOfPreviousTextNode = atLastCharOfNextTextNode;
      continue;
    }

    // If next node ends with non-white-space character, return start of
    // previous text node.
    if (!atLastCharOfNextTextNode.IsCharASCIISpace()) {
      return atLastWhiteSpace;
    }

    // Otherwise, scan the text node.
    uint32_t firstASCIIWhiteSpaceOffset =
        HTMLEditUtils::GetFirstASCIIWhiteSpaceOffsetCollapsedWith(
            atLastCharOfNextTextNode);
    if (firstASCIIWhiteSpaceOffset) {
      return EditorDOMPointInText(atLastCharOfNextTextNode.ContainerAsText(),
                                  firstASCIIWhiteSpaceOffset);
    }

    // The next text nodes starts with white-space too.  Try next one.
    atLastWhiteSpace = atStartOfPreviousTextNode =
        EditorDOMPointInText(atLastCharOfNextTextNode.ContainerAsText(), 0);
  }
}

nsresult WSRunObject::ReplaceASCIIWhiteSpacesWithOneNBSP(
    const EditorDOMPointInText& aAtFirstASCIIWhiteSpace,
    const EditorDOMPointInText& aEndOfCollapsibleASCIIWhiteSpaces) {
  MOZ_ASSERT(aAtFirstASCIIWhiteSpace.IsSetAndValid());
  MOZ_ASSERT(!aAtFirstASCIIWhiteSpace.IsEndOfContainer());
  MOZ_ASSERT(aAtFirstASCIIWhiteSpace.IsCharASCIISpace());
  MOZ_ASSERT(aEndOfCollapsibleASCIIWhiteSpaces.IsSetAndValid());
  MOZ_ASSERT(aEndOfCollapsibleASCIIWhiteSpaces.IsEndOfContainer() ||
             !aEndOfCollapsibleASCIIWhiteSpaces.IsCharASCIISpace());

  AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
  nsresult rv =
      MOZ_KnownLive(mHTMLEditor)
          .ReplaceTextWithTransaction(
              MOZ_KnownLive(*aAtFirstASCIIWhiteSpace.ContainerAsText()),
              aAtFirstASCIIWhiteSpace.Offset(),
              aAtFirstASCIIWhiteSpace.ContainerAsText() ==
                      aEndOfCollapsibleASCIIWhiteSpaces.ContainerAsText()
                  ? aEndOfCollapsibleASCIIWhiteSpaces.Offset() -
                        aAtFirstASCIIWhiteSpace.Offset()
                  : aAtFirstASCIIWhiteSpace.ContainerAsText()->TextLength() -
                        aAtFirstASCIIWhiteSpace.Offset(),
              nsDependentSubstring(&kNBSP, 1));
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
    return rv;
  }

  if (aAtFirstASCIIWhiteSpace.GetContainer() ==
      aEndOfCollapsibleASCIIWhiteSpaces.GetContainer()) {
    return NS_OK;
  }

  // We need to remove the following unnecessary ASCII white-spaces because we
  // collapsed them into the start node.
  rv = MOZ_KnownLive(mHTMLEditor)
           .DeleteTextAndTextNodesWithTransaction(
               EditorDOMPointInText::AtEndOf(
                   *aAtFirstASCIIWhiteSpace.ContainerAsText()),
               aEndOfCollapsibleASCIIWhiteSpaces);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
  return rv;
}

char16_t WSRunScanner::GetCharAt(Text* aTextNode, int32_t aOffset) const {
  // return 0 if we can't get a char, for whatever reason
  if (NS_WARN_IF(!aTextNode) || NS_WARN_IF(aOffset < 0) ||
      NS_WARN_IF(aOffset >=
                 static_cast<int32_t>(aTextNode->TextDataLength()))) {
    return 0;
  }
  return aTextNode->TextFragment().CharAt(aOffset);
}

nsresult WSRunObject::NormalizeWhiteSpacesAtEndOf(
    const VisibleWhiteSpacesData& aVisibleWhiteSpacesData) {
  // Remove this block if we ship Blink-compat white-space normalization.
  if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    // now check that what is to the left of it is compatible with replacing
    // nbsp with space
    EditorDOMPoint atEndOfVisibleWhiteSpaces =
        aVisibleWhiteSpacesData.EndPoint();
    EditorDOMPointInText atPreviousCharOfEndOfVisibleWhiteSpaces =
        GetPreviousEditableCharPoint(atEndOfVisibleWhiteSpaces);
    if (!atPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() ||
        atPreviousCharOfEndOfVisibleWhiteSpaces.IsEndOfContainer() ||
        !atPreviousCharOfEndOfVisibleWhiteSpaces.IsCharNBSP()) {
      return NS_OK;
    }

    // now check that what is to the left of it is compatible with replacing
    // nbsp with space
    EditorDOMPointInText atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces =
        GetPreviousEditableCharPoint(atPreviousCharOfEndOfVisibleWhiteSpaces);
    bool isPreviousCharASCIIWhiteSpace =
        atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
        !atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
             .IsEndOfContainer() &&
        atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
            .IsCharASCIISpace();
    bool maybeNBSPFollowingVisibleContent =
        (atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
         !isPreviousCharASCIIWhiteSpace) ||
        (!atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
         (aVisibleWhiteSpacesData.StartsFromNormalText() ||
          aVisibleWhiteSpacesData.StartsFromSpecialContent()));
    bool followedByVisibleContentOrBRElement = false;

    // If the NBSP follows a visible content or an ASCII white-space, i.e.,
    // unless NBSP is first character and start of a block, we may need to
    // insert <br> element and restore the NBSP to an ASCII white-space.
    if (maybeNBSPFollowingVisibleContent || isPreviousCharASCIIWhiteSpace) {
      followedByVisibleContentOrBRElement =
          aVisibleWhiteSpacesData.EndsByNormalText() ||
          aVisibleWhiteSpacesData.EndsBySpecialContent() ||
          aVisibleWhiteSpacesData.EndsByBRElement();
      // First, try to insert <br> element if NBSP is at end of a block.
      // XXX We should stop this if there is a visible content.
      if (aVisibleWhiteSpacesData.EndsByBlockBoundary() &&
          mScanStartPoint.IsInContentNode()) {
        bool insertBRElement = HTMLEditUtils::IsBlockElement(
            *mScanStartPoint.ContainerAsContent());
        if (!insertBRElement) {
          nsIContent* blockParentOrTopmostEditableInlineContent =
              GetEditableBlockParentOrTopmotEditableInlineContent(
                  mScanStartPoint.ContainerAsContent());
          insertBRElement = blockParentOrTopmostEditableInlineContent &&
                            HTMLEditUtils::IsBlockElement(
                                *blockParentOrTopmostEditableInlineContent);
        }
        if (insertBRElement) {
          // We are at a block boundary.  Insert a <br>.  Why?  Well, first note
          // that the br will have no visible effect since it is up against a
          // block boundary.  |foo<br><p>bar| renders like |foo<p>bar| and
          // similarly |<p>foo<br></p>bar| renders like |<p>foo</p>bar|.  What
          // this <br> addition gets us is the ability to convert a trailing
          // nbsp to a space.  Consider: |<body>foo. '</body>|, where '
          // represents selection.  User types space attempting to put 2 spaces
          // after the end of their sentence.  We used to do this as:
          // |<body>foo. &nbsp</body>|  This caused problems with soft wrapping:
          // the nbsp would wrap to the next line, which looked attrocious.  If
          // you try to do: |<body>foo.&nbsp </body>| instead, the trailing
          // space is invisible because it is against a block boundary.  If you
          // do:
          // |<body>foo.&nbsp&nbsp</body>| then you get an even uglier soft
          // wrapping problem, where foo is on one line until you type the final
          // space, and then "foo  " jumps down to the next line.  Ugh.  The
          // best way I can find out of this is to throw in a harmless <br>
          // here, which allows us to do: |<body>foo.&nbsp <br></body>|, which
          // doesn't cause foo to jump lines, doesn't cause spaces to show up at
          // the beginning of soft wrapped lines, and lets the user see 2 spaces
          // when they type 2 spaces.

          RefPtr<Element> brElement =
              MOZ_KnownLive(mHTMLEditor)
                  .InsertBRElementWithTransaction(atEndOfVisibleWhiteSpaces);
          if (NS_WARN_IF(mHTMLEditor.Destroyed())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (!brElement) {
            NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
            return NS_ERROR_FAILURE;
          }

          atPreviousCharOfEndOfVisibleWhiteSpaces =
              GetPreviousEditableCharPoint(atEndOfVisibleWhiteSpaces);
          atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces =
              GetPreviousEditableCharPoint(
                  atPreviousCharOfEndOfVisibleWhiteSpaces);
          isPreviousCharASCIIWhiteSpace =
              atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
              !atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                   .IsEndOfContainer() &&
              atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                  .IsCharASCIISpace();
          followedByVisibleContentOrBRElement = true;
        }
      }

      // Next, replace the NBSP with an ASCII white-space if it's surrounded
      // by visible contents (or immediately before a <br> element).
      if (maybeNBSPFollowingVisibleContent &&
          followedByVisibleContentOrBRElement) {
        AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
        nsresult rv =
            MOZ_KnownLive(mHTMLEditor)
                .ReplaceTextWithTransaction(
                    MOZ_KnownLive(*atPreviousCharOfEndOfVisibleWhiteSpaces
                                       .ContainerAsText()),
                    atPreviousCharOfEndOfVisibleWhiteSpaces.Offset(), 1,
                    u" "_ns);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "HTMLEditor::ReplaceTextWithTransaction() failed");
        return rv;
      }
    }
    // If the text node is not preformatted, and the NBSP is followed by a <br>
    // element and following (maybe multiple) ASCII spaces, remove the NBSP,
    // but inserts a NBSP before the spaces.  This makes a line break
    // opportunity to wrap the line.
    // XXX This is different behavior from Blink.  Blink generates pairs of
    //     an NBSP and an ASCII white-space, but put NBSP at the end of the
    //     sequence.  We should follow the behavior for web-compat.
    if (mPRE || maybeNBSPFollowingVisibleContent ||
        !isPreviousCharASCIIWhiteSpace ||
        !followedByVisibleContentOrBRElement) {
      return NS_OK;
    }

    // Currently, we're at an NBSP following an ASCII space, and we need to
    // replace them with `"&nbsp; "` for avoiding collapsing white-spaces.
    MOZ_ASSERT(!atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                    .IsEndOfContainer());
    EditorDOMPointInText atFirstASCIIWhiteSpace =
        GetFirstASCIIWhiteSpacePointCollapsedTo(
            atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces);
    AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
    uint32_t numberOfASCIIWhiteSpacesInStartNode =
        atFirstASCIIWhiteSpace.ContainerAsText() ==
                atPreviousCharOfEndOfVisibleWhiteSpaces.ContainerAsText()
            ? atPreviousCharOfEndOfVisibleWhiteSpaces.Offset() -
                  atFirstASCIIWhiteSpace.Offset()
            : atFirstASCIIWhiteSpace.ContainerAsText()->Length() -
                  atFirstASCIIWhiteSpace.Offset();
    // Replace all preceding ASCII white-spaces **and** the NBSP.
    uint32_t replaceLengthInStartNode =
        numberOfASCIIWhiteSpacesInStartNode +
        (atFirstASCIIWhiteSpace.ContainerAsText() ==
                 atPreviousCharOfEndOfVisibleWhiteSpaces.ContainerAsText()
             ? 1
             : 0);
    nsresult rv =
        MOZ_KnownLive(mHTMLEditor)
            .ReplaceTextWithTransaction(
                MOZ_KnownLive(*atFirstASCIIWhiteSpace.ContainerAsText()),
                atFirstASCIIWhiteSpace.Offset(), replaceLengthInStartNode,
                u"\x00A0 "_ns);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
      return rv;
    }

    if (atFirstASCIIWhiteSpace.GetContainer() ==
        atPreviousCharOfEndOfVisibleWhiteSpaces.GetContainer()) {
      return NS_OK;
    }

    // We need to remove the following unnecessary ASCII white-spaces and
    // NBSP at atPreviousCharOfEndOfVisibleWhiteSpaces because we collapsed them
    // into the start node.
    rv = MOZ_KnownLive(mHTMLEditor)
             .DeleteTextAndTextNodesWithTransaction(
                 EditorDOMPointInText::AtEndOf(
                     *atFirstASCIIWhiteSpace.ContainerAsText()),
                 atPreviousCharOfEndOfVisibleWhiteSpaces.NextPoint());
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  // XXX This is called when top-level edit sub-action handling ends for
  //     3 points at most.  However, this is not compatible with Blink.
  //     Blink touches white-space sequence which includes new character
  //     or following white-space sequence of new <br> element or, if and
  //     only if deleting range is followed by white-space sequence (i.e.,
  //     not touched previous white-space sequence of deleting range).
  //     This should be done when we change to make each edit action
  //     handler directly normalize white-space sequence rather than
  //     OnEndHandlingTopLevelEditSucAction().

  // First, check if the last character is an NBSP.  Otherwise, we don't need
  // to do nothing here.
  EditorDOMPoint atEndOfVisibleWhiteSpaces = aVisibleWhiteSpacesData.EndPoint();
  EditorDOMPointInText atPreviousCharOfEndOfVisibleWhiteSpaces =
      GetPreviousEditableCharPoint(atEndOfVisibleWhiteSpaces);
  if (!atPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() ||
      atPreviousCharOfEndOfVisibleWhiteSpaces.IsEndOfContainer() ||
      !atPreviousCharOfEndOfVisibleWhiteSpaces.IsCharNBSP()) {
    return NS_OK;
  }

  // Next, consider the range to collapse ASCII white-spaces before there.
  EditorDOMPointInText startToDelete, endToDelete;

  EditorDOMPointInText atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces =
      GetPreviousEditableCharPoint(atPreviousCharOfEndOfVisibleWhiteSpaces);
  // If there are some preceding ASCII white-spaces, we need to treat them
  // as one white-space.  I.e., we need to collapse them.
  if (atPreviousCharOfEndOfVisibleWhiteSpaces.IsCharNBSP() &&
      atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
      atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
          .IsCharASCIISpace()) {
    startToDelete = GetFirstASCIIWhiteSpacePointCollapsedTo(
        atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces);
    endToDelete = atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces;
  }
  // Otherwise, we don't need to remove any white-spaces, but we may need
  // to normalize the white-space sequence containing the previous NBSP.
  else {
    startToDelete = endToDelete =
        atPreviousCharOfEndOfVisibleWhiteSpaces.NextPoint();
  }

  AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
  nsresult rv = MOZ_KnownLive(mHTMLEditor)
                    .DeleteTextAndNormalizeSurroundingWhiteSpaces(startToDelete,
                                                                  endToDelete);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::DeleteTextAndNormalizeSurroundingWhiteSpaces() failed");
  return rv;
}

nsresult WSRunObject::MaybeReplacePreviousNBSPWithASCIIWhiteSpace(
    const VisibleWhiteSpacesData& aVisibleWhiteSpacesData,
    const EditorDOMPoint& aPoint) {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  // Try to change an NBSP to a space, if possible, just to prevent NBSP
  // proliferation.  This routine is called when we are about to make this
  // point in the ws abut an inserted break or text, so we don't have to worry
  // about what is after it.  What is after it now will end up after the
  // inserted object.
  EditorDOMPointInText atPreviousChar = GetPreviousEditableCharPoint(aPoint);
  if (!atPreviousChar.IsSet() || atPreviousChar.IsEndOfContainer() ||
      !atPreviousChar.IsCharNBSP()) {
    return NS_OK;
  }

  EditorDOMPointInText atPreviousCharOfPreviousChar =
      GetPreviousEditableCharPoint(atPreviousChar);
  if (atPreviousCharOfPreviousChar.IsSet()) {
    // If the previous char of the NBSP at previous position of aPoint is
    // an ASCII white-space, we don't need to replace it with same character.
    if (!atPreviousCharOfPreviousChar.IsEndOfContainer() &&
        atPreviousCharOfPreviousChar.IsCharASCIISpace()) {
      return NS_OK;
    }
  }
  // If previous content of the NBSP is block boundary, we cannot replace the
  // NBSP with an ASCII white-space to keep it rendered.
  else if (!aVisibleWhiteSpacesData.StartsFromNormalText() &&
           !aVisibleWhiteSpacesData.StartsFromSpecialContent()) {
    return NS_OK;
  }

  AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
  nsresult rv = MOZ_KnownLive(mHTMLEditor)
                    .ReplaceTextWithTransaction(
                        MOZ_KnownLive(*atPreviousChar.ContainerAsText()),
                        atPreviousChar.Offset(), 1, u" "_ns);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::ReplaceTextWithTransaction() failed");
  return rv;
}

nsresult WSRunObject::MaybeReplaceInclusiveNextNBSPWithASCIIWhiteSpace(
    const VisibleWhiteSpacesData& aVisibleWhiteSpacesData,
    const EditorDOMPoint& aPoint) {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  // Try to change an nbsp to a space, if possible, just to prevent nbsp
  // proliferation This routine is called when we are about to make this point
  // in the ws abut an inserted text, so we don't have to worry about what is
  // before it.  What is before it now will end up before the inserted text.
  EditorDOMPointInText atNextChar = GetInclusiveNextEditableCharPoint(aPoint);
  if (!atNextChar.IsSet() || NS_WARN_IF(atNextChar.IsEndOfContainer()) ||
      !atNextChar.IsCharNBSP()) {
    return NS_OK;
  }

  EditorDOMPointInText atNextCharOfNextCharOfNBSP =
      GetInclusiveNextEditableCharPoint(atNextChar.NextPoint());
  if (atNextCharOfNextCharOfNBSP.IsSet()) {
    // If following character of an NBSP is an ASCII white-space, we don't
    // need to replace it with same character.
    if (!atNextCharOfNextCharOfNBSP.IsEndOfContainer() &&
        atNextCharOfNextCharOfNBSP.IsCharASCIISpace()) {
      return NS_OK;
    }
  }
  // If the NBSP is last character in the hard line, we don't need to
  // replace it because it's required to render multiple white-spaces.
  else if (!aVisibleWhiteSpacesData.EndsByNormalText() &&
           !aVisibleWhiteSpacesData.EndsBySpecialContent() &&
           !aVisibleWhiteSpacesData.EndsByBRElement()) {
    return NS_OK;
  }

  AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
  nsresult rv = MOZ_KnownLive(mHTMLEditor)
                    .ReplaceTextWithTransaction(
                        MOZ_KnownLive(*atNextChar.ContainerAsText()),
                        atNextChar.Offset(), 1, u" "_ns);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::ReplaceTextWithTransaction() failed");
  return rv;
}

nsresult WSRunObject::DeleteInvisibleASCIIWhiteSpacesInternal() {
  TextFragmentData textFragment(mStart, mEnd, mNBSPData, mPRE);
  EditorDOMRange leadingWhiteSpaceRange =
      textFragment.GetInvisibleLeadingWhiteSpaceRange();
  // XXX Getting trailing white-space range now must be wrong because
  //     mutation event listener may invalidate it.
  EditorDOMRange trailingWhiteSpaceRange =
      textFragment.GetInvisibleTrailingWhiteSpaceRange();
  DebugOnly<bool> leadingWhiteSpacesDeleted = false;
  if (leadingWhiteSpaceRange.IsPositioned() &&
      !leadingWhiteSpaceRange.Collapsed()) {
    nsresult rv = MOZ_KnownLive(mHTMLEditor)
                      .DeleteTextAndTextNodesWithTransaction(
                          leadingWhiteSpaceRange.StartRef(),
                          leadingWhiteSpaceRange.EndRef());
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed to "
          "delete leading white-spaces");
      return rv;
    }
    leadingWhiteSpacesDeleted = true;
  }
  if (trailingWhiteSpaceRange.IsPositioned() &&
      !trailingWhiteSpaceRange.Collapsed() &&
      leadingWhiteSpaceRange != trailingWhiteSpaceRange) {
    NS_ASSERTION(!leadingWhiteSpacesDeleted,
                 "We're trying to remove trailing white-spaces with maybe "
                 "outdated range");
    nsresult rv = MOZ_KnownLive(mHTMLEditor)
                      .DeleteTextAndTextNodesWithTransaction(
                          trailingWhiteSpaceRange.StartRef(),
                          trailingWhiteSpaceRange.EndRef());
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed to "
          "delete trailing white-spaces");
      return rv;
    }
  }
  return NS_OK;
}

}  // namespace mozilla
