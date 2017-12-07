/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WSRunObject.h"

#include "TextEditUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/mozalloc.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/SelectionState.h"

#include "nsAString.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsISupportsImpl.h"
#include "nsRange.h"
#include "nsString.h"
#include "nsTextFragment.h"

namespace mozilla {

using namespace dom;

const char16_t nbsp = 160;

WSRunObject::WSRunObject(HTMLEditor* aHTMLEditor,
                         nsINode* aNode,
                         int32_t aOffset)
  : mNode(aNode)
  , mOffset(aOffset)
  , mPRE(false)
  , mStartOffset(0)
  , mEndOffset(0)
  , mFirstNBSPOffset(0)
  , mLastNBSPOffset(0)
  , mStartRun(nullptr)
  , mEndRun(nullptr)
  , mHTMLEditor(aHTMLEditor)
{
  GetWSNodes();
  GetRuns();
}

WSRunObject::WSRunObject(HTMLEditor* aHTMLEditor,
                         nsIDOMNode* aNode,
                         int32_t aOffset)
  : mNode(do_QueryInterface(aNode))
  , mOffset(aOffset)
  , mPRE(false)
  , mStartOffset(0)
  , mEndOffset(0)
  , mFirstNBSPOffset(0)
  , mLastNBSPOffset(0)
  , mStartRun(nullptr)
  , mEndRun(nullptr)
  , mHTMLEditor(aHTMLEditor)
{
  GetWSNodes();
  GetRuns();
}

WSRunObject::~WSRunObject()
{
  ClearRuns();
}

nsresult
WSRunObject::ScrubBlockBoundary(HTMLEditor* aHTMLEditor,
                                BlockBoundary aBoundary,
                                nsINode* aBlock,
                                int32_t aOffset)
{
  NS_ENSURE_TRUE(aHTMLEditor && aBlock, NS_ERROR_NULL_POINTER);

  int32_t offset;
  if (aBoundary == kBlockStart) {
    offset = 0;
  } else if (aBoundary == kBlockEnd) {
    offset = aBlock->Length();
  } else {
    // Else we are scrubbing an outer boundary - just before or after a block
    // element.
    NS_ENSURE_STATE(aOffset >= 0);
    offset = aOffset;
  }

  WSRunObject theWSObj(aHTMLEditor, aBlock, offset);
  return theWSObj.Scrub();
}

nsresult
WSRunObject::PrepareToJoinBlocks(HTMLEditor* aHTMLEditor,
                                 Element* aLeftBlock,
                                 Element* aRightBlock)
{
  NS_ENSURE_TRUE(aLeftBlock && aRightBlock && aHTMLEditor,
                 NS_ERROR_NULL_POINTER);

  WSRunObject leftWSObj(aHTMLEditor, aLeftBlock, aLeftBlock->Length());
  WSRunObject rightWSObj(aHTMLEditor, aRightBlock, 0);

  return leftWSObj.PrepareToDeleteRangePriv(&rightWSObj);
}

nsresult
WSRunObject::PrepareToDeleteRange(HTMLEditor* aHTMLEditor,
                                  nsCOMPtr<nsINode>* aStartNode,
                                  int32_t* aStartOffset,
                                  nsCOMPtr<nsINode>* aEndNode,
                                  int32_t* aEndOffset)
{
  NS_ENSURE_TRUE(aHTMLEditor && aStartNode && *aStartNode && aStartOffset &&
                 aEndNode && *aEndNode && aEndOffset, NS_ERROR_NULL_POINTER);

  AutoTrackDOMPoint trackerStart(aHTMLEditor->mRangeUpdater,
                                 aStartNode, aStartOffset);
  AutoTrackDOMPoint trackerEnd(aHTMLEditor->mRangeUpdater,
                               aEndNode, aEndOffset);

  WSRunObject leftWSObj(aHTMLEditor, *aStartNode, *aStartOffset);
  WSRunObject rightWSObj(aHTMLEditor, *aEndNode, *aEndOffset);

  return leftWSObj.PrepareToDeleteRangePriv(&rightWSObj);
}

nsresult
WSRunObject::PrepareToDeleteNode(HTMLEditor* aHTMLEditor,
                                 nsIContent* aContent)
{
  NS_ENSURE_TRUE(aContent && aHTMLEditor, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> parent = aContent->GetParentNode();
  NS_ENSURE_STATE(parent);
  int32_t offset = parent->IndexOf(aContent);

  WSRunObject leftWSObj(aHTMLEditor, parent, offset);
  WSRunObject rightWSObj(aHTMLEditor, parent, offset + 1);

  return leftWSObj.PrepareToDeleteRangePriv(&rightWSObj);
}

nsresult
WSRunObject::PrepareToSplitAcrossBlocks(HTMLEditor* aHTMLEditor,
                                        nsCOMPtr<nsINode>* aSplitNode,
                                        int32_t* aSplitOffset)
{
  NS_ENSURE_TRUE(aHTMLEditor && aSplitNode && *aSplitNode && aSplitOffset,
                 NS_ERROR_NULL_POINTER);

  AutoTrackDOMPoint tracker(aHTMLEditor->mRangeUpdater,
                            aSplitNode, aSplitOffset);

  WSRunObject wsObj(aHTMLEditor, *aSplitNode, *aSplitOffset);

  return wsObj.PrepareToSplitAcrossBlocksPriv();
}

already_AddRefed<Element>
WSRunObject::InsertBreak(Selection& aSelection,
                         const EditorRawDOMPoint& aPointToInsert,
                         nsIEditor::EDirection aSelect)
{
  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return nullptr;
  }

  // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
  // meanwhile, the pre case is handled in WillInsertText in
  // HTMLEditRules.cpp

  WSFragment* beforeRun = FindNearestRun(aPointToInsert, false);
  WSFragment* afterRun = FindNearestRun(aPointToInsert, true);

  EditorDOMPoint pointToInsert(aPointToInsert);
  {
    // Some scoping for AutoTrackDOMPoint.  This will track our insertion
    // point while we tweak any surrounding whitespace
    AutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, &pointToInsert);

    // Handle any changes needed to ws run after inserted br
    if (!afterRun || (afterRun->mType & WSType::trailingWS)) {
      // Don't need to do anything.  Just insert break.  ws won't change.
    } else if (afterRun->mType & WSType::leadingWS) {
      // Delete the leading ws that is after insertion point.  We don't
      // have to (it would still not be significant after br), but it's
      // just more aesthetically pleasing to.
      nsresult rv = DeleteRange(pointToInsert.AsRaw(), afterRun->EndPoint());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }
    } else if (afterRun->mType == WSType::normalWS) {
      // Need to determine if break at front of non-nbsp run.  If so, convert
      // run to nbsp.
      WSPoint thePoint = GetNextCharPoint(pointToInsert.AsRaw());
      if (thePoint.mTextNode && nsCRT::IsAsciiSpace(thePoint.mChar)) {
        WSPoint prevPoint = GetPreviousCharPoint(thePoint);
        if (!prevPoint.mTextNode ||
            (prevPoint.mTextNode && !nsCRT::IsAsciiSpace(prevPoint.mChar))) {
          // We are at start of non-nbsps.  Convert to a single nbsp.
          nsresult rv = ConvertToNBSP(thePoint);
          NS_ENSURE_SUCCESS(rv, nullptr);
        }
      }
    }

