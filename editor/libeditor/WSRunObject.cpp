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
      nsresult rv = DeleteRange(pointToInsert, afterRun->EndPoint());
      if (NS_FAILED(rv)) {
        NS_WARNING("WSRunObject::DeleteRange() failed");
        return nullptr;
      }
    } else if (afterRun->IsVisibleAndMiddleOfHardLine()) {
      // Need to determine if break at front of non-nbsp run.  If so, convert
      // run to nbsp.
      EditorDOMPointInText atNextCharOfInsertionPoint =
          GetNextCharPoint(pointToInsert);
      if (atNextCharOfInsertionPoint.IsSet() &&
          !atNextCharOfInsertionPoint.IsEndOfContainer() &&
          atNextCharOfInsertionPoint.IsCharASCIISpace()) {
        EditorDOMPointInText atPreviousCharOfNextCharOfInsertionPoint =
            GetPreviousCharPointFromPointInText(atNextCharOfInsertionPoint);
        if (!atPreviousCharOfNextCharOfInsertionPoint.IsSet() ||
            atPreviousCharOfNextCharOfInsertionPoint.IsEndOfContainer() ||
            !atPreviousCharOfNextCharOfInsertionPoint.IsCharASCIISpace()) {
          // We are at start of non-nbsps.  Convert to a single nbsp.
          nsresult rv = InsertNBSPAndRemoveFollowingASCIIWhitespaces(
              atNextCharOfInsertionPoint);
          if (NS_FAILED(rv)) {
            NS_WARNING(
                "WSRunObject::InsertNBSPAndRemoveFollowingASCIIWhitespaces() "
                "failed");
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
      nsresult rv = DeleteRange(beforeRun->StartPoint(), pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING("WSRunObject::DeleteRange() failed");
        return nullptr;
      }
    } else if (beforeRun->IsVisibleAndMiddleOfHardLine()) {
      // Try to change an nbsp to a space, just to prevent nbsp proliferation
      nsresult rv = ReplacePreviousNBSPIfUnnecessary(beforeRun, pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING("WSRunObject::ReplacePreviousNBSPIfUnnecessary() failed");
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
      nsresult rv = DeleteRange(pointToInsert, afterRun->EndPoint());
      if (NS_FAILED(rv)) {
        NS_WARNING("WSRunObject::DeleteRange() failed");
        return rv;
      }
    } else if (afterRun->IsVisibleAndMiddleOfHardLine()) {
      // Try to change an nbsp to a space, if possible, just to prevent nbsp
      // proliferation
      nsresult rv = CheckLeadingNBSP(
          afterRun, MOZ_KnownLive(pointToInsert.GetContainer()),
          pointToInsert.Offset());
      if (NS_FAILED(rv)) {
        NS_WARNING("WSRunObject::CheckLeadingNBSP() failed");
        return rv;
      }
    }

    // Handle any changes needed to ws run before inserted text
    if (!beforeRun || beforeRun->IsStartOfHardLine()) {
      // Don't need to do anything.  Just insert text.  ws won't change.
    } else if (beforeRun->IsEndOfHardLine()) {
      // Need to delete the trailing ws that is before insertion point, because
      // it would become significant after text inserted.
      nsresult rv = DeleteRange(beforeRun->StartPoint(), pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING("WSRunObject::DeleteRange() failed");
        return rv;
      }
    } else if (beforeRun->IsVisibleAndMiddleOfHardLine()) {
      // Try to change an nbsp to a space, if possible, just to prevent nbsp
      // proliferation
      nsresult rv = ReplacePreviousNBSPIfUnnecessary(beforeRun, pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING("WSRunObject::ReplacePreviousNBSPIfUnnecessary() failed");
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
            GetPreviousCharPoint(pointToInsert);
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
        EditorDOMPointInText atNextChar = GetNextCharPoint(pointToInsert);
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
      GetPreviousCharPoint(mScanStartPoint);
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
        DeleteRange(atPreviousCharOfStart, atPreviousCharOfStart.NextPoint());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WSRunObject::DeleteRange() failed");
    return rv;
  }

  // Caller's job to ensure that previous char is really ws.  If it is normal
  // ws, we need to delete the whole run.
  if (atPreviousCharOfStart.IsCharASCIISpace()) {
    EditorDOMPointInText start, end;
    Tie(start, end) =
        GetASCIIWhitespacesBounds(eBoth, atPreviousCharOfStart.NextPoint());
    NS_WARNING_ASSERTION(start.IsSet(),
                         "WSRunObject::GetASCIIWhitespacesBounds() didn't "
                         "return start position, but ignored");
    NS_WARNING_ASSERTION(end.IsSet(),
                         "WSRunObject::GetASCIIWhitespacesBounds() didn't "
                         "return end position, but ignored");

    // adjust surrounding ws
    EditorDOMPoint startToDelete(start), endToDelete(end);
    nsresult rv = WSRunObject::PrepareToDeleteRange(
        MOZ_KnownLive(mHTMLEditor), &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING("WSRunObject::PrepareToDeleteRange() failed");
      return rv;
    }

    // finally, delete that ws
    rv = DeleteRange(startToDelete, endToDelete);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WSRunObject::DeleteRange() failed");
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
    rv = DeleteRange(startToDelete, endToDelete);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WSRunObject::DeleteRange() failed");
    return rv;
  }

  return NS_OK;
}

nsresult WSRunObject::DeleteWSForward() {
  EditorDOMPointInText atNextCharOfStart = GetNextCharPoint(mScanStartPoint);
  if (!atNextCharOfStart.IsSet() || atNextCharOfStart.IsEndOfContainer()) {
    return NS_OK;
  }

  // Easy case, preformatted ws.
  if (mPRE) {
    if (!atNextCharOfStart.IsCharASCIISpace() &&
        !atNextCharOfStart.IsCharNBSP()) {
      return NS_OK;
    }
    nsresult rv = DeleteRange(atNextCharOfStart, atNextCharOfStart.NextPoint());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WSRunObject::DeleteRange() failed");
    return rv;
  }

  // Caller's job to ensure that next char is really ws.  If it is normal ws,
  // we need to delete the whole run.
  if (atNextCharOfStart.IsCharASCIISpace()) {
    EditorDOMPointInText start, end;
    Tie(start, end) =
        GetASCIIWhitespacesBounds(eBoth, atNextCharOfStart.NextPoint());
    NS_WARNING_ASSERTION(start.IsSet(),
                         "WSRunObject::GetASCIIWhitespacesBounds() didn't "
                         "return start position, but ignored");
    NS_WARNING_ASSERTION(end.IsSet(),
                         "WSRunObject::GetASCIIWhitespacesBounds() didn't "
                         "return end position, but ignored");
    // Adjust surrounding ws
    EditorDOMPoint startToDelete(start), endToDelete(end);
    nsresult rv = WSRunObject::PrepareToDeleteRange(
        MOZ_KnownLive(mHTMLEditor), &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING("WSRunObject::PrepareToDeleteRange() failed");
      return rv;
    }

    // Finally, delete that ws
    rv = DeleteRange(startToDelete, endToDelete);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WSRunObject::DeleteRange() failed");
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
    rv = DeleteRange(startToDelete, endToDelete);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WSRunObject::DeleteRange() failed");
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
      EditorDOMPointInText atPreviousChar = GetPreviousCharPoint(aPoint);
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
      EditorDOMPointInText atNextChar = GetNextCharPoint(aPoint);
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
    nsresult rv = CheckTrailingNBSPOfRun(run);
    if (NS_FAILED(rv)) {
      NS_WARNING("WSRunObject::CheckTrailingNBSPOfRun() failed");
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
  NS_ASSERTION(mHTMLEditor->IsEditable(aContent),
               "Given content is not editable");
  // XXX What should we do if scan range crosses block boundary?  Currently,
  //     it's not collapsed only when inserting composition string so that
  //     it's possible but shouldn't occur actually.
  nsIContent* editableBlockParentOrTopmotEditableInlineContent = nullptr;
  for (nsIContent* content : InclusiveAncestorsOfType<nsIContent>(*aContent)) {
    if (!mHTMLEditor->IsEditable(content)) {
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
    mNodeArray.InsertElementAt(0, textNode);
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
    nsCOMPtr<nsIContent> priorNode = GetPreviousWSNode(
        start, editableBlockParentOrTopmotEditableInlineContent);
    if (priorNode) {
      if (HTMLEditUtils::IsBlockElement(*priorNode)) {
        mStartNode = start.GetContainer();
        mStartOffset = start.Offset();
        mStartReason = WSType::OtherBlockBoundary;
        mStartReasonContent = priorNode;
      } else if (priorNode->IsText() && priorNode->IsEditable()) {
        RefPtr<Text> textNode = priorNode->AsText();
        mNodeArray.InsertElementAt(0, textNode);
        const nsTextFragment* textFrag = &textNode->TextFragment();
        uint32_t len = textNode->TextLength();

        if (len < 1) {
          // Zero length text node. Set start point to it
          // so we can get past it!
          start.Set(priorNode, 0);
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
        if (priorNode->IsHTMLElement(nsGkAtoms::br)) {
          mStartReason = WSType::BRElement;
        } else {
          mStartReason = WSType::SpecialContent;
        }
        mStartReasonContent = priorNode;
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
    nsCOMPtr<nsIContent> nextNode =
        GetNextWSNode(end, editableBlockParentOrTopmotEditableInlineContent);
    if (nextNode) {
      if (HTMLEditUtils::IsBlockElement(*nextNode)) {
        // we encountered a new block.  therefore no more ws.
        mEndNode = end.GetContainer();
        mEndOffset = end.Offset();
        mEndReason = WSType::OtherBlockBoundary;
        mEndReasonContent = nextNode;
      } else if (nextNode->IsText() && nextNode->IsEditable()) {
        RefPtr<Text> textNode = nextNode->AsText();
        mNodeArray.AppendElement(textNode);
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
        if (nextNode->IsHTMLElement(nsGkAtoms::br)) {
          mEndReason = WSType::BRElement;
        } else {
          mEndReason = WSType::SpecialContent;
        }
        mEndReasonContent = nextNode;
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
  mPRE = EditorBase::IsPreformatted(mScanStartPoint.GetContainer());
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

nsIContent* WSRunScanner::GetPreviousWSNodeInner(nsINode* aStartNode,
                                                 nsINode* aBlockParent) const {
  // Can't really recycle various getnext/prior routines because we have
  // special needs here.  Need to step into inline containers but not block
  // containers.
  MOZ_ASSERT(aStartNode && aBlockParent);

  if (aStartNode == mEditingHost) {
    NS_WARNING(
        "WSRunScanner::GetPreviousWSNodeInner() was called with editing host");
    return nullptr;
  }

  nsCOMPtr<nsIContent> priorNode = aStartNode->GetPreviousSibling();
  OwningNonNull<nsINode> curNode = *aStartNode;
  while (!priorNode) {
    // We have exhausted nodes in parent of aStartNode.
    nsCOMPtr<nsINode> curParent = curNode->GetParentNode();
    if (!curParent) {
      NS_WARNING("Reached orphan node while climbing up the DOM tree");
      return nullptr;
    }
    if (curParent == aBlockParent) {
      // We have exhausted nodes in the block parent.  The convention here is
      // to return null.
      return nullptr;
    }
    if (curParent == mEditingHost) {
      NS_WARNING("Reached editing host while climbing up the DOM tree");
      return nullptr;
    }
    // We have a parent: look for previous sibling
    priorNode = curParent->GetPreviousSibling();
    curNode = curParent;
  }

  if (!priorNode) {
    return nullptr;
  }

  // We have a prior node.  If it's a block, return it.
  if (HTMLEditUtils::IsBlockElement(*priorNode)) {
    return priorNode;
  }
  if (mHTMLEditor->IsContainer(priorNode)) {
    // Else if it's a container, get deep rightmost child
    nsCOMPtr<nsIContent> child = mHTMLEditor->GetRightmostChild(priorNode);
    if (child) {
      return child;
    }
  }
  // Else return the node itself
  return priorNode;
}

nsIContent* WSRunScanner::GetPreviousWSNode(const EditorDOMPoint& aPoint,
                                            nsINode* aBlockParent) const {
  // Can't really recycle various getnext/prior routines because we
  // have special needs here.  Need to step into inline containers but
  // not block containers.
  MOZ_ASSERT(aPoint.IsSet() && aBlockParent);

  if (aPoint.IsInTextNode()) {
    return GetPreviousWSNodeInner(aPoint.GetContainer(), aBlockParent);
  }
  if (!mHTMLEditor->IsContainer(aPoint.GetContainer())) {
    return GetPreviousWSNodeInner(aPoint.GetContainer(), aBlockParent);
  }

  if (!aPoint.Offset()) {
    if (aPoint.GetContainer() == aBlockParent) {
      // We are at start of the block.
      return nullptr;
    }

    // We are at start of non-block container
    return GetPreviousWSNodeInner(aPoint.GetContainer(), aBlockParent);
  }

  if (NS_WARN_IF(!aPoint.IsInContentNode())) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> priorNode = aPoint.GetPreviousSiblingOfChild();
  if (NS_WARN_IF(!priorNode)) {
    return nullptr;
  }

  // We have a prior node.  If it's a block, return it.
  if (HTMLEditUtils::IsBlockElement(*priorNode)) {
    return priorNode;
  }
  if (mHTMLEditor->IsContainer(priorNode)) {
    // Else if it's a container, get deep rightmost child
    nsCOMPtr<nsIContent> child = mHTMLEditor->GetRightmostChild(priorNode);
    if (child) {
      return child;
    }
  }
  // Else return the node itself
  return priorNode;
}

nsIContent* WSRunScanner::GetNextWSNodeInner(nsINode* aStartNode,
                                             nsINode* aBlockParent) const {
  // Can't really recycle various getnext/prior routines because we have
  // special needs here.  Need to step into inline containers but not block
  // containers.
  MOZ_ASSERT(aStartNode && aBlockParent);

  if (aStartNode == mEditingHost) {
    NS_WARNING(
        "WSRunScanner::GetNextWSNodeInner() was called with editing host");
    return nullptr;
  }

  nsCOMPtr<nsIContent> nextNode = aStartNode->GetNextSibling();
  nsCOMPtr<nsINode> curNode = aStartNode;
  while (!nextNode) {
    // We have exhausted nodes in parent of aStartNode.
    nsCOMPtr<nsINode> curParent = curNode->GetParentNode();
    if (!curParent) {
      NS_WARNING("Reached orphan node while climbing up the DOM tree");
      return nullptr;
    }
    if (curParent == aBlockParent) {
      // We have exhausted nodes in the block parent.  The convention here is
      // to return null.
      return nullptr;
    }
    if (curParent == mEditingHost) {
      NS_WARNING("Reached editing host while climbing up the DOM tree");
      return nullptr;
    }
    // We have a parent: look for next sibling
    nextNode = curParent->GetNextSibling();
    curNode = curParent;
  }

  if (!nextNode) {
    return nullptr;
  }

  // We have a next node.  If it's a block, return it.
  if (HTMLEditUtils::IsBlockElement(*nextNode)) {
    return nextNode;
  }
  if (mHTMLEditor->IsContainer(nextNode)) {
    // Else if it's a container, get deep leftmost child
    nsCOMPtr<nsIContent> child = mHTMLEditor->GetLeftmostChild(nextNode);
    if (child) {
      return child;
    }
  }
  // Else return the node itself
  return nextNode;
}

nsIContent* WSRunScanner::GetNextWSNode(const EditorDOMPoint& aPoint,
                                        nsINode* aBlockParent) const {
  // Can't really recycle various getnext/prior routines because we have
  // special needs here.  Need to step into inline containers but not block
  // containers.
  MOZ_ASSERT(aPoint.IsSet() && aBlockParent);

  if (aPoint.IsInTextNode()) {
    return GetNextWSNodeInner(aPoint.GetContainer(), aBlockParent);
  }
  if (!mHTMLEditor->IsContainer(aPoint.GetContainer())) {
    return GetNextWSNodeInner(aPoint.GetContainer(), aBlockParent);
  }

  if (NS_WARN_IF(!aPoint.IsInContentNode())) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> nextNode = aPoint.GetChild();
  if (!nextNode) {
    if (aPoint.GetContainer() == aBlockParent) {
      // We are at end of the block.
      return nullptr;
    }

    // We are at end of non-block container
    return GetNextWSNodeInner(aPoint.GetContainer(), aBlockParent);
  }

  // We have a next node.  If it's a block, return it.
  if (HTMLEditUtils::IsBlockElement(*nextNode)) {
    return nextNode;
  }
  if (mHTMLEditor->IsContainer(nextNode)) {
    // else if it's a container, get deep leftmost child
    nsCOMPtr<nsIContent> child = mHTMLEditor->GetLeftmostChild(nextNode);
    if (child) {
      return child;
    }
  }
  // Else return the node itself
  return nextNode;
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
      nsresult rv = aEndObject->DeleteRange(aEndObject->mScanStartPoint,
                                            afterRun->EndPoint());
      if (NS_FAILED(rv)) {
        NS_WARNING("WSRunObject::DeleteRange() failed");
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
            aEndObject->GetNextCharPoint(aEndObject->mScanStartPoint);
        if (nextCharOfStartOfEnd.IsSet() &&
            !nextCharOfStartOfEnd.IsEndOfContainer() &&
            nextCharOfStartOfEnd.IsCharASCIISpace()) {
          // mScanStartPoint will be referred bellow so that we need to keep
          // it a valid point.
          AutoEditorDOMPointChildInvalidator forgetChild(mScanStartPoint);
          nsresult rv =
              aEndObject->InsertNBSPAndRemoveFollowingASCIIWhitespaces(
                  nextCharOfStartOfEnd);
          if (NS_FAILED(rv)) {
            NS_WARNING(
                "WSRunObject::InsertNBSPAndRemoveFollowingASCIIWhitespaces() "
                "failed");
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
    nsresult rv = DeleteRange(beforeRun->StartPoint(), mScanStartPoint);
    if (NS_FAILED(rv)) {
      NS_WARNING("WSRunObject::DeleteRange() failed");
      return rv;
    }
    return NS_OK;
  }

  if (beforeRun->IsVisibleAndMiddleOfHardLine() && !mPRE) {
    if ((afterRun && (afterRun->IsEndOfHardLine() || afterRun->IsVisible())) ||
        (!afterRun && aEndObject->EndsByBlockBoundary())) {
      // make sure trailing char of starting ws is an nbsp, so that it will show
      // up
      EditorDOMPointInText atPreviousCharOfStart =
          GetPreviousCharPoint(mScanStartPoint);
      if (atPreviousCharOfStart.IsSet() &&
          !atPreviousCharOfStart.IsEndOfContainer() &&
          atPreviousCharOfStart.IsCharASCIISpace()) {
        EditorDOMPointInText start, end;
        Tie(start, end) = GetASCIIWhitespacesBounds(eBoth, mScanStartPoint);
        NS_WARNING_ASSERTION(start.IsSet(),
                             "WSRunObject::GetASCIIWhitespacesBounds() didn't "
                             "return start point, but ignored");
        NS_WARNING_ASSERTION(end.IsSet(),
                             "WSRunObject::GetASCIIWhitespacesBounds() didn't "
                             "return end point, but ignored");
        nsresult rv = InsertNBSPAndRemoveFollowingASCIIWhitespaces(start);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "WSRunObject::InsertNBSPAndRemoveFollowingASCIIWhitespaces() "
              "failed");
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
    EditorDOMPointInText atNextCharOfStart = GetNextCharPoint(mScanStartPoint);
    if (atNextCharOfStart.IsSet() && !atNextCharOfStart.IsEndOfContainer() &&
        atNextCharOfStart.IsCharASCIISpace()) {
      // mScanStartPoint will be referred bellow so that we need to keep
      // it a valid point.
      AutoEditorDOMPointChildInvalidator forgetChild(mScanStartPoint);
      nsresult rv =
          InsertNBSPAndRemoveFollowingASCIIWhitespaces(atNextCharOfStart);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WSRunObject::InsertNBSPAndRemoveFollowingASCIIWhitespaces() "
            "failed");
        return rv;
      }
    }
  }

  // adjust normal ws in beforeRun if needed
  if (beforeRun && beforeRun->IsVisibleAndMiddleOfHardLine()) {
    // make sure trailing char of starting ws is an nbsp, so that it will show
    // up
    EditorDOMPointInText atPreviousCharOfStart =
        GetPreviousCharPoint(mScanStartPoint);
    if (atPreviousCharOfStart.IsSet() &&
        !atPreviousCharOfStart.IsEndOfContainer() &&
        atPreviousCharOfStart.IsCharASCIISpace()) {
      EditorDOMPointInText start, end;
      Tie(start, end) = GetASCIIWhitespacesBounds(eBoth, mScanStartPoint);
      NS_WARNING_ASSERTION(start.IsSet(),
                           "WSRunObject::GetASCIIWhitespacesBounds() didn't "
                           "return start point, but ignored");
      NS_WARNING_ASSERTION(end.IsSet(),
                           "WSRunObject::GetASCIIWhitespacesBounds() didn't "
                           "return end point, but ignored");
      nsresult rv = InsertNBSPAndRemoveFollowingASCIIWhitespaces(start);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WSRunObject::InsertNBSPAndRemoveFollowingASCIIWhitespaces() "
            "failed");
        return rv;
      }
    }
  }
  return NS_OK;
}

nsresult WSRunObject::DeleteRange(const EditorDOMPoint& aStartPoint,
                                  const EditorDOMPoint& aEndPoint) {
  if (NS_WARN_IF(!aStartPoint.IsSet()) || NS_WARN_IF(!aEndPoint.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }
  MOZ_ASSERT(aStartPoint.IsSetAndValid());
  MOZ_ASSERT(aEndPoint.IsSetAndValid());

  // MOOSE: this routine needs to be modified to preserve the integrity of the
  // wsFragment info.

  if (aStartPoint == aEndPoint) {
    // Nothing to delete
    return NS_OK;
  }

  if (aStartPoint.GetContainer() == aEndPoint.GetContainer() &&
      aStartPoint.IsInTextNode()) {
    RefPtr<Text> textNode = aStartPoint.ContainerAsText();
    nsresult rv = MOZ_KnownLive(mHTMLEditor)
                      .DeleteTextWithTransaction(
                          *textNode, aStartPoint.Offset(),
                          aEndPoint.Offset() - aStartPoint.Offset());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::DeleteTextWithTransaction() failed");
    return rv;
  }

  RefPtr<nsRange> range;
  int32_t count = mNodeArray.Length();
  int32_t idx = mNodeArray.IndexOf(aStartPoint.GetContainer());
  if (idx == -1) {
    // If our starting point wasn't one of our ws text nodes, then just go
    // through them from the beginning.
    idx = 0;
  }
  for (; idx < count; idx++) {
    RefPtr<Text> node = mNodeArray[idx];
    if (!node) {
      // We ran out of ws nodes; must have been deleting to end
      return NS_OK;
    }
    if (node == aStartPoint.GetContainer()) {
      if (!aStartPoint.IsEndOfContainer()) {
        nsresult rv = MOZ_KnownLive(mHTMLEditor)
                          .DeleteTextWithTransaction(
                              *node, aStartPoint.Offset(),
                              aStartPoint.GetContainer()->Length() -
                                  aStartPoint.Offset());
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::DeleteTextWithTransaction() failed");
          return rv;
        }
      }
    } else if (node == aEndPoint.GetContainer()) {
      if (!aEndPoint.IsStartOfContainer()) {
        nsresult rv =
            MOZ_KnownLive(mHTMLEditor)
                .DeleteTextWithTransaction(*node, 0, aEndPoint.Offset());
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::DeleteTextWithTransaction() failed");
          return rv;
        }
      }
      break;
    } else {
      if (!range) {
        ErrorResult error;
        range = nsRange::Create(aStartPoint.ToRawRangeBoundary(),
                                aEndPoint.ToRawRangeBoundary(), error);
        if (!range) {
          NS_WARNING("nsRange::Create() failed");
          return error.StealNSResult();
        }
      }
      bool nodeBefore, nodeAfter;
      nsresult rv =
          RangeUtils::CompareNodeToRange(node, range, &nodeBefore, &nodeAfter);
      if (NS_FAILED(rv)) {
        NS_WARNING("RangeUtils::CompareNodeToRange() failed");
        return rv;
      }
      if (nodeAfter) {
        break;
      }
      if (!nodeBefore) {
        nsresult rv =
            MOZ_KnownLive(mHTMLEditor).DeleteNodeWithTransaction(*node);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
          return rv;
        }
        mNodeArray.RemoveElement(node);
        --count;
        --idx;
      }
    }
  }
  return NS_OK;
}

template <typename PT, typename CT>
EditorDOMPointInText WSRunScanner::GetNextCharPoint(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  size_t index = aPoint.IsInTextNode()
                     ? mNodeArray.IndexOf(aPoint.GetContainer())
                     : decltype(mNodeArray)::NoIndex;
  if (index == decltype(mNodeArray)::NoIndex) {
    // Use range comparisons to get next text node which is in mNodeArray.
    return LookForNextCharPointWithinAllTextNodes(aPoint);
  }
  return GetNextCharPointFromPointInText(
      EditorDOMPointInText(mNodeArray[index], aPoint.Offset()));
}

template <typename PT, typename CT>
EditorDOMPointInText WSRunScanner::GetPreviousCharPoint(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  size_t index = aPoint.IsInTextNode()
                     ? mNodeArray.IndexOf(aPoint.GetContainer())
                     : decltype(mNodeArray)::NoIndex;
  if (index == decltype(mNodeArray)::NoIndex) {
    // Use range comparisons to get previous text node which is in mNodeArray.
    return LookForPreviousCharPointWithinAllTextNodes(aPoint);
  }
  return GetPreviousCharPointFromPointInText(
      EditorDOMPointInText(mNodeArray[index], aPoint.Offset()));
}

EditorDOMPointInText WSRunScanner::GetNextCharPointFromPointInText(
    const EditorDOMPointInText& aPoint) const {
  MOZ_ASSERT(aPoint.IsSet());

  size_t index = mNodeArray.IndexOf(aPoint.GetContainer());
  if (index == decltype(mNodeArray)::NoIndex) {
    // Can't find point, but it's not an error
    return EditorDOMPointInText();
  }

  if (aPoint.IsSetAndValid() && !aPoint.IsEndOfContainer()) {
    // XXX This may return empty text node.
    return aPoint;
  }

  if (index + 1 == mNodeArray.Length()) {
    return EditorDOMPointInText();
  }

  // XXX This may return empty text node.
  return EditorDOMPointInText(mNodeArray[index + 1], 0);
}

EditorDOMPointInText WSRunScanner::GetPreviousCharPointFromPointInText(
    const EditorDOMPointInText& aPoint) const {
  MOZ_ASSERT(aPoint.IsSet());

  size_t index = mNodeArray.IndexOf(aPoint.GetContainer());
  if (index == decltype(mNodeArray)::NoIndex) {
    // Can't find point, but it's not an error
    return EditorDOMPointInText();
  }

  if (!aPoint.IsStartOfContainer()) {
    return aPoint.PreviousPoint();
  }

  if (!index) {
    return EditorDOMPointInText();
  }

  // XXX This may return empty text node.
  return EditorDOMPointInText(mNodeArray[index - 1],
                              mNodeArray[index - 1]->TextLength()
                                  ? mNodeArray[index - 1]->TextLength() - 1
                                  : 0);
}

nsresult WSRunObject::InsertNBSPAndRemoveFollowingASCIIWhitespaces(
    const EditorDOMPointInText& aPoint) {
  // MOOSE: this routine needs to be modified to preserve the integrity of the
  // wsFragment info.
  if (NS_WARN_IF(!aPoint.IsSet())) {
    return NS_ERROR_NULL_POINTER;
  }

  // First, insert an NBSP.
  AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
  nsresult rv = MOZ_KnownLive(mHTMLEditor)
                    .InsertTextIntoTextNodeWithTransaction(
                        nsDependentSubstring(&kNBSP, 1), aPoint, true);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InsertTextIntoTextNodeWithTransaction() failed");
    return rv;
  }

  // Now, the text node may have been modified by mutation observer.
  // So, the NBSP may have gone.
  if (!aPoint.IsSetAndValid() || aPoint.IsEndOfContainer() ||
      !aPoint.IsCharNBSP()) {
    // This is just preparation of an edit action.  Let's return NS_OK.
    // XXX Perhaps, we should return another success code which indicates
    //     mutation observer touched the DOM tree.  However, that should
    //     be returned from each transaction's DoTransaction.
    return NS_OK;
  }

  // Next, find range of whitespaces it will be replaced.
  EditorDOMPointInText start, end;
  Tie(start, end) = GetASCIIWhitespacesBounds(eAfter, aPoint.NextPoint());
  if (!start.IsSet()) {
    return NS_OK;
  }
  NS_WARNING_ASSERTION(end.IsSet(),
                       "WSRunObject::GetASCIIWhitespacesBounds() didn't return "
                       "end point, but ignored");

  // Finally, delete that replaced ws, if any
  rv = DeleteRange(start, end);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WSRunObject::DeleteRange() failed");
  return rv;
}

template <typename PT, typename CT>
Tuple<EditorDOMPointInText, EditorDOMPointInText>
WSRunObject::GetASCIIWhitespacesBounds(
    int16_t aDir, const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSet());

  EditorDOMPointInText start, end;

  if (aDir & eAfter) {
    EditorDOMPointInText atNextChar = GetNextCharPoint(aPoint);
    if (atNextChar.IsSet()) {
      // We found a text node, at least.
      start = end = atNextChar;
      // Scan ahead to end of ASCII whitespaces.
      // XXX Looks like that this is too expensive in most cases.  While we
      //     are scanning a text node, we should do it without
      //     GetNextCharPointInText().
      // XXX This loop ends at end of a text node.  Shouldn't we keep looking
      //     next text node?
      for (; atNextChar.IsSet() && !atNextChar.IsEndOfContainer() &&
             atNextChar.IsCharASCIISpace();
           atNextChar = GetNextCharPointFromPointInText(atNextChar)) {
        // End of the range should be after the whitespace.
        end = atNextChar = atNextChar.NextPoint();
      }
    }
  }

  if (aDir & eBefore) {
    EditorDOMPointInText atPreviousChar = GetPreviousCharPoint(aPoint);
    if (atPreviousChar.IsSet()) {
      // We found a text node, at least.
      start = atPreviousChar.NextPoint();
      if (!end.IsSet()) {
        end = start;
      }
      // Scan back to start of ASCII whitespaces.
      // XXX Looks like that this is too expensive in most cases.  While we
      //     are scanning a text node, we should do it without
      //     GetPreviousCharPointFromPointInText().
      // XXX This loop ends at end of a text node.  Shouldn't we keep looking
      //     the text node?
      for (; atPreviousChar.IsSet() && !atPreviousChar.IsEndOfContainer() &&
             atPreviousChar.IsCharASCIISpace();
           atPreviousChar =
               GetPreviousCharPointFromPointInText(atPreviousChar)) {
        start = atPreviousChar;
      }
    }
  }

  return MakeTuple(start, end);
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

template <typename PT, typename CT>
EditorDOMPointInText WSRunScanner::LookForNextCharPointWithinAllTextNodes(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  // Note: only to be called if aPoint.GetContainer() is not a ws node.

  MOZ_ASSERT(aPoint.IsSetAndValid());

  // Binary search on wsnodes
  uint32_t numNodes = mNodeArray.Length();

  if (!numNodes) {
    // Do nothing if there are no nodes to search
    return EditorDOMPointInText();
  }

  // Begin binary search.  We do this because we need to minimize calls to
  // ComparePoints(), which is expensive.
  uint32_t firstNum = 0, curNum = numNodes / 2, lastNum = numNodes;
  while (curNum != lastNum) {
    Text* curNode = mNodeArray[curNum];
    int16_t cmp = *nsContentUtils::ComparePoints(aPoint.ToRawRangeBoundary(),
                                                 RawRangeBoundary(curNode, 0u));
    if (cmp < 0) {
      lastNum = curNum;
    } else {
      firstNum = curNum + 1;
    }
    curNum = (lastNum - firstNum) / 2 + firstNum;
    MOZ_ASSERT(firstNum <= curNum && curNum <= lastNum, "Bad binary search");
  }

  // When the binary search is complete, we always know that the current node
  // is the same as the end node, which is always past our range.  Therefore,
  // we've found the node immediately after the point of interest.
  if (curNum == mNodeArray.Length()) {
    // hey asked for past our range (it's after the last node).
    // GetNextCharPoint() will do the work for us when we pass it the last
    // index of the last node.
    return GetNextCharPointFromPointInText(
        EditorDOMPointInText::AtEndOf(mNodeArray[curNum - 1]));
  }

  // The char after the point is the first character of our range.
  return GetNextCharPointFromPointInText(
      EditorDOMPointInText(mNodeArray[curNum], 0));
}

template <typename PT, typename CT>
EditorDOMPointInText WSRunScanner::LookForPreviousCharPointWithinAllTextNodes(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  // Note: only to be called if aNode is not a ws node.

  MOZ_ASSERT(aPoint.IsSetAndValid());

  // Binary search on wsnodes
  uint32_t numNodes = mNodeArray.Length();

  if (!numNodes) {
    // Do nothing if there are no nodes to search
    return EditorDOMPointInText();
  }

  uint32_t firstNum = 0, curNum = numNodes / 2, lastNum = numNodes;
  int16_t cmp = 0;

  // Begin binary search.  We do this because we need to minimize calls to
  // ComparePoints(), which is expensive.
  while (curNum != lastNum) {
    Text* curNode = mNodeArray[curNum];
    cmp = *nsContentUtils::ComparePoints(aPoint.ToRawRangeBoundary(),
                                         RawRangeBoundary(curNode, 0u));
    if (cmp < 0) {
      lastNum = curNum;
    } else {
      firstNum = curNum + 1;
    }
    curNum = (lastNum - firstNum) / 2 + firstNum;
    MOZ_ASSERT(firstNum <= curNum && curNum <= lastNum, "Bad binary search");
  }

  // When the binary search is complete, we always know that the current node
  // is the same as the end node, which is always past our range. Therefore,
  // we've found the node immediately after the point of interest.
  if (curNum == mNodeArray.Length()) {
    // Get the point before the end of the last node, we can pass the length of
    // the node into GetPreviousCharPoint(), and it will return the last
    // character.
    return GetPreviousCharPointFromPointInText(
        EditorDOMPointInText::AtEndOf(mNodeArray[curNum - 1]));
  }

  // We can just ask the current node for the point immediately before it,
  // it will handle moving to the previous node (if any) and returning the
  // appropriate character
  return GetPreviousCharPointFromPointInText(
      EditorDOMPointInText(mNodeArray[curNum], 0));
}

nsresult WSRunObject::CheckTrailingNBSPOfRun(WSFragment* aRun) {
  if (NS_WARN_IF(!aRun)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Try to change an nbsp to a space, if possible, just to prevent nbsp
  // proliferation.  Examine what is before and after the trailing nbsp, if
  // any.
  bool leftCheck = false;
  bool spaceNBSP = false;
  bool rightCheck = false;

  // Check if it's a visible fragment in a hard line.
  if (!aRun->IsVisibleAndMiddleOfHardLine()) {
    return NS_ERROR_FAILURE;
  }

  // first check for trailing nbsp
  EditorDOMPointInText atPreviousCharOfEndOfRun =
      GetPreviousCharPoint(aRun->EndPoint());
  if (atPreviousCharOfEndOfRun.IsSet() &&
      !atPreviousCharOfEndOfRun.IsEndOfContainer() &&
      atPreviousCharOfEndOfRun.IsCharNBSP()) {
    // now check that what is to the left of it is compatible with replacing
    // nbsp with space
    EditorDOMPointInText atPreviousCharOfPreviousCharOfEndOfRun =
        GetPreviousCharPointFromPointInText(atPreviousCharOfEndOfRun);
    if (atPreviousCharOfPreviousCharOfEndOfRun.IsSet()) {
      if (atPreviousCharOfPreviousCharOfEndOfRun.IsEndOfContainer() ||
          !atPreviousCharOfPreviousCharOfEndOfRun.IsCharASCIISpace()) {
        leftCheck = true;
      } else {
        spaceNBSP = true;
      }
    } else if (aRun->StartsFromNormalText() ||
               aRun->StartsFromSpecialContent()) {
      leftCheck = true;
    }
    if (leftCheck || spaceNBSP) {
      // now check that what is to the right of it is compatible with replacing
      // nbsp with space
      if (aRun->EndsByNormalText() || aRun->EndsBySpecialContent() ||
          aRun->EndsByBRElement()) {
        rightCheck = true;
      }
      if (aRun->EndsByBlockBoundary() && mScanStartPoint.IsInContentNode()) {
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
                  .InsertBRElementWithTransaction(aRun->EndPoint());
          if (!brElement) {
            NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
            return NS_ERROR_FAILURE;
          }

          atPreviousCharOfEndOfRun = GetPreviousCharPoint(aRun->EndPoint());
          atPreviousCharOfPreviousCharOfEndOfRun =
              GetPreviousCharPointFromPointInText(atPreviousCharOfEndOfRun);
          rightCheck = true;
        }
      }
    }
    if (leftCheck && rightCheck) {
      // Now replace nbsp with space.  First, insert a space
      AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
      nsresult rv =
          MOZ_KnownLive(mHTMLEditor)
              .InsertTextIntoTextNodeWithTransaction(
                  NS_LITERAL_STRING(" "), atPreviousCharOfEndOfRun, true);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "EditorBase::InsertTextIntoTextNodeWithTransaction() failed");
        return rv;
      }

      // Finally, delete that nbsp
      NS_ASSERTION(!atPreviousCharOfEndOfRun.IsEndOfContainer() &&
                       !atPreviousCharOfEndOfRun.IsAtLastContent(),
                   "The text node was modified by mutation event listener");
      if (!atPreviousCharOfEndOfRun.IsEndOfContainer() &&
          !atPreviousCharOfEndOfRun.IsAtLastContent()) {
        NS_ASSERTION(atPreviousCharOfEndOfRun.IsNextCharNBSP(),
                     "Trying to remove an NBSP, but it's gone from the "
                     "expected position");
        EditorDOMPointInText atNextCharOfPreviousCharOfEndOfRun =
            atPreviousCharOfEndOfRun.NextPoint();
        nsresult rv =
            DeleteRange(atNextCharOfPreviousCharOfEndOfRun,
                        atNextCharOfPreviousCharOfEndOfRun.NextPoint());
        if (NS_FAILED(rv)) {
          NS_WARNING("WSRunObject::DeleteRange() failed");
          return rv;
        }
      }
    } else if (!mPRE && spaceNBSP && rightCheck) {
      // Don't mess with this preformatted for now.  We have a run of ASCII
      // whitespace (which will render as one space) followed by an nbsp (which
      // is at the end of the whitespace run).  Let's switch their order.  This
      // will ensure that if someone types two spaces after a sentence, and the
      // editor softwraps at this point, the spaces won't be split across lines,
      // which looks ugly and is bad for the moose.
      MOZ_ASSERT(!atPreviousCharOfPreviousCharOfEndOfRun.IsEndOfContainer());
      EditorDOMPointInText start, end;
      // XXX end won't be used, whey `eBoth`?
      Tie(start, end) = GetASCIIWhitespacesBounds(
          eBoth, atPreviousCharOfPreviousCharOfEndOfRun.NextPoint());
      NS_WARNING_ASSERTION(
          start.IsSet(),
          "WSRunObject::GetASCIIWhitespacesBounds() didn't return start point");
      NS_WARNING_ASSERTION(end.IsSet(),
                           "WSRunObject::GetASCIIWhitespacesBounds() didn't "
                           "return end point, but ignored");

      // Delete that nbsp
      NS_ASSERTION(!atPreviousCharOfEndOfRun.IsEndOfContainer(),
                   "The text node was modified by mutation event listener");
      if (!atPreviousCharOfEndOfRun.IsEndOfContainer()) {
        NS_ASSERTION(atPreviousCharOfEndOfRun.IsCharNBSP(),
                     "Trying to remove an NBSP, but it's gone from the "
                     "expected position");
        nsresult rv = DeleteRange(atPreviousCharOfEndOfRun,
                                  atPreviousCharOfEndOfRun.NextPoint());
        if (NS_FAILED(rv)) {
          NS_WARNING("WSRunObject::DeleteRange() failed");
          return rv;
        }
      }

      // Finally, insert that nbsp before the ASCII ws run
      NS_ASSERTION(start.IsSetAndValid(),
                   "The text node was modified by mutation event listener");
      if (start.IsSetAndValid()) {
        AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
        nsresult rv = MOZ_KnownLive(mHTMLEditor)
                          .InsertTextIntoTextNodeWithTransaction(
                              nsDependentSubstring(&kNBSP, 1), start, true);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "EditorBase::InsertTextIntoTextNodeWithTransaction() failed");
          return rv;
        }
      }
    }
  }
  return NS_OK;
}

nsresult WSRunObject::ReplacePreviousNBSPIfUnnecessary(
    WSFragment* aRun, const EditorDOMPoint& aPoint) {
  if (NS_WARN_IF(!aRun) || NS_WARN_IF(!aPoint.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }
  MOZ_ASSERT(aPoint.IsSetAndValid());

  // Try to change an NBSP to a space, if possible, just to prevent NBSP
  // proliferation.  This routine is called when we are about to make this
  // point in the ws abut an inserted break or text, so we don't have to worry
  // about what is after it.  What is after it now will end up after the
  // inserted object.
  bool canConvert = false;
  EditorDOMPointInText atPreviousChar = GetPreviousCharPoint(aPoint);
  if (atPreviousChar.IsSet() && !atPreviousChar.IsEndOfContainer() &&
      atPreviousChar.IsCharNBSP()) {
    EditorDOMPointInText atPreviousCharOfPreviousChar =
        GetPreviousCharPointFromPointInText(atPreviousChar);
    if (atPreviousCharOfPreviousChar.IsSet()) {
      if (atPreviousCharOfPreviousChar.IsEndOfContainer() ||
          !atPreviousCharOfPreviousChar.IsCharASCIISpace()) {
        // If previous character is a NBSP and its previous character isn't
        // ASCII space, we can replace the NBSP with ASCII space.
        canConvert = true;
      }
    } else if (aRun->StartsFromNormalText() ||
               aRun->StartsFromSpecialContent()) {
      // If previous character is a NBSP and it's the first character of the
      // text node, additionally, if its previous node is a text node including
      // non-whitespace characters or <img> node or something inline
      // non-container element node, we can replace the NBSP with ASCII space.
      canConvert = true;
    }
  }

  if (!canConvert) {
    return NS_OK;
  }

  // First, insert a space before the previous NBSP.
  AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
  nsresult rv = MOZ_KnownLive(mHTMLEditor)
                    .InsertTextIntoTextNodeWithTransaction(
                        NS_LITERAL_STRING(" "), atPreviousChar, true);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InsertTextIntoTextNodeWithTransaction() failed");
    return rv;
  }

  // Finally, delete the previous NBSP.
  NS_ASSERTION(
      !atPreviousChar.IsEndOfContainer() && !atPreviousChar.IsAtLastContent(),
      "The text node was modified by mutation event listener");
  if (!atPreviousChar.IsEndOfContainer() && !atPreviousChar.IsAtLastContent()) {
    NS_ASSERTION(
        atPreviousChar.IsNextCharNBSP(),
        "Trying to remove an NBSP, but it's gone from the expected position");
    EditorDOMPointInText atNextCharOfPreviousChar = atPreviousChar.NextPoint();
    nsresult rv = DeleteRange(atNextCharOfPreviousChar,
                              atNextCharOfPreviousChar.NextPoint());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WSRunObject::DeleteRange() failed");
    return rv;
  }

  return NS_OK;
}

nsresult WSRunObject::CheckLeadingNBSP(WSFragment* aRun, nsINode* aNode,
                                       int32_t aOffset) {
  // Try to change an nbsp to a space, if possible, just to prevent nbsp
  // proliferation This routine is called when we are about to make this point
  // in the ws abut an inserted text, so we don't have to worry about what is
  // before it.  What is before it now will end up before the inserted text.
  bool canConvert = false;
  EditorDOMPointInText atNextChar =
      GetNextCharPoint(EditorRawDOMPoint(aNode, aOffset));
  if (!atNextChar.IsSet() || NS_WARN_IF(atNextChar.IsEndOfContainer())) {
    return NS_OK;
  }

  if (atNextChar.IsCharNBSP()) {
    EditorDOMPointInText atNextCharOfNextCharOfNBSP =
        GetNextCharPointFromPointInText(atNextChar.NextPoint());
    if (atNextCharOfNextCharOfNBSP.IsSet()) {
      if (atNextCharOfNextCharOfNBSP.IsEndOfContainer() ||
          !atNextCharOfNextCharOfNBSP.IsCharASCIISpace()) {
        canConvert = true;
      }
    } else if (aRun->EndsByNormalText() || aRun->EndsBySpecialContent() ||
               aRun->EndsByBRElement()) {
      canConvert = true;
    }
  }
  if (canConvert) {
    // First, insert a space
    AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
    nsresult rv = MOZ_KnownLive(mHTMLEditor)
                      .InsertTextIntoTextNodeWithTransaction(
                          NS_LITERAL_STRING(" "), atNextChar, true);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertTextIntoTextNodeWithTransaction() failed");
      return rv;
    }

    // Finally, delete that nbsp
    NS_ASSERTION(
        !atNextChar.IsEndOfContainer() && !atNextChar.IsAtLastContent(),
        "The text node was modified by mutation event listener");
    if (!atNextChar.IsEndOfContainer() && !atNextChar.IsAtLastContent()) {
      NS_ASSERTION(
          atNextChar.IsNextCharNBSP(),
          "Trying to remove an NBSP, but it's gone from the expected position");
      EditorDOMPointInText atNextCharOfNextChar = atNextChar.NextPoint();
      rv = DeleteRange(atNextCharOfNextChar, atNextCharOfNextChar.NextPoint());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "WSRunObject::DeleteRange() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult WSRunObject::Scrub() {
  for (WSFragment* run = mStartRun; run; run = run->mRight) {
    if (run->IsMiddleOfHardLine()) {
      continue;
    }
    nsresult rv = DeleteRange(run->StartPoint(), run->EndPoint());
    if (NS_FAILED(rv)) {
      NS_WARNING("WSRunObject::DeleteRange() failed");
      return rv;
    }
  }
  return NS_OK;
}

}  // namespace mozilla
