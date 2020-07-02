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

  const WSFragment* beforeRun = FindNearestFragment(aPointToInsert, false);
  const WSFragment* afterRun = FindNearestFragment(aPointToInsert, true);

  EditorDOMPoint pointToInsert(aPointToInsert);
  {
    // Some scoping for AutoTrackDOMPoint.  This will track our insertion
    // point while we tweak any surrounding white-space
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
      nsresult rv = MaybeReplacePreviousNBSPWithASCIIWhiteSpace(*beforeRun,
                                                                pointToInsert);
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

  const WSFragment* beforeRun = FindNearestFragment(mScanStartPoint, false);
  // If mScanStartPoint isn't equal to mScanEndPoint, it will replace text (i.e.
  // committing composition). And afterRun will be end point of replaced range.
  // So we want to know this white-space type (trailing white-space etc) of
  // this end point, not inserted (start) point, so we re-scan white-space type.
  WSRunObject afterRunObject(MOZ_KnownLive(mHTMLEditor), mScanEndPoint);
  const WSFragment* afterRun =
      afterRunObject.FindNearestFragment(mScanEndPoint, true);

  EditorDOMPoint pointToInsert(mScanStartPoint);
  nsAutoString theString(aStringToInsert);
  {
    // Some scoping for AutoTrackDOMPoint.  This will track our insertion
    // point while we tweak any surrounding white-space
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
      nsresult rv = MaybeReplaceInclusiveNextNBSPWithASCIIWhiteSpace(
          *afterRun, pointToInsert);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WSRunObject::MaybeReplaceInclusiveNextNBSPWithASCIIWhiteSpace() "
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
      nsresult rv = MaybeReplacePreviousNBSPWithASCIIWhiteSpace(*beforeRun,
                                                                pointToInsert);
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
      // If this text insertion replaces composition, this.mEnd.mReason is
      // start position of composition. So we have to use afterRunObject's
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
  // Find first visible thing before the point.  Position
  // outVisNode/outVisOffset just _after_ that thing.  If we don't find
  // anything return start of ws.
  MOZ_ASSERT(aPoint.IsSet());

  WSFragmentArray::index_type index = FindNearestFragmentIndex(aPoint, false);
  if (index != WSFragmentArray::NoIndex) {
    // Is there a visible run there or earlier?
    for (WSFragmentArray::index_type i = index + 1; i; i--) {
      const WSFragment& fragment = WSFragmentArrayRef()[i - 1];
      if (!fragment.IsVisibleAndMiddleOfHardLine()) {
        continue;
      }
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

  if (mStart.GetReasonContent() != mStart.PointRef().GetContainer()) {
    // In this case, mStart.PointRef().Offset() is not meaningful.
    return WSScanResult(mStart.GetReasonContent(), mStart.RawReason());
  }
  return WSScanResult(mStart.PointRef(), mStart.RawReason());
}

template <typename PT, typename CT>
WSScanResult WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  // Find first visible thing after the point.  Position
  // outVisNode/outVisOffset just _before_ that thing.  If we don't find
  // anything return end of ws.
  MOZ_ASSERT(aPoint.IsSet());

  WSFragmentArray::index_type index = FindNearestFragmentIndex(aPoint, true);
  if (index != WSFragmentArray::NoIndex) {
    // Is there a visible run there or later?
    for (size_t i = index; i < WSFragmentArrayRef().Length(); i++) {
      const WSFragment& fragment = WSFragmentArrayRef()[i];
      if (!fragment.IsVisibleAndMiddleOfHardLine()) {
        continue;
      }
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

  if (mEnd.GetReasonContent() != mEnd.PointRef().GetContainer()) {
    // In this case, mEnd.PointRef().Offset() is not meaningful.
    return WSScanResult(mEnd.GetReasonContent(), mEnd.RawReason());
  }
  return WSScanResult(mEnd.PointRef(), mEnd.RawReason());
}

nsresult WSRunObject::AdjustWhiteSpace() {
  // this routine examines a run of ws and tries to get rid of some unneeded
  // nbsp's, replacing them with regular ascii space if possible.  Keeping
  // things simple for now and just trying to fix up the trailing ws in the run.
  if (!mNBSPData.FoundNBSP()) {
    // nothing to do!
    return NS_OK;
  }
  for (const WSFragment& fragment : WSFragmentArrayRef()) {
    if (!fragment.IsVisibleAndMiddleOfHardLine()) {
      continue;
    }
    nsresult rv = NormalizeWhiteSpacesAtEndOf(fragment);
    if (NS_FAILED(rv)) {
      NS_WARNING("WSRunObject::NormalizeWhiteSpacesAtEndOf() failed");
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

  InitializeRangeStart(mScanStartPoint,
                       *editableBlockParentOrTopmotEditableInlineContent);
  InitializeRangeEnd(mScanStartPoint,
                     *editableBlockParentOrTopmotEditableInlineContent);
  return NS_OK;
}

template <typename EditorDOMPointType>
bool WSRunScanner::InitializeRangeStartWithTextNode(
    const EditorDOMPointType& aPoint) {
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
      mNBSPData.NotifyNBSP(
          EditorDOMPointInText(aPoint.ContainerAsText(), i - 1),
          NoBreakingSpaceData::Scanning::Backward);
      continue;
    }

    mStart = BoundaryData(EditorDOMPoint(aPoint.ContainerAsText(), i),
                          *aPoint.ContainerAsText(), WSType::NormalText);
    return true;
  }

  return false;
}

template <typename EditorDOMPointType>
void WSRunScanner::InitializeRangeStart(
    const EditorDOMPointType& aPoint,
    const nsIContent& aEditableBlockParentOrTopmostEditableInlineContent) {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  // first look backwards to find preceding ws nodes
  if (aPoint.IsInTextNode() && !aPoint.IsStartOfContainer()) {
    if (InitializeRangeStartWithTextNode(aPoint)) {
      return;
    }
    // The text node does not have visible character, let's keep scanning
    // preceding nodes.
    InitializeRangeStart(EditorDOMPoint(aPoint.ContainerAsText(), 0),
                         aEditableBlockParentOrTopmostEditableInlineContent);
    return;
  }

  // we haven't found the start of ws yet.  Keep looking
  nsIContent* previousLeafContentOrBlock =
      HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
          aPoint, aEditableBlockParentOrTopmostEditableInlineContent,
          mEditingHost);
  if (!previousLeafContentOrBlock) {
    // no prior node means we exhausted
    // aEditableBlockParentOrTopmostEditableInlineContent
    // mStart.mReasonContent can be either a block element or any non-editable
    // content in this case.
    mStart =
        BoundaryData(aPoint,
                     const_cast<nsIContent&>(
                         aEditableBlockParentOrTopmostEditableInlineContent),
                     WSType::CurrentBlockBoundary);
    return;
  }

  if (HTMLEditUtils::IsBlockElement(*previousLeafContentOrBlock)) {
    mStart = BoundaryData(aPoint, *previousLeafContentOrBlock,
                          WSType::OtherBlockBoundary);
    return;
  }

  if (!previousLeafContentOrBlock->IsText() ||
      !previousLeafContentOrBlock->IsEditable()) {
    // it's a break or a special node, like <img>, that is not a block and
    // not a break but still serves as a terminator to ws runs.
    mStart =
        BoundaryData(aPoint, *previousLeafContentOrBlock,
                     previousLeafContentOrBlock->IsHTMLElement(nsGkAtoms::br)
                         ? WSType::BRElement
                         : WSType::SpecialContent);
    return;
  }

  if (!previousLeafContentOrBlock->AsText()->TextFragment().GetLength()) {
    // Zero length text node. Set start point to it
    // so we can get past it!
    InitializeRangeStart(
        EditorDOMPointInText(previousLeafContentOrBlock->AsText(), 0),
        aEditableBlockParentOrTopmostEditableInlineContent);
    return;
  }

  if (InitializeRangeStartWithTextNode(EditorDOMPointInText::AtEndOf(
          *previousLeafContentOrBlock->AsText()))) {
    return;
  }

  // The text node does not have visible character, let's keep scanning
  // preceding nodes.
  InitializeRangeStart(
      EditorDOMPointInText(previousLeafContentOrBlock->AsText(), 0),
      aEditableBlockParentOrTopmostEditableInlineContent);
}

template <typename EditorDOMPointType>
bool WSRunScanner::InitializeRangeEndWithTextNode(
    const EditorDOMPointType& aPoint) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_DIAGNOSTIC_ASSERT(aPoint.IsInTextNode());

  const nsTextFragment& textFragment = aPoint.ContainerAsText()->TextFragment();
  for (uint32_t i = aPoint.Offset(); i < textFragment.GetLength(); i++) {
    char16_t ch = textFragment.CharAt(i);
    if (nsCRT::IsAsciiSpace(ch)) {
      continue;
    }

    if (ch == HTMLEditUtils::kNBSP) {
      mNBSPData.NotifyNBSP(EditorDOMPointInText(aPoint.ContainerAsText(), i),
                           NoBreakingSpaceData::Scanning::Forward);
      continue;
    }

    mEnd = BoundaryData(EditorDOMPoint(aPoint.ContainerAsText(), i),
                        *aPoint.ContainerAsText(), WSType::NormalText);
    return true;
  }

  return false;
}

template <typename EditorDOMPointType>
void WSRunScanner::InitializeRangeEnd(
    const EditorDOMPointType& aPoint,
    const nsIContent& aEditableBlockParentOrTopmostEditableInlineContent) {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (aPoint.IsInTextNode() && !aPoint.IsEndOfContainer()) {
    if (InitializeRangeEndWithTextNode(aPoint)) {
      return;
    }
    // The text node does not have visible character, let's keep scanning
    // following nodes.
    InitializeRangeEnd(EditorDOMPointInText::AtEndOf(*aPoint.ContainerAsText()),
                       aEditableBlockParentOrTopmostEditableInlineContent);
    return;
  }

  // we haven't found the end of ws yet.  Keep looking
  nsIContent* nextLeafContentOrBlock =
      HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          aPoint, aEditableBlockParentOrTopmostEditableInlineContent,
          mEditingHost);
  if (!nextLeafContentOrBlock) {
    // no next node means we exhausted
    // aEditableBlockParentOrTopmostEditableInlineContent
    // mEnd.mReasonContent can be either a block element or any non-editable
    // content in this case.
    mEnd = BoundaryData(aPoint,
                        const_cast<nsIContent&>(
                            aEditableBlockParentOrTopmostEditableInlineContent),
                        WSType::CurrentBlockBoundary);
    return;
  }

  if (HTMLEditUtils::IsBlockElement(*nextLeafContentOrBlock)) {
    // we encountered a new block.  therefore no more ws.
    mEnd = BoundaryData(aPoint, *nextLeafContentOrBlock,
                        WSType::OtherBlockBoundary);
    return;
  }

  if (!nextLeafContentOrBlock->IsText() ||
      !nextLeafContentOrBlock->IsEditable()) {
    // we encountered a break or a special node, like <img>,
    // that is not a block and not a break but still
    // serves as a terminator to ws runs.
    mEnd = BoundaryData(aPoint, *nextLeafContentOrBlock,
                        nextLeafContentOrBlock->IsHTMLElement(nsGkAtoms::br)
                            ? WSType::BRElement
                            : WSType::SpecialContent);
    return;
  }

  if (!nextLeafContentOrBlock->AsText()->TextFragment().GetLength()) {
    // Zero length text node. Set end point to it
    // so we can get past it!
    InitializeRangeEnd(
        EditorDOMPointInText(nextLeafContentOrBlock->AsText(), 0),
        aEditableBlockParentOrTopmostEditableInlineContent);
    return;
  }

  if (InitializeRangeEndWithTextNode(
          EditorDOMPointInText(nextLeafContentOrBlock->AsText(), 0))) {
    return;
  }

  // The text node does not have visible character, let's keep scanning
  // following nodes.
  InitializeRangeEnd(
      EditorDOMPointInText::AtEndOf(*nextLeafContentOrBlock->AsText()),
      aEditableBlockParentOrTopmostEditableInlineContent);
}

void WSRunScanner::EnsureWSFragments() {
  if (!mFragments.IsEmpty()) {
    return;
  }

  // Handle preformatted case first since it's simple.  Note that if end of
  // the scan range isn't in preformatted element, we need to check only the
  // style at mScanStartPoint since the range would be replaced and the start
  // style will be applied to all new string.

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
  if (!mNBSPData.FoundNBSP() &&
      (StartsFromHardLineBreak() || EndsByBlockBoundary())) {
    InitializeWithSingleFragment(
        WSFragment::Visible::No,
        StartsFromHardLineBreak() ? WSFragment::StartOfHardLine::Yes
                                  : WSFragment::StartOfHardLine::No,
        EndsByBlockBoundary() ? WSFragment::EndOfHardLine::Yes
                              : WSFragment::EndOfHardLine::No);
    return;
  }

  if (!StartsFromHardLineBreak()) {
    WSFragment* startRun = mFragments.AppendElement();
    startRun->MarkAsVisible();
    if (mStart.PointRef().IsSet()) {
      startRun->mStartNode = mStart.PointRef().GetContainer();
      startRun->mStartOffset = mStart.PointRef().Offset();
    }
    startRun->SetStartFrom(mStart.RawReason());
    if (mNBSPData.LastPointRef().IsSet()) {
      startRun->mEndNode = mNBSPData.LastPointRef().GetContainer();
      startRun->mEndOffset = mNBSPData.LastPointRef().Offset() + 1;
    }

    // we might have trailing ws.
    // it so happens that *if* there is an nbsp at end, {mEndNode,mEndOffset-1}
    // will point to it, even though in general start/end points not
    // guaranteed to be in text nodes.
    if (mNBSPData.LastPointRef().IsSet() && mEnd.PointRef().IsSet() &&
        mNBSPData.LastPointRef().GetContainer() ==
            mEnd.PointRef().GetContainer() &&
        mNBSPData.LastPointRef().Offset() == mEnd.PointRef().Offset() - 1) {
      startRun->SetEndBy(mEnd.RawReason());
      startRun->mEndNode = mEnd.PointRef().GetContainer();
      startRun->mEndOffset = mEnd.PointRef().Offset();
      return;
    }

    // set up next run
    WSFragment* lastRun = mFragments.AppendElement();
    lastRun->MarkAsEndOfHardLine();
    if (mNBSPData.LastPointRef().IsSet()) {
      lastRun->mStartNode = mNBSPData.LastPointRef().GetContainer();
      lastRun->mStartOffset = mNBSPData.LastPointRef().Offset() + 1;
    }
    lastRun->SetStartFromNormalWhiteSpaces();
    lastRun->SetEndBy(mEnd.RawReason());
    startRun->SetEndByTrailingWhiteSpaces();
    return;
  }

  MOZ_ASSERT(StartsFromHardLineBreak());

  WSFragment* startRun = mFragments.AppendElement();
  startRun->MarkAsStartOfHardLine();
  if (mStart.PointRef().IsSet()) {
    startRun->mStartNode = mStart.PointRef().GetContainer();
    startRun->mStartOffset = mStart.PointRef().Offset();
  }
  startRun->SetStartFrom(mStart.RawReason());
  if (mNBSPData.FirstPointRef().IsSet()) {
    startRun->mEndNode = mNBSPData.FirstPointRef().GetContainer();
    startRun->mEndOffset = mNBSPData.FirstPointRef().Offset();
  }
  startRun->SetEndByNormalWiteSpaces();

  // set up next run
  WSFragment* normalRun = mFragments.AppendElement();
  normalRun->MarkAsVisible();
  if (mNBSPData.FirstPointRef().IsSet()) {
    normalRun->mStartNode = mNBSPData.FirstPointRef().GetContainer();
    normalRun->mStartOffset = mNBSPData.FirstPointRef().Offset();
  }
  normalRun->SetStartFromLeadingWhiteSpaces();
  if (!EndsByBlockBoundary()) {
    // then no trailing ws.  this normal run ends the overall ws run.
    normalRun->SetEndBy(mEnd.RawReason());
    if (mEnd.PointRef().IsSet()) {
      normalRun->mEndNode = mEnd.PointRef().GetContainer();
      normalRun->mEndOffset = mEnd.PointRef().Offset();
    }
    return;
  }

  // we might have trailing ws.
  // it so happens that *if* there is an nbsp at end,
  // {mEndNode,mEndOffset-1} will point to it, even though in general
  // start/end points not guaranteed to be in text nodes.
  if (mNBSPData.LastPointRef().IsSet() && mEnd.PointRef().IsSet() &&
      mNBSPData.LastPointRef().GetContainer() ==
          mEnd.PointRef().GetContainer() &&
      mNBSPData.LastPointRef().Offset() == mEnd.PointRef().Offset() - 1) {
    // normal ws runs right up to adjacent block (nbsp next to block)
    normalRun->SetEndBy(mEnd.RawReason());
    normalRun->mEndNode = mEnd.PointRef().GetContainer();
    normalRun->mEndOffset = mEnd.PointRef().Offset();
    return;
  }

  if (mNBSPData.LastPointRef().IsSet()) {
    normalRun->mEndNode = mNBSPData.LastPointRef().GetContainer();
    normalRun->mEndOffset = mNBSPData.LastPointRef().Offset() + 1;
  }
  normalRun->SetEndByTrailingWhiteSpaces();

  // set up next run
  WSFragment* lastRun = mFragments.AppendElement();
  lastRun->MarkAsEndOfHardLine();
  if (mNBSPData.LastPointRef().IsSet()) {
    lastRun->mStartNode = mNBSPData.LastPointRef().GetContainer();
    lastRun->mStartOffset = mNBSPData.LastPointRef().Offset() + 1;
  }
  if (mEnd.PointRef().IsSet()) {
    lastRun->mEndNode = mEnd.PointRef().GetContainer();
    lastRun->mEndOffset = mEnd.PointRef().Offset();
  }
  lastRun->SetStartFromNormalWhiteSpaces();
  lastRun->SetEndBy(mEnd.RawReason());
}

void WSRunScanner::InitializeWithSingleFragment(
    WSFragment::Visible aIsVisible,
    WSFragment::StartOfHardLine aIsStartOfHardLine,
    WSFragment::EndOfHardLine aIsEndOfHardLine) {
  MOZ_ASSERT(mFragments.IsEmpty());

  WSFragment* startRun = mFragments.AppendElement();

  if (mStart.PointRef().IsSet()) {
    startRun->mStartNode = mStart.PointRef().GetContainer();
    startRun->mStartOffset = mStart.PointRef().Offset();
  }
  if (aIsVisible == WSFragment::Visible::Yes) {
    startRun->MarkAsVisible();
  }
  if (aIsStartOfHardLine == WSFragment::StartOfHardLine::Yes) {
    startRun->MarkAsStartOfHardLine();
  }
  if (aIsEndOfHardLine == WSFragment::EndOfHardLine::Yes) {
    startRun->MarkAsEndOfHardLine();
  }
  if (mEnd.PointRef().IsSet()) {
    startRun->mEndNode = mEnd.PointRef().GetContainer();
    startRun->mEndOffset = mEnd.PointRef().Offset();
  }
  startRun->SetStartFrom(mStart.RawReason());
  startRun->SetEndBy(mEnd.RawReason());
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

  // get the runs before and after selection
  const WSFragment* beforeRun = FindNearestFragment(mScanStartPoint, false);
  const WSFragment* afterRun =
      aEndObject->FindNearestFragment(aEndObject->mScanStartPoint, true);

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
  // used to prepare ws to be split across two blocks.  The main issue
  // here is make sure normalWS doesn't end up becoming non-significant
  // leading or trailing ws after the split.

  // get the runs before and after selection
  const WSFragment* beforeRun = FindNearestFragment(mScanStartPoint, false);
  const WSFragment* afterRun = FindNearestFragment(mScanStartPoint, true);

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

template <typename PT, typename CT>
WSRunScanner::WSFragmentArray::index_type
WSRunScanner::FindNearestFragmentIndex(const EditorDOMPointBase<PT, CT>& aPoint,
                                       bool aForward) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  for (WSFragmentArray::index_type i = 0; i < WSFragmentArrayRef().Length();
       i++) {
    const WSFragment& fragment = WSFragmentArrayRef()[i];
    int32_t comp = fragment.mStartNode
                       ? *nsContentUtils::ComparePoints(
                             aPoint.ToRawRangeBoundary(),
                             fragment.StartPoint().ToRawRangeBoundary())
                       : -1;
    if (comp <= 0) {
      // aPoint equals or before start of the fragment.
      // Return it if we're scanning forward, otherwise, nullptr.
      return aForward ? i : WSFragmentArray::NoIndex;
    }

    comp = fragment.mEndNode ? *nsContentUtils::ComparePoints(
                                   aPoint.ToRawRangeBoundary(),
                                   fragment.EndPoint().ToRawRangeBoundary())
                             : -1;
    if (comp < 0) {
      // If aPoint is in the fragment, return it.
      return i;
    }

    if (!comp) {
      // If aPoint is at end of the fragment, return next fragment if we're
      // scanning forward, otherwise, return it.
      return aForward ? (i + 1 < WSFragmentArrayRef().Length()
                             ? i + 1
                             : WSFragmentArray::NoIndex)
                      : i;
    }

    if (i == WSFragmentArrayRef().Length() - 1) {
      // If the fragment is the last fragment and aPoint is after end of the
      // last fragment, return nullptr if we're scanning forward, otherwise,
      // return this last fragment.
      return aForward ? WSFragmentArray::NoIndex : i;
    }
  }

  return WSFragmentArray::NoIndex;
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

nsresult WSRunObject::NormalizeWhiteSpacesAtEndOf(const WSFragment& aRun) {
  // Check if it's a visible fragment in a hard line.
  if (!aRun.IsVisibleAndMiddleOfHardLine()) {
    return NS_ERROR_FAILURE;
  }

  // Remove this block if we ship Blink-compat white-space normalization.
  if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    // now check that what is to the left of it is compatible with replacing
    // nbsp with space
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
    bool isPreviousCharASCIIWhiteSpace =
        atPreviousCharOfPreviousCharOfEndOfRun.IsSet() &&
        !atPreviousCharOfPreviousCharOfEndOfRun.IsEndOfContainer() &&
        atPreviousCharOfPreviousCharOfEndOfRun.IsCharASCIISpace();
    bool maybeNBSPFollowingVisibleContent =
        (atPreviousCharOfPreviousCharOfEndOfRun.IsSet() &&
         !isPreviousCharASCIIWhiteSpace) ||
        (!atPreviousCharOfPreviousCharOfEndOfRun.IsSet() &&
         (aRun.StartsFromNormalText() || aRun.StartsFromSpecialContent()));
    bool followedByVisibleContentOrBRElement = false;

    // If the NBSP follows a visible content or an ASCII white-space, i.e.,
    // unless NBSP is first character and start of a block, we may need to
    // insert <br> element and restore the NBSP to an ASCII white-space.
    if (maybeNBSPFollowingVisibleContent || isPreviousCharASCIIWhiteSpace) {
      followedByVisibleContentOrBRElement = aRun.EndsByNormalText() ||
                                            aRun.EndsBySpecialContent() ||
                                            aRun.EndsByBRElement();
      // First, try to insert <br> element if NBSP is at end of a block.
      // XXX We should stop this if there is a visible content.
      if (aRun.EndsByBlockBoundary() && mScanStartPoint.IsInContentNode()) {
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
          isPreviousCharASCIIWhiteSpace =
              atPreviousCharOfPreviousCharOfEndOfRun.IsSet() &&
              !atPreviousCharOfPreviousCharOfEndOfRun.IsEndOfContainer() &&
              atPreviousCharOfPreviousCharOfEndOfRun.IsCharASCIISpace();
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
                    MOZ_KnownLive(*atPreviousCharOfEndOfRun.ContainerAsText()),
                    atPreviousCharOfEndOfRun.Offset(), 1, u" "_ns);
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
    MOZ_ASSERT(!atPreviousCharOfPreviousCharOfEndOfRun.IsEndOfContainer());
    EditorDOMPointInText atFirstASCIIWhiteSpace =
        GetFirstASCIIWhiteSpacePointCollapsedTo(
            atPreviousCharOfPreviousCharOfEndOfRun);
    AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
    uint32_t numberOfASCIIWhiteSpacesInStartNode =
        atFirstASCIIWhiteSpace.ContainerAsText() ==
                atPreviousCharOfEndOfRun.ContainerAsText()
            ? atPreviousCharOfEndOfRun.Offset() -
                  atFirstASCIIWhiteSpace.Offset()
            : atFirstASCIIWhiteSpace.ContainerAsText()->Length() -
                  atFirstASCIIWhiteSpace.Offset();
    // Replace all preceding ASCII white-spaces **and** the NBSP.
    uint32_t replaceLengthInStartNode =
        numberOfASCIIWhiteSpacesInStartNode +
        (atFirstASCIIWhiteSpace.ContainerAsText() ==
                 atPreviousCharOfEndOfRun.ContainerAsText()
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
        atPreviousCharOfEndOfRun.GetContainer()) {
      return NS_OK;
    }

    // We need to remove the following unnecessary ASCII white-spaces and
    // NBSP at atPreviousCharOfEndOfRun because we collapsed them into
    // the start node.
    rv = MOZ_KnownLive(mHTMLEditor)
             .DeleteTextAndTextNodesWithTransaction(
                 EditorDOMPointInText::AtEndOf(
                     *atFirstASCIIWhiteSpace.ContainerAsText()),
                 atPreviousCharOfEndOfRun.NextPoint());
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
  EditorDOMPoint atEndOfRun = aRun.EndPoint();
  EditorDOMPointInText atPreviousCharOfEndOfRun =
      GetPreviousEditableCharPoint(atEndOfRun);
  if (!atPreviousCharOfEndOfRun.IsSet() ||
      atPreviousCharOfEndOfRun.IsEndOfContainer() ||
      !atPreviousCharOfEndOfRun.IsCharNBSP()) {
    return NS_OK;
  }

  // Next, consider the range to collapse ASCII white-spaces before there.
  EditorDOMPointInText startToDelete, endToDelete;

  EditorDOMPointInText atPreviousCharOfPreviousCharOfEndOfRun =
      GetPreviousEditableCharPoint(atPreviousCharOfEndOfRun);
  // If there are some preceding ASCII white-spaces, we need to treat them
  // as one white-space.  I.e., we need to collapse them.
  if (atPreviousCharOfEndOfRun.IsCharNBSP() &&
      atPreviousCharOfPreviousCharOfEndOfRun.IsSet() &&
      atPreviousCharOfPreviousCharOfEndOfRun.IsCharASCIISpace()) {
    startToDelete = GetFirstASCIIWhiteSpacePointCollapsedTo(
        atPreviousCharOfPreviousCharOfEndOfRun);
    endToDelete = atPreviousCharOfPreviousCharOfEndOfRun;
  }
  // Otherwise, we don't need to remove any white-spaces, but we may need
  // to normalize the white-space sequence containing the previous NBSP.
  else {
    startToDelete = endToDelete = atPreviousCharOfEndOfRun.NextPoint();
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
    // an ASCII white-space, we don't need to replace it with same character.
    if (!atPreviousCharOfPreviousChar.IsEndOfContainer() &&
        atPreviousCharOfPreviousChar.IsCharASCIISpace()) {
      return NS_OK;
    }
  }
  // If previous content of the NBSP is block boundary, we cannot replace the
  // NBSP with an ASCII white-space to keep it rendered.
  else if (!aRun.StartsFromNormalText() && !aRun.StartsFromSpecialContent()) {
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
    // If following character of an NBSP is an ASCII white-space, we don't
    // need to replace it with same character.
    if (!atNextCharOfNextCharOfNBSP.IsEndOfContainer() &&
        atNextCharOfNextCharOfNBSP.IsCharASCIISpace()) {
      return NS_OK;
    }
  }
  // If the NBSP is last character in the hard line, we don't need to
  // replace it because it's required to render multiple white-spaces.
  else if (!aRun.EndsByNormalText() && !aRun.EndsBySpecialContent() &&
           !aRun.EndsByBRElement()) {
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

nsresult WSRunObject::Scrub() {
  for (const WSFragment& fragment : WSFragmentArrayRef()) {
    if (fragment.IsMiddleOfHardLine()) {
      continue;
    }
    nsresult rv = MOZ_KnownLive(mHTMLEditor)
                      .DeleteTextAndTextNodesWithTransaction(
                          fragment.StartPoint(), fragment.EndPoint());
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

}  // namespace mozilla
