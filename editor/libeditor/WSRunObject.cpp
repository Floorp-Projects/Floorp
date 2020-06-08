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
template WSRunObject::WSRunObject(HTMLEditor& aHTMLEditor,
                                  const EditorDOMPoint& aScanStartPoint,
                                  const EditorDOMPoint& aScanEndPoint);
template WSRunObject::WSRunObject(HTMLEditor& aHTMLEditor,
                                  const EditorRawDOMPoint& aScanStartPoint,
                                  const EditorRawDOMPoint& aScanEndPoint);
template WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPoint& aPoint) const;
template WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorRawDOMPoint& aPoint) const;
template WSScanResult WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPoint& aPoint) const;
template WSScanResult WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom(
    const EditorRawDOMPoint& aPoint) const;

template <typename PT, typename CT>
WSRunScanner::WSRunScanner(const HTMLEditor* aHTMLEditor,
                           const EditorDOMPointBase<PT, CT>& aScanStartPoint,
                           const EditorDOMPointBase<PT, CT>& aScanEndPoint)
    : mScanStartPoint(aScanStartPoint),
      mScanEndPoint(aScanEndPoint),
      mEditingHost(aHTMLEditor->GetActiveEditingHost()),
      mPRE(false),
      mStartOffset(0),
      mEndOffset(0),
      mFirstNBSPOffset(0),
      mLastNBSPOffset(0),
      mStartRun(nullptr),
      mEndRun(nullptr),
      mHTMLEditor(aHTMLEditor),
      mStartReason(WSType::NotInitialized),
      mEndReason(WSType::NotInitialized) {
  MOZ_ASSERT(
      *nsContentUtils::ComparePoints(aScanStartPoint.ToRawRangeBoundary(),
                                     aScanEndPoint.ToRawRangeBoundary()) <= 0);
  DebugOnly<nsresult> rvIgnored = GetWSNodes();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "WSRunScanner::GetWSNodes() failed, but ignored");
  GetRuns();
}

WSRunScanner::~WSRunScanner() { ClearRuns(); }

template <typename PT, typename CT>
WSRunObject::WSRunObject(HTMLEditor& aHTMLEditor,
                         const EditorDOMPointBase<PT, CT>& aScanStartPoint,
                         const EditorDOMPointBase<PT, CT>& aScanEndPoint)
    : WSRunScanner(&aHTMLEditor, aScanStartPoint, aScanEndPoint),
      mHTMLEditor(aHTMLEditor) {}