    // Handle any changes needed to ws run before inserted br
    if (!beforeRun || (beforeRun->mType & WSType::leadingWS)) {
      // Don't need to do anything.  Just insert break.  ws won't change.
    } else if (beforeRun->mType & WSType::trailingWS) {
      // Need to delete the trailing ws that is before insertion point, because it
      // would become significant after break inserted.
      nsresult rv = DeleteRange(beforeRun->StartPoint(), pointToInsert.AsRaw());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }
    } else if (beforeRun->mType == WSType::normalWS) {
      // Try to change an nbsp to a space, just to prevent nbsp proliferation
      nsresult rv =
        ReplacePreviousNBSPIfUnncessary(beforeRun, pointToInsert.AsRaw());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }
    }
  }

  RefPtr<Element> newBRElement =
    mHTMLEditor->CreateBRImpl(aSelection, pointToInsert.AsRaw(), aSelect);
  if (NS_WARN_IF(!newBRElement)) {
    return nullptr;
  }
  return newBRElement.forget();
}

nsresult
WSRunObject::InsertText(nsIDocument& aDocument,
                        const nsAString& aStringToInsert,
                        const EditorRawDOMPoint& aPointToInsert,
                        EditorRawDOMPoint* aPointAfterInsertedString)
{
  // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
  // meanwhile, the pre case is handled in WillInsertText in
  // HTMLEditRules.cpp

  // MOOSE: for now, just getting the ws logic straight.  This implementation
  // is very slow.  Will need to replace edit rules impl with a more efficient
  // text sink here that does the minimal amount of searching/replacing/copying

  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }
  MOZ_ASSERT(aPointToInsert.IsSet());


  if (aStringToInsert.IsEmpty()) {
    if (aPointAfterInsertedString) {
      *aPointAfterInsertedString = aPointToInsert;
    }
    return NS_OK;
  }

  WSFragment* beforeRun = FindNearestRun(aPointToInsert, false);
  WSFragment* afterRun = FindNearestRun(aPointToInsert, true);

  EditorDOMPoint pointToInsert(aPointToInsert);
  nsAutoString theString(aStringToInsert);
  {
    // Some scoping for AutoTrackDOMPoint.  This will track our insertion
    // point while we tweak any surrounding whitespace
    AutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, &pointToInsert);

    // Handle any changes needed to ws run after inserted text
    if (!afterRun || afterRun->mType & WSType::trailingWS) {
      // Don't need to do anything.  Just insert text.  ws won't change.
    } else if (afterRun->mType & WSType::leadingWS) {
      // Delete the leading ws that is after insertion point, because it
      // would become significant after text inserted.
      nsresult rv = DeleteRange(pointToInsert.AsRaw(), afterRun->EndPoint());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (afterRun->mType == WSType::normalWS) {
      // Try to change an nbsp to a space, if possible, just to prevent nbsp
      // proliferation
      nsresult rv = CheckLeadingNBSP(afterRun, pointToInsert.GetContainer(),
                                     pointToInsert.Offset());
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Handle any changes needed to ws run before inserted text
    if (!beforeRun || beforeRun->mType & WSType::leadingWS) {
      // Don't need to do anything.  Just insert text.  ws won't change.
    } else if (beforeRun->mType & WSType::trailingWS) {
      // Need to delete the trailing ws that is before insertion point, because
      // it would become significant after text inserted.
      nsresult rv = DeleteRange(beforeRun->StartPoint(), pointToInsert.AsRaw());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (beforeRun->mType == WSType::normalWS) {
      // Try to change an nbsp to a space, if possible, just to prevent nbsp
      // proliferation
      nsresult rv =
        ReplacePreviousNBSPIfUnncessary(beforeRun, pointToInsert.AsRaw());
      if (NS_WARN_IF(NS_FAILED(rv))) {
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
      if (beforeRun->mType & WSType::leadingWS) {
        theString.SetCharAt(nbsp, 0);
      } else if (beforeRun->mType & WSType::normalWS) {
        WSPoint wspoint = GetPreviousCharPoint(pointToInsert.AsRaw());
        if (wspoint.mTextNode && nsCRT::IsAsciiSpace(wspoint.mChar)) {
          theString.SetCharAt(nbsp, 0);
        }
      }
    } else if (mStartReason & WSType::block || mStartReason == WSType::br) {
      theString.SetCharAt(nbsp, 0);
    }
  }

  // Then the tail
  uint32_t lastCharIndex = theString.Length() - 1;

  if (nsCRT::IsAsciiSpace(theString[lastCharIndex])) {
    // We have a leading space
    if (afterRun) {
      if (afterRun->mType & WSType::trailingWS) {
        theString.SetCharAt(nbsp, lastCharIndex);
      } else if (afterRun->mType & WSType::normalWS) {
        WSPoint wspoint = GetNextCharPoint(pointToInsert.AsRaw());
        if (wspoint.mTextNode && nsCRT::IsAsciiSpace(wspoint.mChar)) {
          theString.SetCharAt(nbsp, lastCharIndex);
        }
      }
    } else if (mEndReason & WSType::block) {
      theString.SetCharAt(nbsp, lastCharIndex);
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
        theString.SetCharAt(nbsp, i - 1);
      } else {
        prevWS = true;
      }
    } else {
      prevWS = false;
    }
  }

  // Ready, aim, fire!
  nsresult rv =
    mHTMLEditor->InsertTextImpl(aDocument, theString, pointToInsert.AsRaw(),
                                aPointAfterInsertedString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_OK;
  }
  return NS_OK;
}

nsresult
WSRunObject::DeleteWSBackward()
{
  WSPoint point = GetPreviousCharPoint(Point());
  NS_ENSURE_TRUE(point.mTextNode, NS_OK);  // nothing to delete

  // Easy case, preformatted ws.
  if (mPRE &&  (nsCRT::IsAsciiSpace(point.mChar) || point.mChar == nbsp)) {
    nsresult rv =
      DeleteRange(EditorRawDOMPoint(point.mTextNode, point.mOffset),
                  EditorRawDOMPoint(point.mTextNode, point.mOffset + 1));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // Caller's job to ensure that previous char is really ws.  If it is normal
  // ws, we need to delete the whole run.
  if (nsCRT::IsAsciiSpace(point.mChar)) {
    RefPtr<Text> startNodeText, endNodeText;
    int32_t startOffset, endOffset;
    GetAsciiWSBounds(eBoth, point.mTextNode, point.mOffset + 1,
                     getter_AddRefs(startNodeText), &startOffset,
                     getter_AddRefs(endNodeText), &endOffset);

    // adjust surrounding ws
    nsCOMPtr<nsINode> startNode = startNodeText.get();
    nsCOMPtr<nsINode> endNode = endNodeText.get();
    nsresult rv =
      WSRunObject::PrepareToDeleteRange(mHTMLEditor,
                                        address_of(startNode), &startOffset,
                                        address_of(endNode), &endOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    // finally, delete that ws
    rv = DeleteRange(EditorRawDOMPoint(startNode, startOffset),
                     EditorRawDOMPoint(endNode, endOffset));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (point.mChar == nbsp) {
    nsCOMPtr<nsINode> node(point.mTextNode);
    // adjust surrounding ws
    int32_t startOffset = point.mOffset;
    int32_t endOffset = point.mOffset + 1;
    nsresult rv =
      WSRunObject::PrepareToDeleteRange(mHTMLEditor,
                                        address_of(node), &startOffset,
                                        address_of(node), &endOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    // finally, delete that ws
    rv = DeleteRange(EditorRawDOMPoint(node, startOffset),
                     EditorRawDOMPoint(node, endOffset));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  return NS_OK;
}

nsresult
WSRunObject::DeleteWSForward()
{
  WSPoint point = GetNextCharPoint(Point());
  NS_ENSURE_TRUE(point.mTextNode, NS_OK); // nothing to delete

  // Easy case, preformatted ws.
  if (mPRE && (nsCRT::IsAsciiSpace(point.mChar) || point.mChar == nbsp)) {
    nsresult rv =
      DeleteRange(EditorRawDOMPoint(point.mTextNode, point.mOffset),
                  EditorRawDOMPoint(point.mTextNode, point.mOffset + 1));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // Caller's job to ensure that next char is really ws.  If it is normal ws,
  // we need to delete the whole run.
  if (nsCRT::IsAsciiSpace(point.mChar)) {
    RefPtr<Text> startNodeText, endNodeText;
    int32_t startOffset, endOffset;
    GetAsciiWSBounds(eBoth, point.mTextNode, point.mOffset + 1,
                     getter_AddRefs(startNodeText), &startOffset,
                     getter_AddRefs(endNodeText), &endOffset);

    // Adjust surrounding ws
    nsCOMPtr<nsINode> startNode(startNodeText), endNode(endNodeText);
    nsresult rv =
      WSRunObject::PrepareToDeleteRange(mHTMLEditor,
                                        address_of(startNode), &startOffset,
                                        address_of(endNode), &endOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    // Finally, delete that ws
    rv = DeleteRange(EditorRawDOMPoint(startNode, startOffset),
                     EditorRawDOMPoint(endNode, endOffset));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (point.mChar == nbsp) {
    nsCOMPtr<nsINode> node(point.mTextNode);
    // Adjust surrounding ws
    int32_t startOffset = point.mOffset;
    int32_t endOffset = point.mOffset+1;
    nsresult rv =
      WSRunObject::PrepareToDeleteRange(mHTMLEditor,
                                        address_of(node), &startOffset,
                                        address_of(node), &endOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    // Finally, delete that ws
    rv = DeleteRange(EditorRawDOMPoint(node, startOffset),
                     EditorRawDOMPoint(node, endOffset));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  return NS_OK;
}

void
WSRunObject::PriorVisibleNode(nsINode* aNode,
                              int32_t aOffset,
                              nsCOMPtr<nsINode>* outVisNode,
                              int32_t* outVisOffset,
                              WSType* outType)
{
  // Find first visible thing before the point.  Position
  // outVisNode/outVisOffset just _after_ that thing.  If we don't find
  // anything return start of ws.
  MOZ_ASSERT(aNode && outVisNode && outVisOffset && outType);

  WSFragment* run = FindNearestRun(EditorRawDOMPoint(aNode, aOffset), false);

  // Is there a visible run there or earlier?
  for (; run; run = run->mLeft) {
    if (run->mType == WSType::normalWS) {
      WSPoint point = GetPreviousCharPoint(EditorRawDOMPoint(aNode, aOffset));
      // When it's a non-empty text node, return it.
      if (point.mTextNode && point.mTextNode->Length()) {
        *outVisNode = point.mTextNode;
        *outVisOffset = point.mOffset + 1;
        if (nsCRT::IsAsciiSpace(point.mChar) || point.mChar == nbsp) {
          *outType = WSType::normalWS;
        } else {
          *outType = WSType::text;
        }
        return;
      }
      // If no text node, keep looking.  We should eventually fall out of loop
    }
  }

  // If we get here, then nothing in ws data to find.  Return start reason.
  *outVisNode = mStartReasonNode;
  // This really isn't meaningful if mStartReasonNode != mStartNode
  *outVisOffset = mStartOffset;
  *outType = mStartReason;
}


void
WSRunObject::NextVisibleNode(nsINode* aNode,
                             int32_t aOffset,
                             nsCOMPtr<nsINode>* outVisNode,
                             int32_t* outVisOffset,
                             WSType* outType)
{
  // Find first visible thing after the point.  Position
  // outVisNode/outVisOffset just _before_ that thing.  If we don't find
  // anything return end of ws.
  MOZ_ASSERT(aNode && outVisNode && outVisOffset && outType);

  WSFragment* run = FindNearestRun(EditorRawDOMPoint(aNode, aOffset), true);

  // Is there a visible run there or later?
  for (; run; run = run->mRight) {
    if (run->mType == WSType::normalWS) {
      WSPoint point = GetNextCharPoint(EditorRawDOMPoint(aNode, aOffset));
      // When it's a non-empty text node, return it.
      if (point.mTextNode && point.mTextNode->Length()) {
        *outVisNode = point.mTextNode;
        *outVisOffset = point.mOffset;
        if (nsCRT::IsAsciiSpace(point.mChar) || point.mChar == nbsp) {
          *outType = WSType::normalWS;
        } else {
          *outType = WSType::text;
        }
        return;
      }
      // If no text node, keep looking.  We should eventually fall out of loop
    }
  }

  // If we get here, then nothing in ws data to find.  Return end reason
  *outVisNode = mEndReasonNode;
  // This really isn't meaningful if mEndReasonNode != mEndNode
  *outVisOffset = mEndOffset;
  *outType = mEndReason;
}

nsresult
WSRunObject::AdjustWhitespace()
{
  // this routine examines a run of ws and tries to get rid of some unneeded nbsp's,
  // replacing them with regualr ascii space if possible.  Keeping things simple
  // for now and just trying to fix up the trailing ws in the run.
  if (!mLastNBSPNode) {
    // nothing to do!
    return NS_OK;
  }
  WSFragment *curRun = mStartRun;
  while (curRun) {
    // look for normal ws run
    if (curRun->mType == WSType::normalWS) {
      nsresult rv = CheckTrailingNBSPOfRun(curRun);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    curRun = curRun->mRight;
  }
  return NS_OK;
}


//--------------------------------------------------------------------------------------------
//   protected methods
//--------------------------------------------------------------------------------------------

nsINode*
WSRunObject::GetWSBoundingParent()
{
  NS_ENSURE_TRUE(mNode, nullptr);
  OwningNonNull<nsINode> wsBoundingParent = *mNode;
  while (!IsBlockNode(wsBoundingParent)) {
    nsCOMPtr<nsINode> parent = wsBoundingParent->GetParentNode();
    if (!parent || !mHTMLEditor->IsEditable(parent)) {
      break;
    }
    wsBoundingParent = parent;
  }
  return wsBoundingParent;
}

nsresult
WSRunObject::GetWSNodes()
{
  // collect up an array of nodes that are contiguous with the insertion point
  // and which contain only whitespace.  Stop if you reach non-ws text or a new
  // block boundary.
  EditorDOMPoint start(mNode, mOffset), end(mNode, mOffset);
  nsCOMPtr<nsINode> wsBoundingParent = GetWSBoundingParent();

  // first look backwards to find preceding ws nodes
  if (RefPtr<Text> textNode = mNode->GetAsText()) {
    const nsTextFragment* textFrag = textNode->GetText();

    mNodeArray.InsertElementAt(0, textNode);
    if (mOffset) {
      for (int32_t pos = mOffset - 1; pos >= 0; pos--) {
        // sanity bounds check the char position.  bug 136165
        if (uint32_t(pos) >= textFrag->GetLength()) {
          NS_NOTREACHED("looking beyond end of text fragment");
          continue;
        }
        char16_t theChar = textFrag->CharAt(pos);
        if (!nsCRT::IsAsciiSpace(theChar)) {
          if (theChar != nbsp) {
            mStartNode = textNode;
            mStartOffset = pos + 1;
            mStartReason = WSType::text;
            mStartReasonNode = textNode;
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
  }

  while (!mStartNode) {
    // we haven't found the start of ws yet.  Keep looking
    nsCOMPtr<nsIContent> priorNode = GetPreviousWSNode(start, wsBoundingParent);
    if (priorNode) {
      if (IsBlockNode(priorNode)) {
        mStartNode = start.GetContainer();
        mStartOffset = start.Offset();
        mStartReason = WSType::otherBlock;
        mStartReasonNode = priorNode;
      } else if (priorNode->IsNodeOfType(nsINode::eTEXT) &&
                 priorNode->IsEditable()) {
        RefPtr<Text> textNode = priorNode->GetAsText();
        mNodeArray.InsertElementAt(0, textNode);
        const nsTextFragment *textFrag;
        if (!textNode || !(textFrag = textNode->GetText())) {
          return NS_ERROR_NULL_POINTER;
        }
        uint32_t len = textNode->TextLength();

        if (len < 1) {
          // Zero length text node. Set start point to it
          // so we can get past it!
          start.Set(priorNode, 0);
        } else {
          for (int32_t pos = len - 1; pos >= 0; pos--) {
            // sanity bounds check the char position.  bug 136165
            if (uint32_t(pos) >= textFrag->GetLength()) {
              NS_NOTREACHED("looking beyond end of text fragment");
              continue;
            }
            char16_t theChar = textFrag->CharAt(pos);
            if (!nsCRT::IsAsciiSpace(theChar)) {
              if (theChar != nbsp) {
                mStartNode = textNode;
                mStartOffset = pos + 1;
                mStartReason = WSType::text;
                mStartReasonNode = textNode;
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
        // it's a break or a special node, like <img>, that is not a block and not
        // a break but still serves as a terminator to ws runs.
        mStartNode = start.GetContainer();
        mStartOffset = start.Offset();
        if (TextEditUtils::IsBreak(priorNode)) {
          mStartReason = WSType::br;
        } else {
          mStartReason = WSType::special;
        }
        mStartReasonNode = priorNode;
      }
    } else {
      // no prior node means we exhausted wsBoundingParent
      mStartNode = start.GetContainer();
      mStartOffset = start.Offset();
      mStartReason = WSType::thisBlock;
      mStartReasonNode = wsBoundingParent;
    }
  }

  // then look ahead to find following ws nodes
  if (RefPtr<Text> textNode = mNode->GetAsText()) {
    // don't need to put it on list. it already is from code above
    const nsTextFragment *textFrag = textNode->GetText();

    uint32_t len = textNode->TextLength();
    if (uint16_t(mOffset)<len) {
      for (uint32_t pos = mOffset; pos < len; pos++) {
        // sanity bounds check the char position.  bug 136165
        if (pos >= textFrag->GetLength()) {
          NS_NOTREACHED("looking beyond end of text fragment");
          continue;
        }
        char16_t theChar = textFrag->CharAt(pos);
        if (!nsCRT::IsAsciiSpace(theChar)) {
          if (theChar != nbsp) {
            mEndNode = textNode;
            mEndOffset = pos;
            mEndReason = WSType::text;
            mEndReasonNode = textNode;
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
  }

  while (!mEndNode) {
    // we haven't found the end of ws yet.  Keep looking
    nsCOMPtr<nsIContent> nextNode = GetNextWSNode(end, wsBoundingParent);
    if (nextNode) {
      if (IsBlockNode(nextNode)) {
        // we encountered a new block.  therefore no more ws.
        mEndNode = end.GetContainer();
        mEndOffset = end.Offset();
        mEndReason = WSType::otherBlock;
        mEndReasonNode = nextNode;
      } else if (nextNode->IsNodeOfType(nsINode::eTEXT) &&
                 nextNode->IsEditable()) {
        RefPtr<Text> textNode = nextNode->GetAsText();
        mNodeArray.AppendElement(textNode);
        const nsTextFragment *textFrag;
        if (!textNode || !(textFrag = textNode->GetText())) {
          return NS_ERROR_NULL_POINTER;
        }
        uint32_t len = textNode->TextLength();

        if (len < 1) {
          // Zero length text node. Set end point to it
          // so we can get past it!
          end.Set(textNode, 0);
        } else {
          for (uint32_t pos = 0; pos < len; pos++) {
            // sanity bounds check the char position.  bug 136165
            if (pos >= textFrag->GetLength()) {
              NS_NOTREACHED("looking beyond end of text fragment");
              continue;
            }
            char16_t theChar = textFrag->CharAt(pos);
            if (!nsCRT::IsAsciiSpace(theChar)) {
              if (theChar != nbsp) {
                mEndNode = textNode;
                mEndOffset = pos;
                mEndReason = WSType::text;
                mEndReasonNode = textNode;
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
        if (TextEditUtils::IsBreak(nextNode)) {
          mEndReason = WSType::br;
        } else {
          mEndReason = WSType::special;
        }
        mEndReasonNode = nextNode;
      }
    } else {
      // no next node means we exhausted wsBoundingParent
      mEndNode = end.GetContainer();
      mEndOffset = end.Offset();
      mEndReason = WSType::thisBlock;
      mEndReasonNode = wsBoundingParent;
    }
  }

  return NS_OK;
}

void
WSRunObject::GetRuns()
{
  ClearRuns();

  // handle some easy cases first
  mHTMLEditor->IsPreformatted(GetAsDOMNode(mNode), &mPRE);
  // if it's preformatedd, or if we are surrounded by text or special, it's all one
  // big normal ws run
  if (mPRE ||
      ((mStartReason == WSType::text || mStartReason == WSType::special) &&
       (mEndReason == WSType::text || mEndReason == WSType::special ||
        mEndReason == WSType::br))) {
    MakeSingleWSRun(WSType::normalWS);
    return;
  }

  // if we are before or after a block (or after a break), and there are no nbsp's,
  // then it's all non-rendering ws.
  if (!mFirstNBSPNode && !mLastNBSPNode &&
      ((mStartReason & WSType::block) || mStartReason == WSType::br ||
       (mEndReason & WSType::block))) {
    WSType wstype;
    if ((mStartReason & WSType::block) || mStartReason == WSType::br) {
      wstype = WSType::leadingWS;
    }
    if (mEndReason & WSType::block) {
      wstype |= WSType::trailingWS;
    }
    MakeSingleWSRun(wstype);
    return;
  }

  // otherwise a little trickier.  shucks.
  mStartRun = new WSFragment();
  mStartRun->mStartNode = mStartNode;
  mStartRun->mStartOffset = mStartOffset;

  if (mStartReason & WSType::block || mStartReason == WSType::br) {
    // set up mStartRun
    mStartRun->mType = WSType::leadingWS;
    mStartRun->mEndNode = mFirstNBSPNode;
    mStartRun->mEndOffset = mFirstNBSPOffset;
    mStartRun->mLeftType = mStartReason;
    mStartRun->mRightType = WSType::normalWS;

    // set up next run
    WSFragment *normalRun = new WSFragment();
    mStartRun->mRight = normalRun;
    normalRun->mType = WSType::normalWS;
    normalRun->mStartNode = mFirstNBSPNode;
    normalRun->mStartOffset = mFirstNBSPOffset;
    normalRun->mLeftType = WSType::leadingWS;
    normalRun->mLeft = mStartRun;
    if (mEndReason != WSType::block) {
      // then no trailing ws.  this normal run ends the overall ws run.
      normalRun->mRightType = mEndReason;
      normalRun->mEndNode   = mEndNode;
      normalRun->mEndOffset = mEndOffset;
      mEndRun = normalRun;
    } else {
      // we might have trailing ws.
      // it so happens that *if* there is an nbsp at end, {mEndNode,mEndOffset-1}
      // will point to it, even though in general start/end points not
      // guaranteed to be in text nodes.
      if (mLastNBSPNode == mEndNode && mLastNBSPOffset == mEndOffset - 1) {
        // normal ws runs right up to adjacent block (nbsp next to block)
        normalRun->mRightType = mEndReason;
        normalRun->mEndNode   = mEndNode;
        normalRun->mEndOffset = mEndOffset;
        mEndRun = normalRun;
      } else {
        normalRun->mEndNode = mLastNBSPNode;
        normalRun->mEndOffset = mLastNBSPOffset+1;
        normalRun->mRightType = WSType::trailingWS;

        // set up next run
        WSFragment *lastRun = new WSFragment();
        lastRun->mType = WSType::trailingWS;
        lastRun->mStartNode = mLastNBSPNode;
        lastRun->mStartOffset = mLastNBSPOffset+1;
        lastRun->mEndNode = mEndNode;
        lastRun->mEndOffset = mEndOffset;
        lastRun->mLeftType = WSType::normalWS;
        lastRun->mLeft = normalRun;
        lastRun->mRightType = mEndReason;
        mEndRun = lastRun;
        normalRun->mRight = lastRun;
      }
    }
  } else {
    // mStartReason is not WSType::block or WSType::br; set up mStartRun
    mStartRun->mType = WSType::normalWS;
    mStartRun->mEndNode = mLastNBSPNode;
    mStartRun->mEndOffset = mLastNBSPOffset+1;
    mStartRun->mLeftType = mStartReason;

    // we might have trailing ws.
    // it so happens that *if* there is an nbsp at end, {mEndNode,mEndOffset-1}
    // will point to it, even though in general start/end points not
    // guaranteed to be in text nodes.
    if (mLastNBSPNode == mEndNode && mLastNBSPOffset == (mEndOffset - 1)) {
      mStartRun->mRightType = mEndReason;
      mStartRun->mEndNode   = mEndNode;
      mStartRun->mEndOffset = mEndOffset;
      mEndRun = mStartRun;
    } else {
      // set up next run
      WSFragment *lastRun = new WSFragment();
      lastRun->mType = WSType::trailingWS;
      lastRun->mStartNode = mLastNBSPNode;
      lastRun->mStartOffset = mLastNBSPOffset+1;
      lastRun->mLeftType = WSType::normalWS;
      lastRun->mLeft = mStartRun;
      lastRun->mRightType = mEndReason;
      mEndRun = lastRun;
      mStartRun->mRight = lastRun;
      mStartRun->mRightType = WSType::trailingWS;
    }
  }
}

void
WSRunObject::ClearRuns()
{
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

void
WSRunObject::MakeSingleWSRun(WSType aType)
{
  mStartRun = new WSFragment();

  mStartRun->mStartNode   = mStartNode;
  mStartRun->mStartOffset = mStartOffset;
  mStartRun->mType        = aType;
  mStartRun->mEndNode     = mEndNode;
  mStartRun->mEndOffset   = mEndOffset;
  mStartRun->mLeftType    = mStartReason;
  mStartRun->mRightType   = mEndReason;

  mEndRun  = mStartRun;
}

nsIContent*
WSRunObject::GetPreviousWSNodeInner(nsINode* aStartNode,
                                    nsINode* aBlockParent)
{
  // Can't really recycle various getnext/prior routines because we have
  // special needs here.  Need to step into inline containers but not block
  // containers.
  MOZ_ASSERT(aStartNode && aBlockParent);

  nsCOMPtr<nsIContent> priorNode = aStartNode->GetPreviousSibling();
  OwningNonNull<nsINode> curNode = *aStartNode;
  while (!priorNode) {
    // We have exhausted nodes in parent of aStartNode.
    nsCOMPtr<nsINode> curParent = curNode->GetParentNode();
    NS_ENSURE_TRUE(curParent, nullptr);
    if (curParent == aBlockParent) {
      // We have exhausted nodes in the block parent.  The convention here is
      // to return null.
      return nullptr;
    }
    // We have a parent: look for previous sibling
    priorNode = curParent->GetPreviousSibling();
    curNode = curParent;
  }
  // We have a prior node.  If it's a block, return it.
  if (IsBlockNode(priorNode)) {
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

nsIContent*
WSRunObject::GetPreviousWSNode(const EditorDOMPoint& aPoint,
                               nsINode* aBlockParent)
{
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

  if (NS_WARN_IF(!aPoint.GetContainerAsContent())) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> priorNode = aPoint.GetPreviousSiblingOfChildAtOffset();
  if (NS_WARN_IF(!priorNode)) {
    return nullptr;
  }

  // We have a prior node.  If it's a block, return it.
  if (IsBlockNode(priorNode)) {
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

nsIContent*
WSRunObject::GetNextWSNodeInner(nsINode* aStartNode,
                                nsINode* aBlockParent)
{
  // Can't really recycle various getnext/prior routines because we have
  // special needs here.  Need to step into inline containers but not block
  // containers.
  MOZ_ASSERT(aStartNode && aBlockParent);

  nsCOMPtr<nsIContent> nextNode = aStartNode->GetNextSibling();
  nsCOMPtr<nsINode> curNode = aStartNode;
  while (!nextNode) {
    // We have exhausted nodes in parent of aStartNode.
    nsCOMPtr<nsINode> curParent = curNode->GetParentNode();
    NS_ENSURE_TRUE(curParent, nullptr);
    if (curParent == aBlockParent) {
      // We have exhausted nodes in the block parent.  The convention here is
      // to return null.
      return nullptr;
    }
    // We have a parent: look for next sibling
    nextNode = curParent->GetNextSibling();
    curNode = curParent;
  }
  // We have a next node.  If it's a block, return it.
  if (IsBlockNode(nextNode)) {
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

nsIContent*
WSRunObject::GetNextWSNode(const EditorDOMPoint& aPoint,
                           nsINode* aBlockParent)
{
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

  if (NS_WARN_IF(!aPoint.GetContainerAsContent())) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> nextNode = aPoint.GetChildAtOffset();
  if (!nextNode) {
    if (aPoint.GetContainer() == aBlockParent) {
      // We are at end of the block.
      return nullptr;
    }

    // We are at end of non-block container
    return GetNextWSNodeInner(aPoint.GetContainer(), aBlockParent);
  }

  // We have a next node.  If it's a block, return it.
  if (IsBlockNode(nextNode)) {
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

nsresult
WSRunObject::PrepareToDeleteRangePriv(WSRunObject* aEndObject)
{
  // this routine adjust whitespace before *this* and after aEndObject
  // in preperation for the two areas to become adjacent after the
  // intervening content is deleted.  It's overly agressive right
  // now.  There might be a block boundary remaining between them after
  // the deletion, in which case these adjstments are unneeded (though
  // I don't think they can ever be harmful?)

  NS_ENSURE_TRUE(aEndObject, NS_ERROR_NULL_POINTER);

  // get the runs before and after selection
  WSFragment* beforeRun = FindNearestRun(Point(), false);
  WSFragment* afterRun = aEndObject->FindNearestRun(aEndObject->Point(), true);

  // trim after run of any leading ws
  if (afterRun && (afterRun->mType & WSType::leadingWS)) {
    nsresult rv =
      aEndObject->DeleteRange(aEndObject->Point(), afterRun->EndPoint());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  // adjust normal ws in afterRun if needed
  if (afterRun && afterRun->mType == WSType::normalWS && !aEndObject->mPRE) {
    if ((beforeRun && (beforeRun->mType & WSType::leadingWS)) ||
        (!beforeRun && ((mStartReason & WSType::block) ||
                        mStartReason == WSType::br))) {
      // make sure leading char of following ws is an nbsp, so that it will show up
      WSPoint point = aEndObject->GetNextCharPoint(aEndObject->Point());
      if (point.mTextNode && nsCRT::IsAsciiSpace(point.mChar)) {
        nsresult rv = aEndObject->ConvertToNBSP(point);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }
  // trim before run of any trailing ws
  if (beforeRun && (beforeRun->mType & WSType::trailingWS)) {
    nsresult rv = DeleteRange(beforeRun->StartPoint(), Point());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else if (beforeRun && beforeRun->mType == WSType::normalWS && !mPRE) {
    if ((afterRun && (afterRun->mType & WSType::trailingWS)) ||
        (afterRun && afterRun->mType == WSType::normalWS) ||
        (!afterRun && (aEndObject->mEndReason & WSType::block))) {
      // make sure trailing char of starting ws is an nbsp, so that it will show up
      WSPoint point = GetPreviousCharPoint(Point());
      if (point.mTextNode && nsCRT::IsAsciiSpace(point.mChar)) {
        RefPtr<Text> wsStartNode, wsEndNode;
        int32_t wsStartOffset, wsEndOffset;
        GetAsciiWSBounds(eBoth, mNode, mOffset,
                         getter_AddRefs(wsStartNode), &wsStartOffset,
                         getter_AddRefs(wsEndNode), &wsEndOffset);
        point.mTextNode = wsStartNode;
        point.mOffset = wsStartOffset;
        nsresult rv = ConvertToNBSP(point);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }
  return NS_OK;
}

nsresult
WSRunObject::PrepareToSplitAcrossBlocksPriv()
{
  // used to prepare ws to be split across two blocks.  The main issue
  // here is make sure normalWS doesn't end up becoming non-significant
  // leading or trailing ws after the split.

  // get the runs before and after selection
  WSFragment* beforeRun = FindNearestRun(Point(), false);
  WSFragment* afterRun = FindNearestRun(Point(), true);

  // adjust normal ws in afterRun if needed
  if (afterRun && afterRun->mType == WSType::normalWS) {
    // make sure leading char of following ws is an nbsp, so that it will show up
    WSPoint point = GetNextCharPoint(Point());
    if (point.mTextNode && nsCRT::IsAsciiSpace(point.mChar)) {
      nsresult rv = ConvertToNBSP(point);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // adjust normal ws in beforeRun if needed
  if (beforeRun && beforeRun->mType == WSType::normalWS) {
    // make sure trailing char of starting ws is an nbsp, so that it will show up
    WSPoint point = GetPreviousCharPoint(Point());
    if (point.mTextNode && nsCRT::IsAsciiSpace(point.mChar)) {
      RefPtr<Text> wsStartNode, wsEndNode;
      int32_t wsStartOffset, wsEndOffset;
      GetAsciiWSBounds(eBoth, mNode, mOffset,
                       getter_AddRefs(wsStartNode), &wsStartOffset,
                       getter_AddRefs(wsEndNode), &wsEndOffset);
      point.mTextNode = wsStartNode;
      point.mOffset = wsStartOffset;
      nsresult rv = ConvertToNBSP(point);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

nsresult
WSRunObject::DeleteRange(const EditorRawDOMPoint& aStartPoint,
                         const EditorRawDOMPoint& aEndPoint)
{
  if (NS_WARN_IF(!aStartPoint.IsSet()) ||
      NS_WARN_IF(!aEndPoint.IsSet())) {
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
    return mHTMLEditor->DeleteText(*aStartPoint.GetContainerAsText(),
                                   aStartPoint.Offset(),
                                   aEndPoint.Offset() - aStartPoint.Offset());
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
        nsresult rv =
          mHTMLEditor->DeleteText(*node, aStartPoint.Offset(),
                                  aStartPoint.GetContainer()->Length() -
                                    aStartPoint.Offset());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    } else if (node == aEndPoint.GetContainer()) {
      if (!aEndPoint.IsStartOfContainer()) {
        nsresult rv = mHTMLEditor->DeleteText(*node, 0, aEndPoint.Offset());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      break;
    } else {
      if (!range) {
        range = new nsRange(aStartPoint.GetContainer());
        nsresult rv = range->SetStartAndEnd(aStartPoint, aEndPoint);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      bool nodeBefore, nodeAfter;
      nsresult rv =
        nsRange::CompareNodeToRange(node, range, &nodeBefore, &nodeAfter);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (nodeAfter) {
        break;
      }
      if (!nodeBefore) {
        rv = mHTMLEditor->DeleteNode(node);
        if (NS_WARN_IF(NS_FAILED(rv))) {
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

WSRunObject::WSPoint
WSRunObject::GetNextCharPoint(const EditorRawDOMPoint& aPoint)
{
  MOZ_ASSERT(aPoint.IsSetAndValid());

  int32_t idx = mNodeArray.IndexOf(aPoint.GetContainer());
  if (idx == -1) {
    // Use range comparisons to get next text node which is in mNodeArray.
    return GetNextCharPointInternal(aPoint);
  }
  // Use WSPoint version of GetNextCharPoint()
  return GetNextCharPoint(WSPoint(mNodeArray[idx], aPoint.Offset(), 0));
}

WSRunObject::WSPoint
WSRunObject::GetPreviousCharPoint(const EditorRawDOMPoint& aPoint)
{
  MOZ_ASSERT(aPoint.IsSetAndValid());

  int32_t idx = mNodeArray.IndexOf(aPoint.GetContainer());
  if (idx == -1) {
    // Use range comparisons to get previous text node which is in mNodeArray.
    return GetPreviousCharPointInternal(aPoint);
  }
  // Use WSPoint version of GetPreviousCharPoint()
  return GetPreviousCharPoint(WSPoint(mNodeArray[idx], aPoint.Offset(), 0));
}

WSRunObject::WSPoint
WSRunObject::GetNextCharPoint(const WSPoint &aPoint)
{
  MOZ_ASSERT(aPoint.mTextNode);

  WSPoint outPoint;
  outPoint.mTextNode = nullptr;
  outPoint.mOffset = 0;
  outPoint.mChar = 0;

  int32_t idx = mNodeArray.IndexOf(aPoint.mTextNode);
  if (idx == -1) {
    // Can't find point, but it's not an error
    return outPoint;
  }

  if (static_cast<uint16_t>(aPoint.mOffset) < aPoint.mTextNode->TextLength()) {
    outPoint = aPoint;
    outPoint.mChar = GetCharAt(aPoint.mTextNode, aPoint.mOffset);
    return outPoint;
  }

  int32_t numNodes = mNodeArray.Length();
  if (idx + 1 < numNodes) {
    outPoint.mTextNode = mNodeArray[idx + 1];
    MOZ_ASSERT(outPoint.mTextNode);
    outPoint.mOffset = 0;
    outPoint.mChar = GetCharAt(outPoint.mTextNode, 0);
  }

  return outPoint;
}

WSRunObject::WSPoint
WSRunObject::GetPreviousCharPoint(const WSPoint &aPoint)
{
  MOZ_ASSERT(aPoint.mTextNode);

  WSPoint outPoint;
  outPoint.mTextNode = nullptr;
  outPoint.mOffset = 0;
  outPoint.mChar = 0;

  int32_t idx = mNodeArray.IndexOf(aPoint.mTextNode);
  if (idx == -1) {
    // Can't find point, but it's not an error
    return outPoint;
  }

  if (aPoint.mOffset) {
    outPoint = aPoint;
    outPoint.mOffset--;
    outPoint.mChar = GetCharAt(aPoint.mTextNode, aPoint.mOffset - 1);
    return outPoint;
  }

  if (idx) {
    outPoint.mTextNode = mNodeArray[idx - 1];

    uint32_t len = outPoint.mTextNode->TextLength();
    if (len) {
      outPoint.mOffset = len - 1;
      outPoint.mChar = GetCharAt(outPoint.mTextNode, len - 1);
    }
  }
  return outPoint;
}

nsresult
WSRunObject::ConvertToNBSP(WSPoint aPoint)
{
  // MOOSE: this routine needs to be modified to preserve the integrity of the
  // wsFragment info.
  NS_ENSURE_TRUE(aPoint.mTextNode, NS_ERROR_NULL_POINTER);

  // First, insert an nbsp
  AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
  nsAutoString nbspStr(nbsp);
  nsresult rv =
    mHTMLEditor->InsertTextIntoTextNodeImpl(nbspStr, *aPoint.mTextNode,
                                            aPoint.mOffset, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Next, find range of ws it will replace
  RefPtr<Text> startNode, endNode;
  int32_t startOffset = 0, endOffset = 0;

  GetAsciiWSBounds(eAfter, aPoint.mTextNode, aPoint.mOffset + 1,
                   getter_AddRefs(startNode), &startOffset,
                   getter_AddRefs(endNode), &endOffset);

  // Finally, delete that replaced ws, if any
  if (startNode) {
    rv = DeleteRange(EditorRawDOMPoint(startNode, startOffset),
                     EditorRawDOMPoint(endNode, endOffset));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

void
WSRunObject::GetAsciiWSBounds(int16_t aDir,
                              nsINode* aNode,
                              int32_t aOffset,
                              Text** outStartNode,
                              int32_t* outStartOffset,
                              Text** outEndNode,
                              int32_t* outEndOffset)
{
  MOZ_ASSERT(aNode && outStartNode && outStartOffset && outEndNode &&
             outEndOffset);

  RefPtr<Text> startNode, endNode;
  int32_t startOffset = 0, endOffset = 0;

  if (aDir & eAfter) {
    WSPoint point = GetNextCharPoint(EditorRawDOMPoint(aNode, aOffset));
    if (point.mTextNode) {
      // We found a text node, at least
      startNode = endNode = point.mTextNode;
      startOffset = endOffset = point.mOffset;

      // Scan ahead to end of ASCII ws
      for (; nsCRT::IsAsciiSpace(point.mChar) && point.mTextNode;
           point = GetNextCharPoint(point)) {
        endNode = point.mTextNode;
        // endOffset is _after_ ws
        point.mOffset++;
        endOffset = point.mOffset;
      }
    }
  }

  if (aDir & eBefore) {
    WSPoint point = GetPreviousCharPoint(EditorRawDOMPoint(aNode, aOffset));
    if (point.mTextNode) {
      // We found a text node, at least
      startNode = point.mTextNode;
      startOffset = point.mOffset + 1;
      if (!endNode) {
        endNode = startNode;
        endOffset = startOffset;
      }

      // Scan back to start of ASCII ws
      for (; nsCRT::IsAsciiSpace(point.mChar) && point.mTextNode;
           point = GetPreviousCharPoint(point)) {
        startNode = point.mTextNode;
        startOffset = point.mOffset;
      }
    }
  }

  startNode.forget(outStartNode);
  *outStartOffset = startOffset;
  endNode.forget(outEndNode);
  *outEndOffset = endOffset;
}

WSRunObject::WSFragment*
WSRunObject::FindNearestRun(const EditorRawDOMPoint& aPoint,
                            bool aForward)
{
  MOZ_ASSERT(aPoint.IsSetAndValid());

  for (WSFragment* run = mStartRun; run; run = run->mRight) {
    int32_t comp = run->mStartNode ?
      nsContentUtils::ComparePoints(aPoint, run->StartPoint()) : -1;
    if (comp <= 0) {
      // aPoint equals or before start of the run.  Return the run if we're
      // scanning forward, otherwise, nullptr.
      return aForward ? run : nullptr;
    }

    comp = run->mEndNode ?
      nsContentUtils::ComparePoints(aPoint, run->EndPoint()) : -1;
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

char16_t
WSRunObject::GetCharAt(Text* aTextNode,
                       int32_t aOffset)
{
  // return 0 if we can't get a char, for whatever reason
  NS_ENSURE_TRUE(aTextNode, 0);

  int32_t len = int32_t(aTextNode->TextLength());
  if (aOffset < 0 || aOffset >= len) {
    return 0;
  }
  return aTextNode->GetText()->CharAt(aOffset);
}

WSRunObject::WSPoint
WSRunObject::GetNextCharPointInternal(const EditorRawDOMPoint& aPoint)
{
  // Note: only to be called if aPoint.GetContainer() is not a ws node.

  // Binary search on wsnodes
  uint32_t numNodes = mNodeArray.Length();

  if (!numNodes) {
    // Do nothing if there are no nodes to search
    WSPoint outPoint;
    return outPoint;
  }

  // Begin binary search.  We do this because we need to minimize calls to
  // ComparePoints(), which is expensive.
  uint32_t firstNum = 0, curNum = numNodes / 2, lastNum = numNodes;
  while (curNum != lastNum) {
    RefPtr<Text> curNode = mNodeArray[curNum];
    int16_t cmp =
      nsContentUtils::ComparePoints(aPoint, EditorRawDOMPoint(curNode, 0));
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
    RefPtr<Text> textNode(mNodeArray[curNum - 1]);
    WSPoint point(textNode, textNode->TextLength(), 0);
    return GetNextCharPoint(point);
  }

  // The char after the point is the first character of our range.
  RefPtr<Text> textNode(mNodeArray[curNum]);
  WSPoint point(textNode, 0, 0);
  return GetNextCharPoint(point);
}

WSRunObject::WSPoint
WSRunObject::GetPreviousCharPointInternal(const EditorRawDOMPoint& aPoint)
{
  // Note: only to be called if aNode is not a ws node.

  // Binary search on wsnodes
  uint32_t numNodes = mNodeArray.Length();

  if (!numNodes) {
    // Do nothing if there are no nodes to search
    WSPoint outPoint;
    return outPoint;
  }

  uint32_t firstNum = 0, curNum = numNodes/2, lastNum = numNodes;
  int16_t cmp = 0;
  RefPtr<Text>  curNode;

  // Begin binary search.  We do this because we need to minimize calls to
  // ComparePoints(), which is expensive.
  while (curNum != lastNum) {
    curNode = mNodeArray[curNum];
    cmp = nsContentUtils::ComparePoints(aPoint, EditorRawDOMPoint(curNode, 0));
    if (cmp < 0) {
      lastNum = curNum;
    } else {
      firstNum = curNum + 1;
    }
    curNum = (lastNum - firstNum)/2 + firstNum;
    MOZ_ASSERT(firstNum <= curNum && curNum <= lastNum, "Bad binary search");
  }

  // When the binary search is complete, we always know that the current node
  // is the same as the end node, which is always past our range. Therefore,
  // we've found the node immediately after the point of interest.
  if (curNum == mNodeArray.Length()) {
    // Get the point before the end of the last node, we can pass the length of
    // the node into GetPreviousCharPoint(), and it will return the last
    // character.
    RefPtr<Text> textNode(mNodeArray[curNum - 1]);
    WSPoint point(textNode, textNode->TextLength(), 0);
    return GetPreviousCharPoint(point);
  }

  // We can just ask the current node for the point immediately before it,
  // it will handle moving to the previous node (if any) and returning the
  // appropriate character
  RefPtr<Text> textNode(mNodeArray[curNum]);
  WSPoint point(textNode, 0, 0);
  return GetPreviousCharPoint(point);
}

nsresult
WSRunObject::CheckTrailingNBSPOfRun(WSFragment *aRun)
{
  // Try to change an nbsp to a space, if possible, just to prevent nbsp
  // proliferation.  Examine what is before and after the trailing nbsp, if
  // any.
  NS_ENSURE_TRUE(aRun, NS_ERROR_NULL_POINTER);
  bool leftCheck = false;
  bool spaceNBSP = false;
  bool rightCheck = false;

  // confirm run is normalWS
  if (aRun->mType != WSType::normalWS) {
    return NS_ERROR_FAILURE;
  }

  // first check for trailing nbsp
  WSPoint thePoint = GetPreviousCharPoint(aRun->EndPoint());
  if (thePoint.mTextNode && thePoint.mChar == nbsp) {
    // now check that what is to the left of it is compatible with replacing nbsp with space
    WSPoint prevPoint = GetPreviousCharPoint(thePoint);
    if (prevPoint.mTextNode) {
      if (!nsCRT::IsAsciiSpace(prevPoint.mChar)) {
        leftCheck = true;
      } else {
        spaceNBSP = true;
      }
    } else if (aRun->mLeftType == WSType::text ||
               aRun->mLeftType == WSType::special) {
      leftCheck = true;
    }
    if (leftCheck || spaceNBSP) {
      // now check that what is to the right of it is compatible with replacing
      // nbsp with space
      if (aRun->mRightType == WSType::text ||
          aRun->mRightType == WSType::special ||
          aRun->mRightType == WSType::br) {
        rightCheck = true;
      }
      if ((aRun->mRightType & WSType::block) &&
          IsBlockNode(GetWSBoundingParent())) {
        // We are at a block boundary.  Insert a <br>.  Why?  Well, first note
        // that the br will have no visible effect since it is up against a
        // block boundary.  |foo<br><p>bar| renders like |foo<p>bar| and
        // similarly |<p>foo<br></p>bar| renders like |<p>foo</p>bar|.  What
        // this <br> addition gets us is the ability to convert a trailing nbsp
        // to a space.  Consider: |<body>foo. '</body>|, where ' represents
        // selection.  User types space attempting to put 2 spaces after the
        // end of their sentence.  We used to do this as: |<body>foo.
        // &nbsp</body>|  This caused problems with soft wrapping: the nbsp
        // would wrap to the next line, which looked attrocious.  If you try to
        // do: |<body>foo.&nbsp </body>| instead, the trailing space is
        // invisible because it is against a block boundary.  If you do:
        // |<body>foo.&nbsp&nbsp</body>| then you get an even uglier soft
        // wrapping problem, where foo is on one line until you type the final
        // space, and then "foo  " jumps down to the next line.  Ugh.  The best
        // way I can find out of this is to throw in a harmless <br> here,
        // which allows us to do: |<body>foo.&nbsp <br></body>|, which doesn't
        // cause foo to jump lines, doesn't cause spaces to show up at the
        // beginning of soft wrapped lines, and lets the user see 2 spaces when
        // they type 2 spaces.

        nsCOMPtr<Element> brNode =
          mHTMLEditor->CreateBR(aRun->mEndNode, aRun->mEndOffset);
        NS_ENSURE_TRUE(brNode, NS_ERROR_FAILURE);

        // Refresh thePoint, prevPoint
        thePoint = GetPreviousCharPoint(aRun->EndPoint());
        prevPoint = GetPreviousCharPoint(thePoint);
        rightCheck = true;
      }
    }
    if (leftCheck && rightCheck) {
      // Now replace nbsp with space.  First, insert a space
      AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
      nsAutoString spaceStr(char16_t(32));
      nsresult rv =
        mHTMLEditor->InsertTextIntoTextNodeImpl(spaceStr, *thePoint.mTextNode,
                                                thePoint.mOffset, true);
      NS_ENSURE_SUCCESS(rv, rv);

      // Finally, delete that nbsp
      rv = DeleteRange(EditorRawDOMPoint(thePoint.mTextNode,
                                         thePoint.mOffset + 1),
                       EditorRawDOMPoint(thePoint.mTextNode,
                                         thePoint.mOffset + 2));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (!mPRE && spaceNBSP && rightCheck) {
      // Don't mess with this preformatted for now.  We have a run of ASCII
      // whitespace (which will render as one space) followed by an nbsp (which
      // is at the end of the whitespace run).  Let's switch their order.  This
      // will ensure that if someone types two spaces after a sentence, and the
      // editor softwraps at this point, the spaces won't be split across lines,
      // which looks ugly and is bad for the moose.

      RefPtr<Text> startNode, endNode;
      int32_t startOffset, endOffset;
      GetAsciiWSBounds(eBoth, prevPoint.mTextNode, prevPoint.mOffset + 1,
                       getter_AddRefs(startNode), &startOffset,
                       getter_AddRefs(endNode), &endOffset);

      // Delete that nbsp
      nsresult rv = DeleteRange(EditorRawDOMPoint(thePoint.mTextNode,
                                                  thePoint.mOffset),
                                EditorRawDOMPoint(thePoint.mTextNode,
                                                  thePoint.mOffset + 1));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Finally, insert that nbsp before the ASCII ws run
      AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
      nsAutoString nbspStr(nbsp);
      rv = mHTMLEditor->InsertTextIntoTextNodeImpl(nbspStr, *startNode,
                                                   startOffset, true);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

nsresult
WSRunObject::ReplacePreviousNBSPIfUnncessary(WSFragment* aRun,
                                             const EditorRawDOMPoint& aPoint)
{
  if (NS_WARN_IF(!aRun) ||
      NS_WARN_IF(!aPoint.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }
  MOZ_ASSERT(aPoint.IsSetAndValid());

  // Try to change an NBSP to a space, if possible, just to prevent NBSP
  // proliferation.  This routine is called when we are about to make this
  // point in the ws abut an inserted break or text, so we don't have to worry
  // about what is after it.  What is after it now will end up after the
  // inserted object.
  bool canConvert = false;
  WSPoint thePoint = GetPreviousCharPoint(aPoint);
  if (thePoint.mTextNode && thePoint.mChar == nbsp) {
    WSPoint prevPoint = GetPreviousCharPoint(thePoint);
    if (prevPoint.mTextNode) {
      if (!nsCRT::IsAsciiSpace(prevPoint.mChar)) {
        // If previous character is a NBSP and its previous character isn't
        // ASCII space, we can replace the NBSP with ASCII space.
        canConvert = true;
      }
    } else if (aRun->mLeftType == WSType::text ||
               aRun->mLeftType == WSType::special) {
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
  nsAutoString spaceStr(char16_t(32));
  nsresult rv =
    mHTMLEditor->InsertTextIntoTextNodeImpl(spaceStr, *thePoint.mTextNode,
                                            thePoint.mOffset, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Finally, delete the previous NBSP.
  rv = DeleteRange(EditorRawDOMPoint(thePoint.mTextNode,
                                     thePoint.mOffset + 1),
                   EditorRawDOMPoint(thePoint.mTextNode,
                                     thePoint.mOffset + 2));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
WSRunObject::CheckLeadingNBSP(WSFragment* aRun,
                              nsINode* aNode,
                              int32_t aOffset)
{
  // Try to change an nbsp to a space, if possible, just to prevent nbsp
  // proliferation This routine is called when we are about to make this point
  // in the ws abut an inserted text, so we don't have to worry about what is
  // before it.  What is before it now will end up before the inserted text.
  bool canConvert = false;
  WSPoint thePoint = GetNextCharPoint(EditorRawDOMPoint(aNode, aOffset));
  if (thePoint.mChar == nbsp) {
    WSPoint tmp = thePoint;
    // we want to be after thePoint
    tmp.mOffset++;
    WSPoint nextPoint = GetNextCharPoint(tmp);
    if (nextPoint.mTextNode) {
      if (!nsCRT::IsAsciiSpace(nextPoint.mChar)) {
        canConvert = true;
      }
    } else if (aRun->mRightType == WSType::text ||
               aRun->mRightType == WSType::special ||
               aRun->mRightType == WSType::br) {
      canConvert = true;
    }
  }
  if (canConvert) {
    // First, insert a space
    AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
    nsAutoString spaceStr(char16_t(32));
    nsresult rv =
      mHTMLEditor->InsertTextIntoTextNodeImpl(spaceStr, *thePoint.mTextNode,
                                              thePoint.mOffset, true);
    NS_ENSURE_SUCCESS(rv, rv);

    // Finally, delete that nbsp
    rv = DeleteRange(EditorRawDOMPoint(thePoint.mTextNode,
                                       thePoint.mOffset + 1),
                     EditorRawDOMPoint(thePoint.mTextNode,
                                       thePoint.mOffset + 2));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}


nsresult
WSRunObject::Scrub()
{
  WSFragment *run = mStartRun;
  while (run) {
    if (run->mType & (WSType::leadingWS | WSType::trailingWS)) {
      nsresult rv = DeleteRange(run->StartPoint(), run->EndPoint());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    run = run->mRight;
  }
  return NS_OK;
}

bool
WSRunObject::IsBlockNode(nsINode* aNode)
{
  return aNode && aNode->IsElement() &&
         HTMLEditor::NodeIsBlockStatic(aNode->AsElement());
}

} // namespace mozilla