// static
nsresult WSRunObject::Scrub(HTMLEditor& aHTMLEditor,
                            const EditorDOMPoint& aPoint) {
  MOZ_ASSERT(aPoint.IsSet());

  WSRunObject wsRunObject(aHTMLEditor, aPoint);
  nsresult rv = wsRunObject.Scrub();
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WSRunObject::Scrub() failed");
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

  WSFragment* beforeRun = FindNearestRun(aPointToInsert, false);
  WSFragment* afterRun = FindNearestRun(aPointToInsert, true);

  EditorDOMPoint pointToInsert(aPointToInsert);
  {
    // Some scoping for AutoTrackDOMPoint.  This will track our insertion
    // point while we tweak any surrounding whitespace
    AutoTrackDOMPoint tracker(mHTMLEditor.RangeUpdaterRef(), &pointToInsert);

    // Handle any changes needed to ws run after inserted br
    if (!afterRun || afterRun->IsEndOfHardLine()) {
      // Don't need to do anything.  Just insert break.  ws won't change.
    } else if (afterRun->IsStartOfHardLine()) {
      // Delete the leading ws that is after insertion point.  We don't
      // have to (it would still not be significant after br), but it's
      // just more aesthetically pleasing to.
      nsresult rv = MOZ_KnownLive(mHTMLEditor)
                        .DeleteTextAndTextNodesWithTransaction(
                            pointToInsert, afterRun->EndPoint());
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
        return nullptr;
      }
    } else if (afterRun->IsVisibleAndMiddleOfHardLine()) {
      // Need to determine if break at front of non-nbsp run.  If so, convert
      // run to nbsp.
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
          EditorDOMPointInText endOfCollapsibleASCIIWhitespaces =
              GetEndOfCollapsibleASCIIWhitespaces(atNextCharOfInsertionPoint);
          nsresult rv = ReplaceASCIIWhitespacesWithOneNBSP(
              atNextCharOfInsertionPoint, endOfCollapsibleASCIIWhitespaces);
          if (NS_FAILED(rv)) {
            NS_WARNING(
                "WSRunObject::ReplaceASCIIWhitespacesWithOneNBSP() failed");
            return nullptr;
          }
        }
      }
    }

    // Handle any changes needed to ws run before inserted br
    if (!beforeRun || beforeRun->IsStartOfHardLine()) {
      // Don't need to do anything.  Just insert break.  ws won't change.
    } else if (beforeRun->IsEndOfHardLine()) {
      // Need to delete the trailing ws that is before insertion point, because
      // it would become significant after break inserted.
      nsresult rv = MOZ_KnownLive(mHTMLEditor)
                        .DeleteTextAndTextNodesWithTransaction(
                            beforeRun->StartPoint(), pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WSRunObject::DeleteTextAndTextNodesWithTransaction() failed");
        return nullptr;
      }
    } else if (beforeRun->IsVisibleAndMiddleOfHardLine()) {
      // Try to change an nbsp to a space, just to prevent nbsp proliferation
      nsresult rv = MaybeReplacePreviousNBSPWithASCIIWhitespace(*beforeRun,
                                                                pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WSRunObject::MaybeReplacePreviousNBSPWithASCIIWhitespace() "
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

  WSFragment* beforeRun = FindNearestRun(mScanStartPoint, false);
  // If mScanStartPoint isn't equal to mScanEndPoint, it will replace text (i.e.
  // committing composition). And afterRun will be end point of replaced range.
  // So we want to know this white space type (trailing whitespace etc) of
  // this end point, not inserted (start) point, so we re-scan white space type.
  WSRunObject afterRunObject(MOZ_KnownLive(mHTMLEditor), mScanEndPoint);
  WSFragment* afterRun = afterRunObject.FindNearestRun(mScanEndPoint, true);

  EditorDOMPoint pointToInsert(mScanStartPoint);
  nsAutoString theString(aStringToInsert);
  {
    // Some scoping for AutoTrackDOMPoint.  This will track our insertion
    // point while we tweak any surrounding whitespace
    AutoTrackDOMPoint tracker(mHTMLEditor.RangeUpdaterRef(), &pointToInsert);

    // Handle any changes needed to ws run after inserted text
    if (!afterRun || afterRun->IsEndOfHardLine()) {
      // Don't need to do anything.  Just insert text.  ws won't change.
    } else if (afterRun->IsStartOfHardLine()) {
      // Delete the leading ws that is after insertion point, because it
      // would become significant after text inserted.
      nsresult rv = MOZ_KnownLive(mHTMLEditor)
                        .DeleteTextAndTextNodesWithTransaction(
                            pointToInsert, afterRun->EndPoint());
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
        return rv;
      }
    } else if (afterRun->IsVisibleAndMiddleOfHardLine()) {
      // Try to change an nbsp to a space, if possible, just to prevent nbsp
      // proliferation
      nsresult rv = MaybeReplaceInclusiveNextNBSPWithASCIIWhitespace(
          *afterRun, pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WSRunObject::MaybeReplaceInclusiveNextNBSPWithASCIIWhitespace() "
            "failed");
        return rv;
      }
    }

    // Handle any changes needed to ws run before inserted text
    if (!beforeRun || beforeRun->IsStartOfHardLine()) {
      // Don't need to do anything.  Just insert text.  ws won't change.
    } else if (beforeRun->IsEndOfHardLine()) {
      // Need to delete the trailing ws that is before insertion point, because
      // it would become significant after text inserted.
      nsresult rv = MOZ_KnownLive(mHTMLEditor)
                        .DeleteTextAndTextNodesWithTransaction(
                            beforeRun->StartPoint(), pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
        return rv;
      }
    } else if (beforeRun->IsVisibleAndMiddleOfHardLine()) {
      // Try to change an nbsp to a space, if possible, just to prevent nbsp
      // proliferation
      nsresult rv = MaybeReplacePreviousNBSPWithASCIIWhitespace(*beforeRun,
                                                                pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WSRunObject::MaybeReplacePreviousNBSPWithASCIIWhitespace() "
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
    // We have a leading space
    if (beforeRun) {
      if (beforeRun->IsStartOfHardLine()) {
        theString.SetCharAt(kNBSP, 0);
      } else if (beforeRun->IsVisible()) {
        EditorDOMPointInText atPreviousChar =
            GetPreviousEditableCharPoint(pointToInsert);
        if (atPreviousChar.IsSet() && !atPreviousChar.IsEndOfContainer() &&
            atPreviousChar.IsCharASCIISpace()) {
          theString.SetCharAt(kNBSP, 0);
        }
      }
    } else if (StartsFromHardLineBreak()) {
      theString.SetCharAt(kNBSP, 0);
    }
  }

  // Then the tail
  uint32_t lastCharIndex = theString.Length() - 1;

  if (nsCRT::IsAsciiSpace(theString[lastCharIndex])) {
    // We have a leading space
    if (afterRun) {
      if (afterRun->IsEndOfHardLine()) {
        theString.SetCharAt(kNBSP, lastCharIndex);
      } else if (afterRun->IsVisible()) {
        EditorDOMPointInText atNextChar =
            GetInclusiveNextEditableCharPoint(pointToInsert);
        if (atNextChar.IsSet() && !atNextChar.IsEndOfContainer() &&
            atNextChar.IsCharASCIISpace()) {
          theString.SetCharAt(kNBSP, lastCharIndex);
        }
      }
    } else if (afterRunObject.EndsByBlockBoundary()) {
      // When afterRun is null, it means that mScanEndPoint is last point in
      // editing host or editing block.
      // If this text insertion replaces composition, this.mEndReason is
      // start position of compositon. So we have to use afterRunObject's
      // reason instead.
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
        GetFirstASCIIWhitespacePointCollapsedTo(atPreviousCharOfStart);
    EditorDOMPoint endToDelete =
        GetEndOfCollapsibleASCIIWhitespaces(atPreviousCharOfStart);
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
        GetFirstASCIIWhitespacePointCollapsedTo(atNextCharOfStart);
    EditorDOMPoint endToDelete =
        GetEndOfCollapsibleASCIIWhitespaces(atNextCharOfStart);
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
  // Find first visible thing before the point.  Position
  // outVisNode/outVisOffset just _after_ that thing.  If we don't find
  // anything return start of ws.
  MOZ_ASSERT(aPoint.IsSet());

  WSFragment* run = FindNearestRun(aPoint, false);

  // Is there a visible run there or earlier?
  for (; run; run = run->mLeft) {
    if (run->IsVisibleAndMiddleOfHardLine()) {
      EditorDOMPointInText atPreviousChar =
          GetPreviousEditableCharPoint(aPoint);
      // When it's a non-empty text node, return it.
      if (atPreviousChar.IsSet() && !atPreviousChar.IsContainerEmpty()) {
        MOZ_ASSERT(!atPreviousChar.IsEndOfContainer());
        return WSScanResult(
            atPreviousChar.NextPoint(),
            atPreviousChar.IsCharASCIISpace() || atPreviousChar.IsCharNBSP()
                ? WSType::NormalWhiteSpaces
                : WSType::NormalText);
      }
      // If no text node, keep looking.  We should eventually fall out of loop
    }
  }

  if (mStartReasonContent != mStartNode) {
    // In this case, mStartOffset is not meaningful.
    return WSScanResult(mStartReasonContent, mStartReason);
  }
  return WSScanResult(EditorDOMPoint(mStartReasonContent, mStartOffset),
                      mStartReason);
}

template <typename PT, typename CT>
WSScanResult WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  // Find first visible thing after the point.  Position
  // outVisNode/outVisOffset just _before_ that thing.  If we don't find
  // anything return end of ws.
  MOZ_ASSERT(aPoint.IsSet());

  WSFragment* run = FindNearestRun(aPoint, true);

  // Is there a visible run there or later?
  for (; run; run = run->mRight) {
    if (run->IsVisibleAndMiddleOfHardLine()) {
      EditorDOMPointInText atNextChar =
          GetInclusiveNextEditableCharPoint(aPoint);
      // When it's a non-empty text node, return it.
      if (atNextChar.IsSet() && !atNextChar.IsContainerEmpty()) {
        return WSScanResult(
            atNextChar,
            !atNextChar.IsEndOfContainer() &&
                    (atNextChar.IsCharASCIISpace() || atNextChar.IsCharNBSP())
                ? WSType::NormalWhiteSpaces
                : WSType::NormalText);
      }
      // If no text node, keep looking.  We should eventually fall out of loop
    }
  }

  if (mEndReasonContent != mEndNode) {
    // In this case, mEndOffset is not meaningful.
    return WSScanResult(mEndReasonContent, mEndReason);
  }
  return WSScanResult(EditorDOMPoint(mEndReasonContent, mEndOffset),
                      mEndReason);
}

nsresult WSRunObject::AdjustWhitespace() {
  // this routine examines a run of ws and tries to get rid of some unneeded
  // nbsp's, replacing them with regualr ascii space if possible.  Keeping
  // things simple for now and just trying to fix up the trailing ws in the run.
  if (!mLastNBSPNode) {
    // nothing to do!
    return NS_OK;
  }
  for (WSFragment* run = mStartRun; run; run = run->mRight) {
    if (!run->IsVisibleAndMiddleOfHardLine()) {
      continue;
    }
    nsresult rv = NormalizeWhitespacesAtEndOf(*run);
    if (NS_FAILED(rv)) {
      NS_WARNING("WSRunObject::NormalizeWhitespacesAtEndOf() failed");
      return rv;
    }
  }
  return NS_OK;
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
  // and which contain only whitespace.  Stop if you reach non-ws text or a new
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

  // first look backwards to find preceding ws nodes
  if (Text* textNode = mScanStartPoint.GetContainerAsText()) {
    const nsTextFragment* textFrag = &textNode->TextFragment();
    if (!mScanStartPoint.IsStartOfContainer()) {
      for (uint32_t i = mScanStartPoint.Offset(); i; i--) {
        // sanity bounds check the char position.  bug 136165
        if (i > textFrag->GetLength()) {
          MOZ_ASSERT_UNREACHABLE("looking beyond end of text fragment");
          continue;
        }
        char16_t theChar = textFrag->CharAt(i - 1);
        if (!nsCRT::IsAsciiSpace(theChar)) {
          if (theChar != kNBSP) {
            mStartNode = textNode;
            mStartOffset = i;
            mStartReason = WSType::NormalText;
            mStartReasonContent = textNode;
            break;
          }
          // as we look backwards update our earliest found nbsp
          mFirstNBSPNode = textNode;
          mFirstNBSPOffset = i - 1;
          // also keep track of latest nbsp so far
          if (!mLastNBSPNode) {
            mLastNBSPNode = textNode;
            mLastNBSPOffset = i - 1;
          }
        }
        start.Set(textNode, i - 1);
      }
    }
  }

  while (!mStartNode) {
    // we haven't found the start of ws yet.  Keep looking
    nsIContent* previousLeafContentOrBlock =
        HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
            start, *editableBlockParentOrTopmotEditableInlineContent,
            mEditingHost);
    if (previousLeafContentOrBlock) {
      if (HTMLEditUtils::IsBlockElement(*previousLeafContentOrBlock)) {
        mStartNode = start.GetContainer();
        mStartOffset = start.Offset();
        mStartReason = WSType::OtherBlockBoundary;
        mStartReasonContent = previousLeafContentOrBlock;
      } else if (previousLeafContentOrBlock->IsText() &&
                 previousLeafContentOrBlock->IsEditable()) {
        RefPtr<Text> textNode = previousLeafContentOrBlock->AsText();
        const nsTextFragment* textFrag = &textNode->TextFragment();
        uint32_t len = textNode->TextLength();

        if (len < 1) {
          // Zero length text node. Set start point to it
          // so we can get past it!
          start.Set(previousLeafContentOrBlock, 0);
        } else {
          for (int32_t pos = len - 1; pos >= 0; pos--) {
            // sanity bounds check the char position.  bug 136165
            if (uint32_t(pos) >= textFrag->GetLength()) {
              MOZ_ASSERT_UNREACHABLE("looking beyond end of text fragment");
              continue;
            }
            char16_t theChar = textFrag->CharAt(pos);
            if (!nsCRT::IsAsciiSpace(theChar)) {
              if (theChar != kNBSP) {
                mStartNode = textNode;
                mStartOffset = pos + 1;
                mStartReason = WSType::NormalText;
                mStartReasonContent = textNode;
                break;
              }
              // as we look backwards update our earliest found nbsp
              mFirstNBSPNode = textNode;
              mFirstNBSPOffset = pos;
              // also keep track of latest nbsp so far
              if (!mLastNBSPNode) {
                mLastNBSPNode = textNode;
                mLastNBSPOffset = pos;
              }
            }
            start.Set(textNode, pos);
          }
        }
      } else {
        // it's a break or a special node, like <img>, that is not a block and
        // not a break but still serves as a terminator to ws runs.
        mStartNode = start.GetContainer();
        mStartOffset = start.Offset();
        if (previousLeafContentOrBlock->IsHTMLElement(nsGkAtoms::br)) {
          mStartReason = WSType::BRElement;
        } else {
          mStartReason = WSType::SpecialContent;
        }
        mStartReasonContent = previousLeafContentOrBlock;
      }
    } else {
      // no prior node means we exhausted
      // editableBlockParentOrTopmotEditableInlineContent
      mStartNode = start.GetContainer();
      mStartOffset = start.Offset();
      mStartReason = WSType::CurrentBlockBoundary;
      // mStartReasonContent can be either a block element or any non-editable
      // content in this case.
      mStartReasonContent = editableBlockParentOrTopmotEditableInlineContent;
    }
  }

  // then look ahead to find following ws nodes
  if (Text* textNode = end.GetContainerAsText()) {
    // don't need to put it on list. it already is from code above
    const nsTextFragment* textFrag = &textNode->TextFragment();
    if (!end.IsEndOfContainer()) {
      for (uint32_t i = end.Offset(); i < textNode->TextLength(); i++) {
        // sanity bounds check the char position.  bug 136165
        if (i >= textFrag->GetLength()) {
          MOZ_ASSERT_UNREACHABLE("looking beyond end of text fragment");
          continue;
        }
        char16_t theChar = textFrag->CharAt(i);
        if (!nsCRT::IsAsciiSpace(theChar)) {
          if (theChar != kNBSP) {
            mEndNode = textNode;
            mEndOffset = i;
            mEndReason = WSType::NormalText;
            mEndReasonContent = textNode;
            break;
          }
          // as we look forwards update our latest found nbsp
          mLastNBSPNode = textNode;
          mLastNBSPOffset = i;
          // also keep track of earliest nbsp so far
          if (!mFirstNBSPNode) {
            mFirstNBSPNode = textNode;
            mFirstNBSPOffset = i;
          }
        }
        end.Set(textNode, i + 1);
      }
    }
  }

  while (!mEndNode) {
    // we haven't found the end of ws yet.  Keep looking
    nsIContent* nextLeafContentOrBlock =
        HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
            end, *editableBlockParentOrTopmotEditableInlineContent,
            mEditingHost);
    if (nextLeafContentOrBlock) {
      if (HTMLEditUtils::IsBlockElement(*nextLeafContentOrBlock)) {
        // we encountered a new block.  therefore no more ws.
        mEndNode = end.GetContainer();
        mEndOffset = end.Offset();
        mEndReason = WSType::OtherBlockBoundary;
        mEndReasonContent = nextLeafContentOrBlock;
      } else if (nextLeafContentOrBlock->IsText() &&
                 nextLeafContentOrBlock->IsEditable()) {
        RefPtr<Text> textNode = nextLeafContentOrBlock->AsText();
        const nsTextFragment* textFrag = &textNode->TextFragment();
        uint32_t len = textNode->TextLength();

        if (len < 1) {
          // Zero length text node. Set end point to it
          // so we can get past it!
          end.Set(textNode, 0);
        } else {
          for (uint32_t pos = 0; pos < len; pos++) {
            // sanity bounds check the char position.  bug 136165
            if (pos >= textFrag->GetLength()) {
              MOZ_ASSERT_UNREACHABLE("looking beyond end of text fragment");
              continue;
            }
            char16_t theChar = textFrag->CharAt(pos);
            if (!nsCRT::IsAsciiSpace(theChar)) {
              if (theChar != kNBSP) {
                mEndNode = textNode;
                mEndOffset = pos;
                mEndReason = WSType::NormalText;
                mEndReasonContent = textNode;
                break;
              }
              // as we look forwards update our latest found nbsp
              mLastNBSPNode = textNode;
              mLastNBSPOffset = pos;
              // also keep track of earliest nbsp so far
              if (!mFirstNBSPNode) {
                mFirstNBSPNode = textNode;
                mFirstNBSPOffset = pos;
              }
            }
            end.Set(textNode, pos + 1);
          }
        }
      } else {
        // we encountered a break or a special node, like <img>,
        // that is not a block and not a break but still
        // serves as a terminator to ws runs.
        mEndNode = end.GetContainer();
        mEndOffset = end.Offset();
        if (nextLeafContentOrBlock->IsHTMLElement(nsGkAtoms::br)) {
          mEndReason = WSType::BRElement;
        } else {
          mEndReason = WSType::SpecialContent;
        }
        mEndReasonContent = nextLeafContentOrBlock;
      }
    } else {
      // no next node means we exhausted
      // editableBlockParentOrTopmotEditableInlineContent
      mEndNode = end.GetContainer();
      mEndOffset = end.Offset();
      mEndReason = WSType::CurrentBlockBoundary;
      // mEndReasonContent can be either a block element or any non-editable
      // content in this case.
      mEndReasonContent = editableBlockParentOrTopmotEditableInlineContent;
    }
  }

  return NS_OK;
}

void WSRunScanner::GetRuns() {
  ClearRuns();

  // Handle preformatted case first since it's simple.  Note that if end of
  // the scan range isn't in preformatted element, we need to check only the
  // style at mScanStartPoint since the range would be replaced and the start
  // style will be applied to all new string.
  mPRE =
      mScanStartPoint.IsInContentNode() &&
      EditorUtils::IsContentPreformatted(*mScanStartPoint.ContainerAsContent());
  // if it's preformatedd, or if we are surrounded by text or special, it's all
  // one big normal ws run
  if (mPRE ||
      ((StartsFromNormalText() || StartsFromSpecialContent()) &&
       (EndsByNormalText() || EndsBySpecialContent() || EndsByBRElement()))) {
    InitializeWithSingleFragment(WSFragment::Visible::Yes,
                                 WSFragment::StartOfHardLine::No,
                                 WSFragment::EndOfHardLine::No);
    return;
  }

  // if we are before or after a block (or after a break), and there are no
  // nbsp's, then it's all non-rendering ws.
  if (!mFirstNBSPNode && !mLastNBSPNode &&
      (StartsFromHardLineBreak() || EndsByBlockBoundary())) {
    InitializeWithSingleFragment(
        WSFragment::Visible::No,
        StartsFromHardLineBreak() ? WSFragment::StartOfHardLine::Yes
                                  : WSFragment::StartOfHardLine::No,
        EndsByBlockBoundary() ? WSFragment::EndOfHardLine::Yes
                              : WSFragment::EndOfHardLine::No);
    return;
  }

  // otherwise a little trickier.  shucks.
  mStartRun = new WSFragment();
  mStartRun->mStartNode = mStartNode;
  mStartRun->mStartOffset = mStartOffset;

  if (StartsFromHardLineBreak()) {
    // set up mStartRun
    mStartRun->MarkAsStartOfHardLine();
    mStartRun->mEndNode = mFirstNBSPNode;
    mStartRun->mEndOffset = mFirstNBSPOffset;
    mStartRun->SetStartFrom(mStartReason);
    mStartRun->SetEndByNormalWiteSpaces();

    // set up next run
    WSFragment* normalRun = new WSFragment();
    mStartRun->mRight = normalRun;
    normalRun->MarkAsVisible();
    normalRun->mStartNode = mFirstNBSPNode;
    normalRun->mStartOffset = mFirstNBSPOffset;
    normalRun->SetStartFromLeadingWhiteSpaces();
    normalRun->mLeft = mStartRun;
    if (!EndsByBlockBoundary()) {
      // then no trailing ws.  this normal run ends the overall ws run.
      normalRun->SetEndBy(mEndReason);
      normalRun->mEndNode = mEndNode;
      normalRun->mEndOffset = mEndOffset;
      mEndRun = normalRun;
    } else {
      // we might have trailing ws.
      // it so happens that *if* there is an nbsp at end,
      // {mEndNode,mEndOffset-1} will point to it, even though in general
      // start/end points not guaranteed to be in text nodes.
      if (mLastNBSPNode == mEndNode && mLastNBSPOffset == mEndOffset - 1) {
        // normal ws runs right up to adjacent block (nbsp next to block)
        normalRun->SetEndBy(mEndReason);
        normalRun->mEndNode = mEndNode;
        normalRun->mEndOffset = mEndOffset;
        mEndRun = normalRun;
      } else {
        normalRun->mEndNode = mLastNBSPNode;
        normalRun->mEndOffset = mLastNBSPOffset + 1;
        normalRun->SetEndByTrailingWhiteSpaces();

        // set up next run
        WSFragment* lastRun = new WSFragment();
        lastRun->MarkAsEndOfHardLine();
        lastRun->mStartNode = mLastNBSPNode;
        lastRun->mStartOffset = mLastNBSPOffset + 1;
        lastRun->mEndNode = mEndNode;
        lastRun->mEndOffset = mEndOffset;
        lastRun->SetStartFromNormalWhiteSpaces();
        lastRun->mLeft = normalRun;
        lastRun->SetEndBy(mEndReason);
        mEndRun = lastRun;
        normalRun->mRight = lastRun;
      }
    }
  } else {
    MOZ_ASSERT(!StartsFromHardLineBreak());
    mStartRun->MarkAsVisible();
    mStartRun->mEndNode = mLastNBSPNode;
    mStartRun->mEndOffset = mLastNBSPOffset + 1;
    mStartRun->SetStartFrom(mStartReason);

    // we might have trailing ws.
    // it so happens that *if* there is an nbsp at end, {mEndNode,mEndOffset-1}
    // will point to it, even though in general start/end points not
    // guaranteed to be in text nodes.
    if (mLastNBSPNode == mEndNode && mLastNBSPOffset == (mEndOffset - 1)) {
      mStartRun->SetEndBy(mEndReason);
      mStartRun->mEndNode = mEndNode;
      mStartRun->mEndOffset = mEndOffset;
      mEndRun = mStartRun;
    } else {
      // set up next run
      WSFragment* lastRun = new WSFragment();
      lastRun->MarkAsEndOfHardLine();
      lastRun->mStartNode = mLastNBSPNode;
      lastRun->mStartOffset = mLastNBSPOffset + 1;
      lastRun->SetStartFromNormalWhiteSpaces();
      lastRun->mLeft = mStartRun;
      lastRun->SetEndBy(mEndReason);
      mEndRun = lastRun;
      mStartRun->mRight = lastRun;
      mStartRun->SetEndByTrailingWhiteSpaces();
    }
  }
}

void WSRunScanner::ClearRuns() {
  WSFragment *tmp, *run;
  run = mStartRun;
  while (run) {
    tmp = run->mRight;
    delete run;
    run = tmp;
  }
  mStartRun = 0;
  mEndRun = 0;
}

void WSRunScanner::InitializeWithSingleFragment(
    WSFragment::Visible aIsVisible,
    WSFragment::StartOfHardLine aIsStartOfHardLine,
    WSFragment::EndOfHardLine aIsEndOfHardLine) {
  MOZ_ASSERT(!mStartRun);
  MOZ_ASSERT(!mEndRun);

  mStartRun = new WSFragment();

  mStartRun->mStartNode = mStartNode;
  mStartRun->mStartOffset = mStartOffset;
  if (aIsVisible == WSFragment::Visible::Yes) {
    mStartRun->MarkAsVisible();
  }
  if (aIsStartOfHardLine == WSFragment::StartOfHardLine::Yes) {
    mStartRun->MarkAsStartOfHardLine();
  }
  if (aIsEndOfHardLine == WSFragment::EndOfHardLine::Yes) {
    mStartRun->MarkAsEndOfHardLine();
  }
  mStartRun->mEndNode = mEndNode;
  mStartRun->mEndOffset = mEndOffset;
  mStartRun->SetStartFrom(mStartReason);
  mStartRun->SetEndBy(mEndReason);

  mEndRun = mStartRun;
}

nsresult WSRunObject::PrepareToDeleteRangePriv(WSRunObject* aEndObject) {
  // this routine adjust whitespace before *this* and after aEndObject
  // in preperation for the two areas to become adjacent after the
  // intervening content is deleted.  It's overly agressive right
  // now.  There might be a block boundary remaining between them after
  // the deletion, in which case these adjstments are unneeded (though
  // I don't think they can ever be harmful?)

  if (NS_WARN_IF(!aEndObject)) {
    return NS_ERROR_INVALID_ARG;
  }

  // get the runs before and after selection
  WSFragment* beforeRun = FindNearestRun(mScanStartPoint, false);
  WSFragment* afterRun =
      aEndObject->FindNearestRun(aEndObject->mScanStartPoint, true);

  if (!beforeRun && !afterRun) {
    return NS_OK;
  }

  if (afterRun) {
    // trim after run of any leading ws
    if (afterRun->IsStartOfHardLine()) {
      // mScanStartPoint will be referred bellow so that we need to keep
      // it a valid point.
      AutoEditorDOMPointChildInvalidator forgetChild(mScanStartPoint);
      nsresult rv = MOZ_KnownLive(mHTMLEditor)
                        .DeleteTextAndTextNodesWithTransaction(
                            aEndObject->mScanStartPoint, afterRun->EndPoint());
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
        return rv;
      }
    }
    // adjust normal ws in afterRun if needed
    else if (afterRun->IsVisibleAndMiddleOfHardLine() && !aEndObject->mPRE) {
      if ((beforeRun && beforeRun->IsStartOfHardLine()) ||
          (!beforeRun && StartsFromHardLineBreak())) {
        // make sure leading char of following ws is an nbsp, so that it will
        // show up
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
                GetFirstASCIIWhitespacePointCollapsedTo(nextCharOfStartOfEnd);
          }
          EditorDOMPointInText endOfCollapsibleASCIIWhitespaces =
              GetEndOfCollapsibleASCIIWhitespaces(nextCharOfStartOfEnd);
          nsresult rv = aEndObject->ReplaceASCIIWhitespacesWithOneNBSP(
              nextCharOfStartOfEnd, endOfCollapsibleASCIIWhitespaces);
          if (NS_FAILED(rv)) {
            NS_WARNING(
                "WSRunObject::ReplaceASCIIWhitespacesWithOneNBSP() failed");
            return rv;
          }
        }
      }
    }
  }

  if (!beforeRun) {
    return NS_OK;
  }

  // trim before run of any trailing ws
  if (beforeRun->IsEndOfHardLine()) {
    nsresult rv = MOZ_KnownLive(mHTMLEditor)
                      .DeleteTextAndTextNodesWithTransaction(
                          beforeRun->StartPoint(), mScanStartPoint);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  if (beforeRun->IsVisibleAndMiddleOfHardLine() && !mPRE) {
    if ((afterRun && (afterRun->IsEndOfHardLine() || afterRun->IsVisible())) ||
        (!afterRun && aEndObject->EndsByBlockBoundary())) {
      // make sure trailing char of starting ws is an nbsp, so that it will show
      // up
      EditorDOMPointInText atPreviousCharOfStart =
          GetPreviousEditableCharPoint(mScanStartPoint);
      if (atPreviousCharOfStart.IsSet() &&
          !atPreviousCharOfStart.IsEndOfContainer() &&
          atPreviousCharOfStart.IsCharASCIISpace()) {
        if (atPreviousCharOfStart.IsStartOfContainer() ||
            atPreviousCharOfStart.IsPreviousCharASCIISpace()) {
          atPreviousCharOfStart =
              GetFirstASCIIWhitespacePointCollapsedTo(atPreviousCharOfStart);
        }
        EditorDOMPointInText endOfCollapsibleASCIIWhitespaces =
            GetEndOfCollapsibleASCIIWhitespaces(atPreviousCharOfStart);
        nsresult rv = ReplaceASCIIWhitespacesWithOneNBSP(
            atPreviousCharOfStart, endOfCollapsibleASCIIWhitespaces);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "WSRunObject::ReplaceASCIIWhitespacesWithOneNBSP() failed");
          return rv;
        }
      }
    }
  }
  return NS_OK;
}

nsresult WSRunObject::PrepareToSplitAcrossBlocksPriv() {
  // used to prepare ws to be split across two blocks.  The main issue
  // here is make sure normalWS doesn't end up becoming non-significant
  // leading or trailing ws after the split.

  // get the runs before and after selection
  WSFragment* beforeRun = FindNearestRun(mScanStartPoint, false);
  WSFragment* afterRun = FindNearestRun(mScanStartPoint, true);

  // adjust normal ws in afterRun if needed
  if (afterRun && afterRun->IsVisibleAndMiddleOfHardLine()) {
    // make sure leading char of following ws is an nbsp, so that it will show
    // up
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
            GetFirstASCIIWhitespacePointCollapsedTo(atNextCharOfStart);
      }
      EditorDOMPointInText endOfCollapsibleASCIIWhitespaces =
          GetEndOfCollapsibleASCIIWhitespaces(atNextCharOfStart);
      nsresult rv = ReplaceASCIIWhitespacesWithOneNBSP(
          atNextCharOfStart, endOfCollapsibleASCIIWhitespaces);
      if (NS_FAILED(rv)) {
        NS_WARNING("WSRunObject::ReplaceASCIIWhitespacesWithOneNBSP() failed");
        return rv;
      }
    }
  }

  // adjust normal ws in beforeRun if needed
  if (beforeRun && beforeRun->IsVisibleAndMiddleOfHardLine()) {
    // make sure trailing char of starting ws is an nbsp, so that it will show
    // up
    EditorDOMPointInText atPreviousCharOfStart =
        GetPreviousEditableCharPoint(mScanStartPoint);
    if (atPreviousCharOfStart.IsSet() &&
        !atPreviousCharOfStart.IsEndOfContainer() &&
        atPreviousCharOfStart.IsCharASCIISpace()) {
      if (atPreviousCharOfStart.IsStartOfContainer() ||
          atPreviousCharOfStart.IsPreviousCharASCIISpace()) {
        atPreviousCharOfStart =
            GetFirstASCIIWhitespacePointCollapsedTo(atPreviousCharOfStart);
      }
      EditorDOMPointInText endOfCollapsibleASCIIWhitespaces =
          GetEndOfCollapsibleASCIIWhitespaces(atPreviousCharOfStart);
      nsresult rv = ReplaceASCIIWhitespacesWithOneNBSP(
          atPreviousCharOfStart, endOfCollapsibleASCIIWhitespaces);
      if (NS_FAILED(rv)) {
        NS_WARNING("WSRunObject::ReplaceASCIIWhitespacesWithOneNBSP() failed");
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

  if (point.GetContainer() == mEndReasonContent) {
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
      if (nextContent == mEndReasonContent) {
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

  if (point.GetContainer() == mStartReasonContent) {
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
      if (previousContent == mStartReasonContent) {
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

EditorDOMPointInText WSRunScanner::GetEndOfCollapsibleASCIIWhitespaces(
    const EditorDOMPointInText& aPointAtASCIIWhitespace) const {
  MOZ_ASSERT(aPointAtASCIIWhitespace.IsSet());
  MOZ_ASSERT(!aPointAtASCIIWhitespace.IsEndOfContainer());
  MOZ_ASSERT(aPointAtASCIIWhitespace.IsCharASCIISpace());

  // If it's not the last character in the text node, let's scan following
  // characters in it.
  if (!aPointAtASCIIWhitespace.IsAtLastContent()) {
    Maybe<uint32_t> nextVisibleCharOffset =
        HTMLEditUtils::GetNextCharOffsetExceptASCIIWhitespaces(
            aPointAtASCIIWhitespace);
    if (nextVisibleCharOffset.isSome()) {
      // There is non-whitespace character in it.
      return EditorDOMPointInText(aPointAtASCIIWhitespace.ContainerAsText(),
                                  nextVisibleCharOffset.value());
    }
  }

  // Otherwise, i.e., the text node ends with ASCII whitespace, keep scanning
  // the following text nodes.
  // XXX Perhaps, we should stop scanning if there is non-editable and visible
  //     content.
  for (EditorDOMPointInText atEndOfPreviousTextNode =
           EditorDOMPointInText::AtEndOf(
               *aPointAtASCIIWhitespace.ContainerAsText());
       ;) {
    EditorDOMPointInText atStartOfNextTextNode =
        GetInclusiveNextEditableCharPoint(atEndOfPreviousTextNode);
    if (!atStartOfNextTextNode.IsSet()) {
      // There is no more text nodes.  Return end of the previous text node.
      return atEndOfPreviousTextNode;
    }

    // We can ignore empty text nodes.
    if (atStartOfNextTextNode.IsContainerEmpty()) {
      atEndOfPreviousTextNode = atStartOfNextTextNode;
      continue;
    }

    // If next node starts with non-whitespace character, return end of
    // previous text node.
    if (!atStartOfNextTextNode.IsCharASCIISpace()) {
      return atEndOfPreviousTextNode;
    }

    // Otherwise, scan the text node.
    Maybe<uint32_t> nextVisibleCharOffset =
        HTMLEditUtils::GetNextCharOffsetExceptASCIIWhitespaces(
            atStartOfNextTextNode);
    if (nextVisibleCharOffset.isSome()) {
      return EditorDOMPointInText(atStartOfNextTextNode.ContainerAsText(),
                                  nextVisibleCharOffset.value());
    }

    // The next text nodes ends with whitespace too.  Try next one.
    atEndOfPreviousTextNode =
        EditorDOMPointInText::AtEndOf(*atStartOfNextTextNode.ContainerAsText());
  }
}

EditorDOMPointInText WSRunScanner::GetFirstASCIIWhitespacePointCollapsedTo(
    const EditorDOMPointInText& aPointAtASCIIWhitespace) const {
  MOZ_ASSERT(aPointAtASCIIWhitespace.IsSet());
  MOZ_ASSERT(!aPointAtASCIIWhitespace.IsEndOfContainer());
  MOZ_ASSERT(aPointAtASCIIWhitespace.IsCharASCIISpace());

  // If there is some characters before it, scan it in the text node first.
  if (!aPointAtASCIIWhitespace.IsStartOfContainer()) {
    uint32_t firstASCIIWhitespaceOffset =
        HTMLEditUtils::GetFirstASCIIWhitespaceOffsetCollapsedWith(
            aPointAtASCIIWhitespace);
    if (firstASCIIWhitespaceOffset) {
      // There is a non-whitespace character in it.
      return EditorDOMPointInText(aPointAtASCIIWhitespace.ContainerAsText(),
                                  firstASCIIWhitespaceOffset);
    }
  }

  // Otherwise, i.e., the text node starts with ASCII whitespace, keep scanning
  // the preceding text nodes.
  // XXX Perhaps, we should stop scanning if there is non-editable and visible
  //     content.
  for (EditorDOMPointInText atStartOfPreviousTextNode =
           EditorDOMPointInText(aPointAtASCIIWhitespace.ContainerAsText(), 0);
       ;) {
    EditorDOMPointInText atLastCharOfNextTextNode =
        GetPreviousEditableCharPoint(atStartOfPreviousTextNode);
    if (!atLastCharOfNextTextNode.IsSet()) {
      // There is no more text nodes.  Return end of last text node.
      return atStartOfPreviousTextNode;
    }

    // We can ignore empty text nodes.
    if (atLastCharOfNextTextNode.IsContainerEmpty()) {
      atStartOfPreviousTextNode = atLastCharOfNextTextNode;
      continue;
    }

    // If next node ends with non-whitespace character, return start of
    // previous text node.
    if (!atLastCharOfNextTextNode.IsCharASCIISpace()) {
      return atStartOfPreviousTextNode;
    }

    // Otherwise, scan the text node.
    uint32_t firstASCIIWhitespaceOffset =
        HTMLEditUtils::GetFirstASCIIWhitespaceOffsetCollapsedWith(
            atLastCharOfNextTextNode);
    if (firstASCIIWhitespaceOffset) {
      return EditorDOMPointInText(atLastCharOfNextTextNode.ContainerAsText(),
                                  firstASCIIWhitespaceOffset);
    }

    // The next text nodes starts with whitespace too.  Try next one.
    atStartOfPreviousTextNode =
        EditorDOMPointInText(atLastCharOfNextTextNode.ContainerAsText(), 0);
  }
}

nsresult WSRunObject::ReplaceASCIIWhitespacesWithOneNBSP(
    const EditorDOMPointInText& aAtFirstASCIIWhitespace,
    const EditorDOMPointInText& aEndOfCollapsibleASCIIWhitespaces) {
  MOZ_ASSERT(aAtFirstASCIIWhitespace.IsSetAndValid());
  MOZ_ASSERT(!aAtFirstASCIIWhitespace.IsEndOfContainer());
  MOZ_ASSERT(aAtFirstASCIIWhitespace.IsCharASCIISpace());
  MOZ_ASSERT(aEndOfCollapsibleASCIIWhitespaces.IsSetAndValid());
  MOZ_ASSERT(aEndOfCollapsibleASCIIWhitespaces.IsEndOfContainer() ||
             !aEndOfCollapsibleASCIIWhitespaces.IsCharASCIISpace());

  AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
  nsresult rv =
      MOZ_KnownLive(mHTMLEditor)
          .ReplaceTextWithTransaction(
              MOZ_KnownLive(*aAtFirstASCIIWhitespace.ContainerAsText()),
              aAtFirstASCIIWhitespace.Offset(),
              aAtFirstASCIIWhitespace.ContainerAsText() ==
                      aEndOfCollapsibleASCIIWhitespaces.ContainerAsText()
                  ? aEndOfCollapsibleASCIIWhitespaces.Offset() -
                        aAtFirstASCIIWhitespace.Offset()
                  : aAtFirstASCIIWhitespace.ContainerAsText()->TextLength() -
                        aAtFirstASCIIWhitespace.Offset(),
              nsDependentSubstring(&kNBSP, 1));
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
    return rv;
  }

  if (aAtFirstASCIIWhitespace.GetContainer() ==
      aEndOfCollapsibleASCIIWhitespaces.GetContainer()) {
    return NS_OK;
  }

  // We need to remove the following unnecessary ASCII whitespaces because we
  // collapsed them into the start node.
  rv = MOZ_KnownLive(mHTMLEditor)
           .DeleteTextAndTextNodesWithTransaction(
               EditorDOMPointInText::AtEndOf(
                   *aAtFirstASCIIWhitespace.ContainerAsText()),
               aEndOfCollapsibleASCIIWhitespaces);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
  return rv;
}

template <typename PT, typename CT>
WSRunScanner::WSFragment* WSRunScanner::FindNearestRun(
    const EditorDOMPointBase<PT, CT>& aPoint, bool aForward) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  for (WSFragment* run = mStartRun; run; run = run->mRight) {
    int32_t comp = run->mStartNode ? *nsContentUtils::ComparePoints(
                                         aPoint.ToRawRangeBoundary(),
                                         run->StartPoint().ToRawRangeBoundary())
                                   : -1;
    if (comp <= 0) {
      // aPoint equals or before start of the run.  Return the run if we're
      // scanning forward, otherwise, nullptr.
      return aForward ? run : nullptr;
    }

    comp = run->mEndNode ? *nsContentUtils::ComparePoints(
                               aPoint.ToRawRangeBoundary(),
                               run->EndPoint().ToRawRangeBoundary())
                         : -1;
    if (comp < 0) {
      // If aPoint is in the run, return the run.
      return run;
    }

    if (!comp) {
      // If aPoint is at end of the run, return next run if we're scanning
      // forward, otherwise, return the run.
      return aForward ? run->mRight : run;
    }

    if (!run->mRight) {
      // If the run is the last run and aPoint is after end of the last run,
      // return nullptr if we're scanning forward, otherwise, return this
      // last run.
      return aForward ? nullptr : run;
    }
  }

  return nullptr;
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

nsresult WSRunObject::NormalizeWhitespacesAtEndOf(const WSFragment& aRun) {
  // Check if it's a visible fragment in a hard line.
  if (!aRun.IsVisibleAndMiddleOfHardLine()) {
    return NS_ERROR_FAILURE;
  }

  // first check for trailing nbsp
  EditorDOMPoint atEndOfRun = aRun.EndPoint();
  EditorDOMPointInText atPreviousCharOfEndOfRun =
      GetPreviousEditableCharPoint(atEndOfRun);
  if (!atPreviousCharOfEndOfRun.IsSet() ||
      atPreviousCharOfEndOfRun.IsEndOfContainer() ||
      !atPreviousCharOfEndOfRun.IsCharNBSP()) {
    return NS_OK;
  }

  // now check that what is to the left of it is compatible with replacing
  // nbsp with space
  EditorDOMPointInText atPreviousCharOfPreviousCharOfEndOfRun =
      GetPreviousEditableCharPoint(atPreviousCharOfEndOfRun);
  bool isPreviousCharASCIIWhitespace =
      atPreviousCharOfPreviousCharOfEndOfRun.IsSet() &&
      !atPreviousCharOfPreviousCharOfEndOfRun.IsEndOfContainer() &&
      atPreviousCharOfPreviousCharOfEndOfRun.IsCharASCIISpace();
  bool maybeNBSPFollowingVisibleContent =
      (atPreviousCharOfPreviousCharOfEndOfRun.IsSet() &&
       !isPreviousCharASCIIWhitespace) ||
      (!atPreviousCharOfPreviousCharOfEndOfRun.IsSet() &&
       (aRun.StartsFromNormalText() || aRun.StartsFromSpecialContent()));
  bool followedByVisibleContentOrBRElement = false;

  // If the NBSP follows a visible content or an ASCII whitespace, i.e.,
  // unless NBSP is first character and start of a block, we may need to
  // insert <br> element and restore the NBSP to an ASCII whitespace.
  if (maybeNBSPFollowingVisibleContent || isPreviousCharASCIIWhitespace) {
    followedByVisibleContentOrBRElement = aRun.EndsByNormalText() ||
                                          aRun.EndsBySpecialContent() ||
                                          aRun.EndsByBRElement();
    // First, try to insert <br> element if NBSP is at end of a block.
    // XXX We should stop this if there is a visible content.
    if (aRun.EndsByBlockBoundary() && mScanStartPoint.IsInContentNode()) {
      bool insertBRElement =
          HTMLEditUtils::IsBlockElement(*mScanStartPoint.ContainerAsContent());
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
                .InsertBRElementWithTransaction(atEndOfRun);
        if (NS_WARN_IF(mHTMLEditor.Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (!brElement) {
          NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
          return NS_ERROR_FAILURE;
        }

        atPreviousCharOfEndOfRun = GetPreviousEditableCharPoint(atEndOfRun);
        atPreviousCharOfPreviousCharOfEndOfRun =
            GetPreviousEditableCharPoint(atPreviousCharOfEndOfRun);
        isPreviousCharASCIIWhitespace =
            atPreviousCharOfPreviousCharOfEndOfRun.IsCharASCIISpace();
        followedByVisibleContentOrBRElement = true;
      }
    }

    // Next, replace the NBSP with an ASCII whitespace if it's surrounded
    // by visible contents (or immediately before a <br> element).
    if (maybeNBSPFollowingVisibleContent &&
        followedByVisibleContentOrBRElement) {
      AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
      nsresult rv =
          MOZ_KnownLive(mHTMLEditor)
              .ReplaceTextWithTransaction(
                  MOZ_KnownLive(*atPreviousCharOfEndOfRun.ContainerAsText()),
                  atPreviousCharOfEndOfRun.Offset(), 1, NS_LITERAL_STRING(" "));
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::ReplaceTextWithTransaction() failed");
      return rv;
    }
  }

  // If the text node is not preformatted, and the NBSP is followed by a <br>
  // element and following (maybe multiple) ASCII spaces, remove the NBSP,
  // but inserts a NBSP before the spaces.  This makes a line break opportunity
  // to wrap the line.
  // XXX This is different behavior from Blink.  Blink generates pairs of
  //     an NBSP and an ASCII whitespace, but put NBSP at the end of the
  //     sequence.  We should follow the behavior for web-compat.
  if (mPRE || maybeNBSPFollowingVisibleContent ||
      !isPreviousCharASCIIWhitespace || !followedByVisibleContentOrBRElement) {
    return NS_OK;
  }

  // Currently, we're at an NBSP following an ASCII space, and we need to
  // replace them with `"&nbsp; "` for avoiding collapsing whitespaces.
  MOZ_ASSERT(!atPreviousCharOfPreviousCharOfEndOfRun.IsEndOfContainer());
  EditorDOMPointInText atFirstASCIIWhitespace =
      GetFirstASCIIWhitespacePointCollapsedTo(
          atPreviousCharOfPreviousCharOfEndOfRun);
  AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
  uint32_t numberOfASCIIWhitespacesInStartNode =
      atFirstASCIIWhitespace.ContainerAsText() ==
              atPreviousCharOfEndOfRun.ContainerAsText()
          ? atPreviousCharOfEndOfRun.Offset() - atFirstASCIIWhitespace.Offset()
          : atFirstASCIIWhitespace.ContainerAsText()->Length() -
                atFirstASCIIWhitespace.Offset();
  // Replace all preceding ASCII whitespaces **and** the NBSP.
  uint32_t replaceLengthInStartNode =
      numberOfASCIIWhitespacesInStartNode +
      (atFirstASCIIWhitespace.ContainerAsText() ==
               atPreviousCharOfEndOfRun.ContainerAsText()
           ? 1
           : 0);
  nsresult rv =
      MOZ_KnownLive(mHTMLEditor)
          .ReplaceTextWithTransaction(
              MOZ_KnownLive(*atFirstASCIIWhitespace.ContainerAsText()),
              atFirstASCIIWhitespace.Offset(), replaceLengthInStartNode,
              NS_LITERAL_STRING(u"\x00A0 "));
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
    return rv;
  }

  if (atFirstASCIIWhitespace.GetContainer() ==
      atPreviousCharOfEndOfRun.GetContainer()) {
    return NS_OK;
  }

  // We need to remove the following unnecessary ASCII whitespaces and
  // NBSP at atPreviousCharOfEndOfRun because we collapsed them into
  // the start node.
  rv = MOZ_KnownLive(mHTMLEditor)
           .DeleteTextAndTextNodesWithTransaction(
               EditorDOMPointInText::AtEndOf(
                   *atFirstASCIIWhitespace.ContainerAsText()),
               atPreviousCharOfEndOfRun.NextPoint());
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
  return rv;
}

nsresult WSRunObject::MaybeReplacePreviousNBSPWithASCIIWhitespace(
    const WSFragment& aRun, const EditorDOMPoint& aPoint) {
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
    // an ASCII whitespace, we don't need to replace it with same character.
    if (!atPreviousCharOfPreviousChar.IsEndOfContainer() &&
        atPreviousCharOfPreviousChar.IsCharASCIISpace()) {
      return NS_OK;
    }
  }
  // If previous content of the NBSP is block boundary, we cannot replace the
  // NBSP with an ASCII whitespace to keep it rendered.
  else if (!aRun.StartsFromNormalText() && !aRun.StartsFromSpecialContent()) {
    return NS_OK;
  }

  AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
  nsresult rv = MOZ_KnownLive(mHTMLEditor)
                    .ReplaceTextWithTransaction(
                        MOZ_KnownLive(*atPreviousChar.ContainerAsText()),
                        atPreviousChar.Offset(), 1, NS_LITERAL_STRING(" "));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::ReplaceTextWithTransaction() failed");
  return rv;
}

nsresult WSRunObject::MaybeReplaceInclusiveNextNBSPWithASCIIWhitespace(
    const WSFragment& aRun, const EditorDOMPoint& aPoint) {
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
    // If following character of an NBSP is an ASCII whitespace, we don't
    // need to replace it with same character.
    if (!atNextCharOfNextCharOfNBSP.IsEndOfContainer() &&
        atNextCharOfNextCharOfNBSP.IsCharASCIISpace()) {
      return NS_OK;
    }
  }
  // If the NBSP is last character in the hard line, we don't need to
  // replace it because it's required to render multiple whitespaces.
  else if (!aRun.EndsByNormalText() && !aRun.EndsBySpecialContent() &&
           !aRun.EndsByBRElement()) {
    return NS_OK;
  }

  AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
  nsresult rv = MOZ_KnownLive(mHTMLEditor)
                    .ReplaceTextWithTransaction(
                        MOZ_KnownLive(*atNextChar.ContainerAsText()),
                        atNextChar.Offset(), 1, NS_LITERAL_STRING(" "));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::ReplaceTextWithTransaction() failed");
  return rv;
}

nsresult WSRunObject::Scrub() {
  for (WSFragment* run = mStartRun; run; run = run->mRight) {
    if (run->IsMiddleOfHardLine()) {
      continue;
    }
    nsresult rv = MOZ_KnownLive(mHTMLEditor)
                      .DeleteTextAndTextNodesWithTransaction(run->StartPoint(),
                                                             run->EndPoint());
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

}  // namespace mozilla
