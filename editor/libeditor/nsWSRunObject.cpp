/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/mozalloc.h"

#include "nsAString.h"
#include "nsAutoPtr.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsEditorUtils.h"
#include "nsError.h"
#include "nsHTMLEditor.h"
#include "nsIContent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsISupportsImpl.h"
#include "nsRange.h"
#include "nsSelectionState.h"
#include "nsString.h"
#include "nsTextEditUtils.h"
#include "nsTextFragment.h"
#include "nsWSRunObject.h"

using namespace mozilla;
using namespace mozilla::dom;

const char16_t nbsp = 160;

static bool IsBlockNode(nsINode* node)
{
  return node && node->IsElement() &&
         nsHTMLEditor::NodeIsBlockStatic(node->AsElement());
}

//- constructor / destructor -----------------------------------------------
nsWSRunObject::nsWSRunObject(nsHTMLEditor* aEd, nsINode* aNode, int32_t aOffset)
  : mNode(aNode)
  , mOffset(aOffset)
  , mPRE(false)
  , mStartNode()
  , mStartOffset(0)
  , mStartReason()
  , mStartReasonNode()
  , mEndNode()
  , mEndOffset(0)
  , mEndReason()
  , mEndReasonNode()
  , mFirstNBSPNode()
  , mFirstNBSPOffset(0)
  , mLastNBSPNode()
  , mLastNBSPOffset(0)
  , mNodeArray()
  , mStartRun(nullptr)
  , mEndRun(nullptr)
  , mHTMLEditor(aEd)
{
  GetWSNodes();
  GetRuns();
}

nsWSRunObject::nsWSRunObject(nsHTMLEditor *aEd, nsIDOMNode *aNode, int32_t aOffset) :
mNode(do_QueryInterface(aNode))
,mOffset(aOffset)
,mPRE(false)
,mStartNode()
,mStartOffset(0)
,mStartReason()
,mStartReasonNode()
,mEndNode()
,mEndOffset(0)
,mEndReason()
,mEndReasonNode()
,mFirstNBSPNode()
,mFirstNBSPOffset(0)
,mLastNBSPNode()
,mLastNBSPOffset(0)
,mNodeArray()
,mStartRun(nullptr)
,mEndRun(nullptr)
,mHTMLEditor(aEd)
{
  GetWSNodes();
  GetRuns();
}

nsWSRunObject::~nsWSRunObject()
{
  ClearRuns();
}



//--------------------------------------------------------------------------------------------
//   public static methods
//--------------------------------------------------------------------------------------------

nsresult
nsWSRunObject::ScrubBlockBoundary(nsHTMLEditor* aHTMLEd,
                                  BlockBoundary aBoundary,
                                  nsINode* aBlock,
                                  int32_t aOffset)
{
  NS_ENSURE_TRUE(aHTMLEd && aBlock, NS_ERROR_NULL_POINTER);

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
  
  nsWSRunObject theWSObj(aHTMLEd, aBlock, offset);
  return theWSObj.Scrub();
}

nsresult
nsWSRunObject::PrepareToJoinBlocks(nsHTMLEditor* aHTMLEd,
                                   Element* aLeftBlock,
                                   Element* aRightBlock)
{
  NS_ENSURE_TRUE(aLeftBlock && aRightBlock && aHTMLEd, NS_ERROR_NULL_POINTER);

  nsWSRunObject leftWSObj(aHTMLEd, aLeftBlock, aLeftBlock->Length());
  nsWSRunObject rightWSObj(aHTMLEd, aRightBlock, 0);

  return leftWSObj.PrepareToDeleteRangePriv(&rightWSObj);
}

nsresult
nsWSRunObject::PrepareToDeleteRange(nsHTMLEditor* aHTMLEd,
                                    nsCOMPtr<nsINode>* aStartNode,
                                    int32_t* aStartOffset,
                                    nsCOMPtr<nsINode>* aEndNode,
                                    int32_t* aEndOffset)
{
  NS_ENSURE_TRUE(aHTMLEd && aStartNode && *aStartNode && aStartOffset &&
                 aEndNode && *aEndNode && aEndOffset, NS_ERROR_NULL_POINTER);

  nsAutoTrackDOMPoint trackerStart(aHTMLEd->mRangeUpdater, aStartNode,
                                   aStartOffset);
  nsAutoTrackDOMPoint trackerEnd(aHTMLEd->mRangeUpdater, aEndNode, aEndOffset);

  nsWSRunObject leftWSObj(aHTMLEd, *aStartNode, *aStartOffset);
  nsWSRunObject rightWSObj(aHTMLEd, *aEndNode, *aEndOffset);

  return leftWSObj.PrepareToDeleteRangePriv(&rightWSObj);
}

nsresult
nsWSRunObject::PrepareToDeleteNode(nsHTMLEditor* aHTMLEd,
                                   nsIContent* aContent)
{
  NS_ENSURE_TRUE(aContent && aHTMLEd, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> parent = aContent->GetParentNode();
  NS_ENSURE_STATE(parent);
  int32_t offset = parent->IndexOf(aContent);

  nsWSRunObject leftWSObj(aHTMLEd, parent, offset);
  nsWSRunObject rightWSObj(aHTMLEd, parent, offset + 1);

  return leftWSObj.PrepareToDeleteRangePriv(&rightWSObj);
}

nsresult
nsWSRunObject::PrepareToSplitAcrossBlocks(nsHTMLEditor* aHTMLEd,
                                          nsCOMPtr<nsINode>* aSplitNode,
                                          int32_t* aSplitOffset)
{
  NS_ENSURE_TRUE(aHTMLEd && aSplitNode && *aSplitNode && aSplitOffset,
                 NS_ERROR_NULL_POINTER);

  nsAutoTrackDOMPoint tracker(aHTMLEd->mRangeUpdater, aSplitNode, aSplitOffset);

  nsWSRunObject wsObj(aHTMLEd, *aSplitNode, *aSplitOffset);

  return wsObj.PrepareToSplitAcrossBlocksPriv();
}

//--------------------------------------------------------------------------------------------
//   public instance methods
//--------------------------------------------------------------------------------------------

already_AddRefed<Element>
nsWSRunObject::InsertBreak(nsCOMPtr<nsINode>* aInOutParent,
                           int32_t* aInOutOffset,
                           nsIEditor::EDirection aSelect)
{
  // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
  // meanwhile, the pre case is handled in WillInsertText in
  // nsHTMLEditRules.cpp
  NS_ENSURE_TRUE(aInOutParent && aInOutOffset, nullptr);

  nsresult res = NS_OK;
  WSFragment *beforeRun, *afterRun;
  FindRun(*aInOutParent, *aInOutOffset, &beforeRun, false);
  FindRun(*aInOutParent, *aInOutOffset, &afterRun, true);

  {
    // Some scoping for nsAutoTrackDOMPoint.  This will track our insertion
    // point while we tweak any surrounding whitespace
    nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, aInOutParent,
                                aInOutOffset);

    // Handle any changes needed to ws run after inserted br
    if (!afterRun || (afterRun->mType & WSType::trailingWS)) {
      // Don't need to do anything.  Just insert break.  ws won't change.
    } else if (afterRun->mType & WSType::leadingWS) {
      // Delete the leading ws that is after insertion point.  We don't
      // have to (it would still not be significant after br), but it's
      // just more aesthetically pleasing to.
      res = DeleteChars(*aInOutParent, *aInOutOffset,
                        afterRun->mEndNode, afterRun->mEndOffset,
                        eOutsideUserSelectAll);
      NS_ENSURE_SUCCESS(res, nullptr);
    } else if (afterRun->mType == WSType::normalWS) {
      // Need to determine if break at front of non-nbsp run.  If so, convert
      // run to nbsp.
      WSPoint thePoint = GetCharAfter(*aInOutParent, *aInOutOffset);
      if (thePoint.mTextNode && nsCRT::IsAsciiSpace(thePoint.mChar)) {
        WSPoint prevPoint = GetCharBefore(thePoint);
        if (prevPoint.mTextNode && !nsCRT::IsAsciiSpace(prevPoint.mChar)) {
          // We are at start of non-nbsps.  Convert to a single nbsp.
          res = ConvertToNBSP(thePoint);
          NS_ENSURE_SUCCESS(res, nullptr);
        }
      }
    }

    // Handle any changes needed to ws run before inserted br
    if (!beforeRun || (beforeRun->mType & WSType::leadingWS)) {
      // Don't need to do anything.  Just insert break.  ws won't change.
    } else if (beforeRun->mType & WSType::trailingWS) {
      // Need to delete the trailing ws that is before insertion point, because it
      // would become significant after break inserted.
      res = DeleteChars(beforeRun->mStartNode, beforeRun->mStartOffset,
                        *aInOutParent, *aInOutOffset,
                        eOutsideUserSelectAll);
      NS_ENSURE_SUCCESS(res, nullptr);
    } else if (beforeRun->mType == WSType::normalWS) {
      // Try to change an nbsp to a space, just to prevent nbsp proliferation
      res = CheckTrailingNBSP(beforeRun, *aInOutParent, *aInOutOffset);
      NS_ENSURE_SUCCESS(res, nullptr);
    }
  }

  // ready, aim, fire!
  return mHTMLEditor->CreateBRImpl(aInOutParent, aInOutOffset, aSelect);
}

nsresult
nsWSRunObject::InsertText(const nsAString& aStringToInsert,
                          nsCOMPtr<nsINode>* aInOutParent,
                          int32_t* aInOutOffset,
                          nsIDocument* aDoc)
{
  // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
  // meanwhile, the pre case is handled in WillInsertText in
  // nsHTMLEditRules.cpp

  // MOOSE: for now, just getting the ws logic straight.  This implementation
  // is very slow.  Will need to replace edit rules impl with a more efficient
  // text sink here that does the minimal amount of searching/replacing/copying

  NS_ENSURE_TRUE(aInOutParent && aInOutOffset && aDoc, NS_ERROR_NULL_POINTER);

  if (aStringToInsert.IsEmpty()) {
    return NS_OK;
  }

  nsAutoString theString(aStringToInsert);

  WSFragment *beforeRun, *afterRun;
  FindRun(*aInOutParent, *aInOutOffset, &beforeRun, false);
  FindRun(*aInOutParent, *aInOutOffset, &afterRun, true);

  nsresult res;
  {
    // Some scoping for nsAutoTrackDOMPoint.  This will track our insertion
    // point while we tweak any surrounding whitespace
    nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, aInOutParent,
                                aInOutOffset);

    // Handle any changes needed to ws run after inserted text
    if (!afterRun || afterRun->mType & WSType::trailingWS) {
      // Don't need to do anything.  Just insert text.  ws won't change.
    } else if (afterRun->mType & WSType::leadingWS) {
      // Delete the leading ws that is after insertion point, because it
      // would become significant after text inserted.
      res = DeleteChars(*aInOutParent, *aInOutOffset, afterRun->mEndNode,
                        afterRun->mEndOffset, eOutsideUserSelectAll);
      NS_ENSURE_SUCCESS(res, res);
    } else if (afterRun->mType == WSType::normalWS) {
      // Try to change an nbsp to a space, if possible, just to prevent nbsp
      // proliferation
      res = CheckLeadingNBSP(afterRun, *aInOutParent, *aInOutOffset);
      NS_ENSURE_SUCCESS(res, res);
    }

    // Handle any changes needed to ws run before inserted text
    if (!beforeRun || beforeRun->mType & WSType::leadingWS) {
      // Don't need to do anything.  Just insert text.  ws won't change.
    } else if (beforeRun->mType & WSType::trailingWS) {
      // Need to delete the trailing ws that is before insertion point, because
      // it would become significant after text inserted.
      res = DeleteChars(beforeRun->mStartNode, beforeRun->mStartOffset,
                        *aInOutParent, *aInOutOffset, eOutsideUserSelectAll);
      NS_ENSURE_SUCCESS(res, res);
    } else if (beforeRun->mType == WSType::normalWS) {
      // Try to change an nbsp to a space, if possible, just to prevent nbsp
      // proliferation
      res = CheckTrailingNBSP(beforeRun, *aInOutParent, *aInOutOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
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
        WSPoint wspoint = GetCharBefore(*aInOutParent, *aInOutOffset);
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
        WSPoint wspoint = GetCharAfter(*aInOutParent, *aInOutOffset);
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
  nsCOMPtr<nsIDOMNode> parent(GetAsDOMNode(*aInOutParent));
  nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(aDoc));
  res = mHTMLEditor->InsertTextImpl(theString, address_of(parent),
                                    aInOutOffset, doc);
  *aInOutParent = do_QueryInterface(parent);
  return NS_OK;
}

nsresult
nsWSRunObject::DeleteWSBackward()
{
  WSPoint point = GetCharBefore(mNode, mOffset);
  NS_ENSURE_TRUE(point.mTextNode, NS_OK);  // nothing to delete

  if (mPRE) {
    // easy case, preformatted ws
    if (nsCRT::IsAsciiSpace(point.mChar) || point.mChar == nbsp) {
      return DeleteChars(point.mTextNode, point.mOffset,
                         point.mTextNode, point.mOffset + 1);
    }
  }

  // Caller's job to ensure that previous char is really ws.  If it is normal
  // ws, we need to delete the whole run.
  if (nsCRT::IsAsciiSpace(point.mChar)) {
    nsRefPtr<Text> startNodeText, endNodeText;
    int32_t startOffset, endOffset;
    GetAsciiWSBounds(eBoth, point.mTextNode, point.mOffset + 1,
                     getter_AddRefs(startNodeText), &startOffset,
                     getter_AddRefs(endNodeText), &endOffset);

    // adjust surrounding ws
    nsCOMPtr<nsINode> startNode = startNodeText.get();
    nsCOMPtr<nsINode> endNode = endNodeText.get();
    nsresult res =
      nsWSRunObject::PrepareToDeleteRange(mHTMLEditor,
                                          address_of(startNode), &startOffset,
                                          address_of(endNode), &endOffset);
    NS_ENSURE_SUCCESS(res, res);

    // finally, delete that ws
    return DeleteChars(startNode, startOffset, endNode, endOffset);
  } else if (point.mChar == nbsp) {
    nsCOMPtr<nsINode> node(point.mTextNode);
    // adjust surrounding ws
    int32_t startOffset = point.mOffset;
    int32_t endOffset = point.mOffset + 1;
    nsresult res =
      nsWSRunObject::PrepareToDeleteRange(mHTMLEditor,
                                          address_of(node), &startOffset,
                                          address_of(node), &endOffset);
    NS_ENSURE_SUCCESS(res, res);

    // finally, delete that ws
    return DeleteChars(node, startOffset, node, endOffset);
  }
  return NS_OK;
}

nsresult
nsWSRunObject::DeleteWSForward()
{
  WSPoint point = GetCharAfter(mNode, mOffset);
  NS_ENSURE_TRUE(point.mTextNode, NS_OK); // nothing to delete

  if (mPRE) {
    // easy case, preformatted ws
    if (nsCRT::IsAsciiSpace(point.mChar) || point.mChar == nbsp) {
      return DeleteChars(point.mTextNode, point.mOffset,
                         point.mTextNode, point.mOffset + 1);
    }
  }

  // Caller's job to ensure that next char is really ws.  If it is normal ws,
  // we need to delete the whole run.
  if (nsCRT::IsAsciiSpace(point.mChar)) {
    nsRefPtr<Text> startNodeText, endNodeText;
    int32_t startOffset, endOffset;
    GetAsciiWSBounds(eBoth, point.mTextNode, point.mOffset + 1,
                     getter_AddRefs(startNodeText), &startOffset,
                     getter_AddRefs(endNodeText), &endOffset);

    // Adjust surrounding ws
    nsCOMPtr<nsINode> startNode(startNodeText), endNode(endNodeText);
    nsresult res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor,
        address_of(startNode), &startOffset, address_of(endNode), &endOffset);
    NS_ENSURE_SUCCESS(res, res);

    // Finally, delete that ws
    return DeleteChars(startNode, startOffset, endNode, endOffset);
  } else if (point.mChar == nbsp) {
    nsCOMPtr<nsINode> node(point.mTextNode);
    // Adjust surrounding ws
    int32_t startOffset = point.mOffset;
    int32_t endOffset = point.mOffset+1;
    nsresult res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor,
        address_of(node), &startOffset, address_of(node), &endOffset);
    NS_ENSURE_SUCCESS(res, res);

    // Finally, delete that ws
    return DeleteChars(node, startOffset, node, endOffset);
  }
  return NS_OK;
}

void
nsWSRunObject::PriorVisibleNode(nsINode* aNode,
                                int32_t aOffset,
                                nsCOMPtr<nsINode>* outVisNode,
                                int32_t* outVisOffset,
                                WSType* outType)
{
  // Find first visible thing before the point.  Position
  // outVisNode/outVisOffset just _after_ that thing.  If we don't find
  // anything return start of ws.
  MOZ_ASSERT(aNode && outVisNode && outVisOffset && outType);

  WSFragment* run;
  FindRun(aNode, aOffset, &run, false);

  // Is there a visible run there or earlier?
  for (; run; run = run->mLeft) {
    if (run->mType == WSType::normalWS) {
      WSPoint point = GetCharBefore(aNode, aOffset);
      if (point.mTextNode) {
        *outVisNode = point.mTextNode;
        *outVisOffset = point.mOffset + 1;
        if (nsCRT::IsAsciiSpace(point.mChar) || point.mChar == nbsp) {
          *outType = WSType::normalWS;
        } else if (!point.mChar) {
          // MOOSE: not possible?
          *outType = WSType::none;
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
nsWSRunObject::NextVisibleNode(nsINode* aNode,
                               int32_t aOffset,
                               nsCOMPtr<nsINode>* outVisNode,
                               int32_t* outVisOffset,
                               WSType* outType)
{
  // Find first visible thing after the point.  Position
  // outVisNode/outVisOffset just _before_ that thing.  If we don't find
  // anything return end of ws.
  MOZ_ASSERT(aNode && outVisNode && outVisOffset && outType);

  WSFragment* run;
  FindRun(aNode, aOffset, &run, true);

  // Is there a visible run there or later?
  for (; run; run = run->mRight) {
    if (run->mType == WSType::normalWS) {
      WSPoint point = GetCharAfter(aNode, aOffset);
      if (point.mTextNode) {
        *outVisNode = point.mTextNode;
        *outVisOffset = point.mOffset;
        if (nsCRT::IsAsciiSpace(point.mChar) || point.mChar == nbsp) {
          *outType = WSType::normalWS;
        } else if (!point.mChar) {
          // MOOSE: not possible?
          *outType = WSType::none;
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
nsWSRunObject::AdjustWhitespace()
{
  // this routine examines a run of ws and tries to get rid of some unneeded nbsp's,
  // replacing them with regualr ascii space if possible.  Keeping things simple
  // for now and just trying to fix up the trailing ws in the run.
  if (!mLastNBSPNode) {
    // nothing to do!
    return NS_OK;
  }
  nsresult res = NS_OK;
  WSFragment *curRun = mStartRun;
  while (curRun)
  {
    // look for normal ws run
    if (curRun->mType == WSType::normalWS) {
      res = CheckTrailingNBSPOfRun(curRun);
      break;
    }
    curRun = curRun->mRight;
  }
  return res;
}


//--------------------------------------------------------------------------------------------
//   protected methods
//--------------------------------------------------------------------------------------------

already_AddRefed<nsINode>
nsWSRunObject::GetWSBoundingParent()
{
  NS_ENSURE_TRUE(mNode, nullptr);
  nsCOMPtr<nsINode> wsBoundingParent = mNode;
  while (!IsBlockNode(wsBoundingParent)) {
    nsCOMPtr<nsINode> parent = wsBoundingParent->GetParentNode();
    if (!parent || !mHTMLEditor->IsEditable(parent)) {
      break;
    }
    wsBoundingParent.swap(parent);
  }
  return wsBoundingParent.forget();
}

nsresult
nsWSRunObject::GetWSNodes()
{
  // collect up an array of nodes that are contiguous with the insertion point
  // and which contain only whitespace.  Stop if you reach non-ws text or a new 
  // block boundary.
  nsresult res = NS_OK;
  
  ::DOMPoint start(mNode, mOffset), end(mNode, mOffset);
  nsCOMPtr<nsINode> wsBoundingParent = GetWSBoundingParent();

  // first look backwards to find preceding ws nodes
  if (nsRefPtr<Text> textNode = mNode->GetAsText()) {
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
        start.node = textNode;
        start.offset = pos;
      }
    }
  }

  while (!mStartNode) {
    // we haven't found the start of ws yet.  Keep looking
    nsCOMPtr<nsINode> priorNode;
    res = GetPreviousWSNode(start, wsBoundingParent, address_of(priorNode));
    NS_ENSURE_SUCCESS(res, res);
    if (priorNode) {
      if (IsBlockNode(priorNode)) {
        mStartNode = start.node;
        mStartOffset = start.offset;
        mStartReason = WSType::otherBlock;
        mStartReasonNode = priorNode;
      } else if (nsRefPtr<Text> textNode = priorNode->GetAsText()) {
        mNodeArray.InsertElementAt(0, textNode);
        const nsTextFragment *textFrag;
        if (!textNode || !(textFrag = textNode->GetText())) {
          return NS_ERROR_NULL_POINTER;
        }
        uint32_t len = textNode->TextLength();

        if (len < 1) {
          // Zero length text node. Set start point to it
          // so we can get past it!
          start.SetPoint(priorNode, 0);
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
            start.SetPoint(textNode, pos);
          }
        }
      } else {
        // it's a break or a special node, like <img>, that is not a block and not
        // a break but still serves as a terminator to ws runs.
        mStartNode = start.node;
        mStartOffset = start.offset;
        if (nsTextEditUtils::IsBreak(priorNode)) {
          mStartReason = WSType::br;
        } else {
          mStartReason = WSType::special;
        }
        mStartReasonNode = priorNode;
      }
    } else {
      // no prior node means we exhausted wsBoundingParent
      mStartNode = start.node;
      mStartOffset = start.offset;
      mStartReason = WSType::thisBlock;
      mStartReasonNode = wsBoundingParent;
    } 
  }
  
  // then look ahead to find following ws nodes
  if (nsRefPtr<Text> textNode = mNode->GetAsText()) {
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
        end.SetPoint(textNode, pos + 1);
      }
    }
  }

  while (!mEndNode) {
    // we haven't found the end of ws yet.  Keep looking
    nsCOMPtr<nsINode> nextNode;
    res = GetNextWSNode(end, wsBoundingParent, address_of(nextNode));
    NS_ENSURE_SUCCESS(res, res);
    if (nextNode) {
      if (IsBlockNode(nextNode)) {
        // we encountered a new block.  therefore no more ws.
        mEndNode = end.node;
        mEndOffset = end.offset;
        mEndReason = WSType::otherBlock;
        mEndReasonNode = nextNode;
      } else if (nsRefPtr<Text> textNode = nextNode->GetAsText()) {
        mNodeArray.AppendElement(textNode);
        const nsTextFragment *textFrag;
        if (!textNode || !(textFrag = textNode->GetText())) {
          return NS_ERROR_NULL_POINTER;
        }
        uint32_t len = textNode->TextLength();

        if (len < 1) {
          // Zero length text node. Set end point to it
          // so we can get past it!
          end.SetPoint(textNode, 0);
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
            end.SetPoint(textNode, pos + 1);
          }
        }
      } else {
        // we encountered a break or a special node, like <img>, 
        // that is not a block and not a break but still 
        // serves as a terminator to ws runs.
        mEndNode = end.node;
        mEndOffset = end.offset;
        if (nsTextEditUtils::IsBreak(nextNode)) {
          mEndReason = WSType::br;
        } else {
          mEndReason = WSType::special;
        }
        mEndReasonNode = nextNode;
      }
    } else {
      // no next node means we exhausted wsBoundingParent
      mEndNode = end.node;
      mEndOffset = end.offset;
      mEndReason = WSType::thisBlock;
      mEndReasonNode = wsBoundingParent;
    } 
  }

  return NS_OK;
}

void
nsWSRunObject::GetRuns()
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
    }
    else
    {
      // we might have trailing ws.
      // it so happens that *if* there is an nbsp at end, {mEndNode,mEndOffset-1}
      // will point to it, even though in general start/end points not
      // guaranteed to be in text nodes.
      if ((mLastNBSPNode == mEndNode) && (mLastNBSPOffset == (mEndOffset-1)))
      {
        // normal ws runs right up to adjacent block (nbsp next to block)
        normalRun->mRightType = mEndReason;
        normalRun->mEndNode   = mEndNode;
        normalRun->mEndOffset = mEndOffset;
        mEndRun = normalRun;
      }
      else
      {
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
    if ((mLastNBSPNode == mEndNode) && (mLastNBSPOffset == (mEndOffset-1)))
    {
      mStartRun->mRightType = mEndReason;
      mStartRun->mEndNode   = mEndNode;
      mStartRun->mEndOffset = mEndOffset;
      mEndRun = mStartRun;
    }
    else
    {
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
nsWSRunObject::ClearRuns()
{
  WSFragment *tmp, *run;
  run = mStartRun;
  while (run)
  {
    tmp = run->mRight;
    delete run;
    run = tmp;
  }
  mStartRun = 0;
  mEndRun = 0;
}

void
nsWSRunObject::MakeSingleWSRun(WSType aType)
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

nsresult 
nsWSRunObject::GetPreviousWSNodeInner(nsINode* aStartNode,
                                      nsINode* aBlockParent,
                                      nsCOMPtr<nsINode>* aPriorNode)
{
  // can't really recycle various getnext/prior routines because we have
  // special needs here.  Need to step into inline containers but not block
  // containers.
  NS_ENSURE_TRUE(aStartNode && aBlockParent && aPriorNode, NS_ERROR_NULL_POINTER);
  
  *aPriorNode = aStartNode->GetPreviousSibling();
  nsCOMPtr<nsINode> temp, curNode(aStartNode);
  while (!*aPriorNode) {
    // we have exhausted nodes in parent of aStartNode.
    temp = curNode->GetParentNode();
    NS_ENSURE_TRUE(temp, NS_ERROR_NULL_POINTER);
    if (temp == aBlockParent) {
      // we have exhausted nodes in the block parent.  The convention here is
      // to return null.
      *aPriorNode = nullptr;
      return NS_OK;
    }
    // we have a parent: look for previous sibling
    *aPriorNode = temp->GetPreviousSibling();
    curNode = temp;
  }
  // we have a prior node.  If it's a block, return it.
  if (IsBlockNode(*aPriorNode)) {
    return NS_OK;
  } else if (mHTMLEditor->IsContainer(*aPriorNode)) {
    // else if it's a container, get deep rightmost child
    temp = mHTMLEditor->GetRightmostChild(*aPriorNode);
    if (temp) {
      *aPriorNode = temp;
    }
    return NS_OK;
  }
  // else return the node itself
  return NS_OK;
}

nsresult 
nsWSRunObject::GetPreviousWSNode(::DOMPoint aPoint,
                                 nsINode* aBlockParent,
                                 nsCOMPtr<nsINode>* aPriorNode)
{
  // can't really recycle various getnext/prior routines because we
  // have special needs here.  Need to step into inline containers but
  // not block containers.
  NS_ENSURE_TRUE(aPoint.node && aBlockParent && aPriorNode,
                 NS_ERROR_NULL_POINTER);
  *aPriorNode = nullptr;

  if (aPoint.node->NodeType() == nsIDOMNode::TEXT_NODE) {
    return GetPreviousWSNodeInner(aPoint.node, aBlockParent, aPriorNode);
  }
  if (!mHTMLEditor->IsContainer(aPoint.node)) {
    return GetPreviousWSNodeInner(aPoint.node, aBlockParent, aPriorNode);
  }
  
  if (!aPoint.offset) {
    if (aPoint.node == aBlockParent) {
      // we are at start of the block.
      return NS_OK;
    }

    // we are at start of non-block container
    return GetPreviousWSNodeInner(aPoint.node, aBlockParent, aPriorNode);
  }

  nsCOMPtr<nsIContent> startContent(do_QueryInterface(aPoint.node));
  NS_ENSURE_STATE(startContent);
  nsIContent* priorContent = startContent->GetChildAt(aPoint.offset - 1);
  NS_ENSURE_TRUE(priorContent, NS_ERROR_NULL_POINTER);
  *aPriorNode = priorContent;
  // we have a prior node.  If it's a block, return it.
  if (IsBlockNode(*aPriorNode)) {
    return NS_OK;
  } else if (mHTMLEditor->IsContainer(*aPriorNode)) {
    // else if it's a container, get deep rightmost child
    nsCOMPtr<nsINode> temp;
    temp = mHTMLEditor->GetRightmostChild(*aPriorNode);
    if (temp) {
      *aPriorNode = temp;
    }
    return NS_OK;
  }
  // else return the node itself
  return NS_OK;
}

nsresult 
nsWSRunObject::GetNextWSNodeInner(nsINode* aStartNode,
                                  nsINode* aBlockParent,
                                  nsCOMPtr<nsINode>* aNextNode)
{
  // can't really recycle various getnext/prior routines because we have
  // special needs here.  Need to step into inline containers but not block
  // containers.
  NS_ENSURE_TRUE(aStartNode && aBlockParent && aNextNode,
                 NS_ERROR_NULL_POINTER);
  
  *aNextNode = aStartNode->GetNextSibling();
  nsCOMPtr<nsINode> temp, curNode(aStartNode);
  while (!*aNextNode) {
    // we have exhausted nodes in parent of aStartNode.
    temp = curNode->GetParentNode();
    NS_ENSURE_TRUE(temp, NS_ERROR_NULL_POINTER);
    if (temp == aBlockParent) {
      // we have exhausted nodes in the block parent.  The convention here is
      // to return null.
      *aNextNode = nullptr;
      return NS_OK;
    }
    // we have a parent: look for next sibling
    *aNextNode = temp->GetNextSibling();
    curNode = temp;
  }
  // we have a next node.  If it's a block, return it.
  if (IsBlockNode(*aNextNode)) {
    return NS_OK;
  } else if (mHTMLEditor->IsContainer(*aNextNode)) {
    // else if it's a container, get deep leftmost child
    temp = mHTMLEditor->GetLeftmostChild(*aNextNode);
    if (temp) {
      *aNextNode = temp;
    }
    return NS_OK;
  }
  // else return the node itself
  return NS_OK;
}

nsresult 
nsWSRunObject::GetNextWSNode(::DOMPoint aPoint,
                             nsINode* aBlockParent,
                             nsCOMPtr<nsINode>* aNextNode)
{
  // can't really recycle various getnext/prior routines because we have
  // special needs here.  Need to step into inline containers but not block
  // containers.
  NS_ENSURE_TRUE(aPoint.node && aBlockParent && aNextNode,
                 NS_ERROR_NULL_POINTER);
  *aNextNode = nullptr;

  if (aPoint.node->NodeType() == nsIDOMNode::TEXT_NODE) {
    return GetNextWSNodeInner(aPoint.node, aBlockParent, aNextNode);
  }
  if (!mHTMLEditor->IsContainer(aPoint.node)) {
    return GetNextWSNodeInner(aPoint.node, aBlockParent, aNextNode);
  }
  
  nsCOMPtr<nsIContent> startContent(do_QueryInterface(aPoint.node));
  NS_ENSURE_STATE(startContent);
  nsIContent *nextContent = startContent->GetChildAt(aPoint.offset);
  if (!nextContent) {
    if (aPoint.node == aBlockParent) {
      // we are at end of the block.
      return NS_OK;
    }

    // we are at end of non-block container
    return GetNextWSNodeInner(aPoint.node, aBlockParent, aNextNode);
  }
  
  *aNextNode = nextContent;
  // we have a next node.  If it's a block, return it.
  if (IsBlockNode(*aNextNode)) {
    return NS_OK;
  } else if (mHTMLEditor->IsContainer(*aNextNode)) {
    // else if it's a container, get deep leftmost child
    nsCOMPtr<nsINode> temp = mHTMLEditor->GetLeftmostChild(*aNextNode);
    if (temp) {
      *aNextNode = temp;
    }
    return NS_OK;
  }
  // else return the node itself
  return NS_OK;
}

nsresult 
nsWSRunObject::PrepareToDeleteRangePriv(nsWSRunObject* aEndObject)
{
  // this routine adjust whitespace before *this* and after aEndObject
  // in preperation for the two areas to become adjacent after the 
  // intervening content is deleted.  It's overly agressive right
  // now.  There might be a block boundary remaining between them after
  // the deletion, in which case these adjstments are unneeded (though
  // I don't think they can ever be harmful?)
  
  NS_ENSURE_TRUE(aEndObject, NS_ERROR_NULL_POINTER);
  nsresult res = NS_OK;
  
  // get the runs before and after selection
  WSFragment *beforeRun, *afterRun;
  FindRun(mNode, mOffset, &beforeRun, false);
  aEndObject->FindRun(aEndObject->mNode, aEndObject->mOffset, &afterRun, true);
  
  // trim after run of any leading ws
  if (afterRun && (afterRun->mType & WSType::leadingWS)) {
    res = aEndObject->DeleteChars(aEndObject->mNode, aEndObject->mOffset,
                                  afterRun->mEndNode, afterRun->mEndOffset,
                                  eOutsideUserSelectAll);
    NS_ENSURE_SUCCESS(res, res);
  }
  // adjust normal ws in afterRun if needed
  if (afterRun && afterRun->mType == WSType::normalWS && !aEndObject->mPRE) {
    if ((beforeRun && (beforeRun->mType & WSType::leadingWS)) ||
        (!beforeRun && ((mStartReason & WSType::block) ||
                        mStartReason == WSType::br))) {
      // make sure leading char of following ws is an nbsp, so that it will show up
      WSPoint point = aEndObject->GetCharAfter(aEndObject->mNode,
                                               aEndObject->mOffset);
      if (point.mTextNode && nsCRT::IsAsciiSpace(point.mChar))
      {
        res = aEndObject->ConvertToNBSP(point, eOutsideUserSelectAll);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  // trim before run of any trailing ws
  if (beforeRun && (beforeRun->mType & WSType::trailingWS)) {
    res = DeleteChars(beforeRun->mStartNode, beforeRun->mStartOffset,
                      mNode, mOffset, eOutsideUserSelectAll);
    NS_ENSURE_SUCCESS(res, res);
  } else if (beforeRun && beforeRun->mType == WSType::normalWS && !mPRE) {
    if ((afterRun && (afterRun->mType & WSType::trailingWS)) ||
        (afterRun && afterRun->mType == WSType::normalWS) ||
        (!afterRun && (aEndObject->mEndReason & WSType::block))) {
      // make sure trailing char of starting ws is an nbsp, so that it will show up
      WSPoint point = GetCharBefore(mNode, mOffset);
      if (point.mTextNode && nsCRT::IsAsciiSpace(point.mChar))
      {
        nsRefPtr<Text> wsStartNode, wsEndNode;
        int32_t wsStartOffset, wsEndOffset;
        GetAsciiWSBounds(eBoth, mNode, mOffset,
                         getter_AddRefs(wsStartNode), &wsStartOffset,
                         getter_AddRefs(wsEndNode), &wsEndOffset);
        point.mTextNode = wsStartNode;
        point.mOffset = wsStartOffset;
        res = ConvertToNBSP(point, eOutsideUserSelectAll);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  return res;
}

nsresult 
nsWSRunObject::PrepareToSplitAcrossBlocksPriv()
{
  // used to prepare ws to be split across two blocks.  The main issue 
  // here is make sure normalWS doesn't end up becoming non-significant
  // leading or trailing ws after the split.
  nsresult res = NS_OK;
  
  // get the runs before and after selection
  WSFragment *beforeRun, *afterRun;
  FindRun(mNode, mOffset, &beforeRun, false);
  FindRun(mNode, mOffset, &afterRun, true);
  
  // adjust normal ws in afterRun if needed
  if (afterRun && afterRun->mType == WSType::normalWS) {
    // make sure leading char of following ws is an nbsp, so that it will show up
    WSPoint point = GetCharAfter(mNode, mOffset);
    if (point.mTextNode && nsCRT::IsAsciiSpace(point.mChar))
    {
      res = ConvertToNBSP(point);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  // adjust normal ws in beforeRun if needed
  if (beforeRun && beforeRun->mType == WSType::normalWS) {
    // make sure trailing char of starting ws is an nbsp, so that it will show up
    WSPoint point = GetCharBefore(mNode, mOffset);
    if (point.mTextNode && nsCRT::IsAsciiSpace(point.mChar))
    {
      nsRefPtr<Text> wsStartNode, wsEndNode;
      int32_t wsStartOffset, wsEndOffset;
      GetAsciiWSBounds(eBoth, mNode, mOffset,
                       getter_AddRefs(wsStartNode), &wsStartOffset,
                       getter_AddRefs(wsEndNode), &wsEndOffset);
      point.mTextNode = wsStartNode;
      point.mOffset = wsStartOffset;
      res = ConvertToNBSP(point);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return res;
}

nsresult
nsWSRunObject::DeleteChars(nsINode* aStartNode, int32_t aStartOffset,
                           nsINode* aEndNode, int32_t aEndOffset,
                           AreaRestriction aAR)
{
  // MOOSE: this routine needs to be modified to preserve the integrity of the
  // wsFragment info.
  NS_ENSURE_TRUE(aStartNode && aEndNode, NS_ERROR_NULL_POINTER);

  if (aAR == eOutsideUserSelectAll) {
    nsCOMPtr<nsIDOMNode> san =
      mHTMLEditor->FindUserSelectAllNode(GetAsDOMNode(aStartNode));
    if (san) {
      return NS_OK;
    }

    if (aStartNode != aEndNode) {
      san = mHTMLEditor->FindUserSelectAllNode(GetAsDOMNode(aEndNode));
      if (san) {
        return NS_OK;
      }
    }
  }

  if (aStartNode == aEndNode && aStartOffset == aEndOffset) {
    // Nothing to delete
    return NS_OK;
  }

  int32_t idx = mNodeArray.IndexOf(aStartNode);
  if (idx == -1) {
    // If our strarting point wasn't one of our ws text nodes, then just go
    // through them from the beginning.
    idx = 0;
  }

  if (aStartNode == aEndNode && aStartNode->GetAsText()) {
    return mHTMLEditor->DeleteText(*aStartNode->GetAsText(),
        static_cast<uint32_t>(aStartOffset),
        static_cast<uint32_t>(aEndOffset - aStartOffset));
  }

  nsresult res;
  nsRefPtr<nsRange> range;
  int32_t count = mNodeArray.Length();
  for (; idx < count; idx++) {
    nsRefPtr<Text> node = mNodeArray[idx];
    if (!node) {
      // We ran out of ws nodes; must have been deleting to end
      return NS_OK;
    }
    if (node == aStartNode) {
      uint32_t len = node->Length();
      if (uint32_t(aStartOffset) < len) {
        res = mHTMLEditor->DeleteText(*node,
                                      AssertedCast<uint32_t>(aStartOffset),
                                      len - aStartOffset);
        NS_ENSURE_SUCCESS(res, res);
      }
    } else if (node == aEndNode) {
      if (aEndOffset) {
        res = mHTMLEditor->DeleteText(*node, 0,
                                      AssertedCast<uint32_t>(aEndOffset));
        NS_ENSURE_SUCCESS(res, res);
      }
      break;
    } else {
      if (!range) {
        range = new nsRange(aStartNode);
        res = range->Set(aStartNode, aStartOffset, aEndNode, aEndOffset);
        NS_ENSURE_SUCCESS(res, res);
      }
      bool nodeBefore, nodeAfter;
      res = nsRange::CompareNodeToRange(node, range, &nodeBefore, &nodeAfter);
      NS_ENSURE_SUCCESS(res, res);
      if (nodeAfter) {
        break;
      }
      if (!nodeBefore) {
        res = mHTMLEditor->DeleteNode(node);
        NS_ENSURE_SUCCESS(res, res);
        mNodeArray.RemoveElement(node);
        --count;
        --idx;
      }
    }
  }
  return NS_OK;
}

nsWSRunObject::WSPoint
nsWSRunObject::GetCharAfter(nsINode* aNode, int32_t aOffset)
{
  MOZ_ASSERT(aNode);

  int32_t idx = mNodeArray.IndexOf(aNode);
  if (idx == -1) {
    // Use range comparisons to get right ws node
    return GetWSPointAfter(aNode, aOffset);
  } else {
    // Use WSPoint version of GetCharAfter()
    return GetCharAfter(WSPoint(mNodeArray[idx], aOffset, 0));
  }
}

nsWSRunObject::WSPoint
nsWSRunObject::GetCharBefore(nsINode* aNode, int32_t aOffset)
{
  MOZ_ASSERT(aNode);

  int32_t idx = mNodeArray.IndexOf(aNode);
  if (idx == -1) {
    // Use range comparisons to get right ws node
    return GetWSPointBefore(aNode, aOffset);
  } else {
    // Use WSPoint version of GetCharBefore()
    return GetCharBefore(WSPoint(mNodeArray[idx], aOffset, 0));
  }
}

nsWSRunObject::WSPoint
nsWSRunObject::GetCharAfter(const WSPoint &aPoint)
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
  int32_t numNodes = mNodeArray.Length();

  if (uint16_t(aPoint.mOffset) < aPoint.mTextNode->TextLength()) {
    outPoint = aPoint;
    outPoint.mChar = GetCharAt(aPoint.mTextNode, aPoint.mOffset);
    return outPoint;
  } else if (idx + 1 < numNodes) {
    outPoint.mTextNode = mNodeArray[idx + 1];
    MOZ_ASSERT(outPoint.mTextNode);
    outPoint.mOffset = 0;
    outPoint.mChar = GetCharAt(outPoint.mTextNode, 0);
  }
  return outPoint;
}

nsWSRunObject::WSPoint
nsWSRunObject::GetCharBefore(const WSPoint &aPoint)
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

  if (aPoint.mOffset != 0) {
    outPoint = aPoint;
    outPoint.mOffset--;
    outPoint.mChar = GetCharAt(aPoint.mTextNode, aPoint.mOffset - 1);
    return outPoint;
  } else if (idx) {
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
nsWSRunObject::ConvertToNBSP(WSPoint aPoint, AreaRestriction aAR)
{
  // MOOSE: this routine needs to be modified to preserve the integrity of the
  // wsFragment info.
  NS_ENSURE_TRUE(aPoint.mTextNode, NS_ERROR_NULL_POINTER);

  if (aAR == eOutsideUserSelectAll) {
    nsCOMPtr<nsIDOMNode> san =
      mHTMLEditor->FindUserSelectAllNode(GetAsDOMNode(aPoint.mTextNode));
    if (san) {
      return NS_OK;
    }
  }

  // First, insert an nbsp
  nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
  nsAutoString nbspStr(nbsp);
  nsresult res = mHTMLEditor->InsertTextIntoTextNodeImpl(nbspStr,
      aPoint.mTextNode, aPoint.mOffset, true);
  NS_ENSURE_SUCCESS(res, res);

  // Next, find range of ws it will replace
  nsRefPtr<Text> startNode, endNode;
  int32_t startOffset = 0, endOffset = 0;

  GetAsciiWSBounds(eAfter, aPoint.mTextNode, aPoint.mOffset + 1,
                   getter_AddRefs(startNode), &startOffset,
                   getter_AddRefs(endNode), &endOffset);

  // Finally, delete that replaced ws, if any
  if (startNode) {
    res = DeleteChars(startNode, startOffset, endNode, endOffset);
    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}

void
nsWSRunObject::GetAsciiWSBounds(int16_t aDir, nsINode* aNode, int32_t aOffset,
                                Text** outStartNode, int32_t* outStartOffset,
                                Text** outEndNode, int32_t* outEndOffset)
{
  MOZ_ASSERT(aNode && outStartNode && outStartOffset && outEndNode &&
             outEndOffset);

  nsRefPtr<Text> startNode, endNode;
  int32_t startOffset = 0, endOffset = 0;

  if (aDir & eAfter) {
    WSPoint point = GetCharAfter(aNode, aOffset);
    if (point.mTextNode) {
      // We found a text node, at least
      startNode = endNode = point.mTextNode;
      startOffset = endOffset = point.mOffset;

      // Scan ahead to end of ASCII ws
      for (; nsCRT::IsAsciiSpace(point.mChar) && point.mTextNode;
           point = GetCharAfter(point)) {
        endNode = point.mTextNode;
        // endOffset is _after_ ws
        point.mOffset++;
        endOffset = point.mOffset;
      }
    }
  }

  if (aDir & eBefore) {
    WSPoint point = GetCharBefore(aNode, aOffset);
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
           point = GetCharBefore(point)) {
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

/**
 * Given a dompoint, find the ws run that is before or after it, as caller
 * needs
 */
void
nsWSRunObject::FindRun(nsINode* aNode, int32_t aOffset, WSFragment** outRun,
                       bool after)
{
  MOZ_ASSERT(aNode && outRun);
  *outRun = nullptr;

  for (WSFragment* run = mStartRun; run; run = run->mRight) {
    int32_t comp = run->mStartNode ? nsContentUtils::ComparePoints(aNode,
        aOffset, run->mStartNode, run->mStartOffset) : -1;
    if (comp <= 0) {
      if (after) {
        *outRun = run;
      } else {
        // before
        *outRun = nullptr;
      }
      return;
    }
    comp = run->mEndNode ? nsContentUtils::ComparePoints(aNode, aOffset,
        run->mEndNode, run->mEndOffset) : -1;
    if (comp < 0) {
      *outRun = run;
      return;
    } else if (comp == 0) {
      if (after) {
        *outRun = run->mRight;
      } else {
        // before
        *outRun = run;
      }
      return;
    }
    if (!run->mRight) {
      if (after) {
        *outRun = nullptr;
      } else {
        // before
        *outRun = run;
      }
      return;
    }
  }
}

char16_t 
nsWSRunObject::GetCharAt(Text* aTextNode, int32_t aOffset)
{
  // return 0 if we can't get a char, for whatever reason
  NS_ENSURE_TRUE(aTextNode, 0);

  int32_t len = int32_t(aTextNode->TextLength());
  if (aOffset < 0 || aOffset >= len)
    return 0;
    
  return aTextNode->GetText()->CharAt(aOffset);
}

nsWSRunObject::WSPoint
nsWSRunObject::GetWSPointAfter(nsINode* aNode, int32_t aOffset)
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
  nsRefPtr<Text> curNode;

  // Begin binary search.  We do this because we need to minimize calls to
  // ComparePoints(), which is expensive.
  while (curNum != lastNum) {
    curNode = mNodeArray[curNum];
    cmp = nsContentUtils::ComparePoints(aNode, aOffset, curNode, 0);
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
    // hey asked for past our range (it's after the last node). GetCharAfter
    // will do the work for us when we pass it the last index of the last node.
    nsRefPtr<Text> textNode(mNodeArray[curNum - 1]);
    WSPoint point(textNode, textNode->TextLength(), 0);
    return GetCharAfter(point);
  } else {
    // The char after the point is the first character of our range.
    nsRefPtr<Text> textNode(mNodeArray[curNum]);
    WSPoint point(textNode, 0, 0);
    return GetCharAfter(point);
  }
}

nsWSRunObject::WSPoint
nsWSRunObject::GetWSPointBefore(nsINode* aNode, int32_t aOffset)
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
  nsRefPtr<Text>  curNode;

  // Begin binary search.  We do this because we need to minimize calls to
  // ComparePoints(), which is expensive.
  while (curNum != lastNum) {
    curNode = mNodeArray[curNum];
    cmp = nsContentUtils::ComparePoints(aNode, aOffset, curNode, 0);
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
    // the node into GetCharBefore, and it will return the last character.
    nsRefPtr<Text> textNode(mNodeArray[curNum - 1]);
    WSPoint point(textNode, textNode->TextLength(), 0);
    return GetCharBefore(point);
  } else {
    // We can just ask the current node for the point immediately before it,
    // it will handle moving to the previous node (if any) and returning the
    // appropriate character
    nsRefPtr<Text> textNode(mNodeArray[curNum]);
    WSPoint point(textNode, 0, 0);
    return GetCharBefore(point);
  }
}

nsresult
nsWSRunObject::CheckTrailingNBSPOfRun(WSFragment *aRun)
{
  // Try to change an nbsp to a space, if possible, just to prevent nbsp
  // proliferation.  Examine what is before and after the trailing nbsp, if
  // any.
  NS_ENSURE_TRUE(aRun, NS_ERROR_NULL_POINTER);
  nsresult res;
  bool leftCheck = false;
  bool spaceNBSP = false;
  bool rightCheck = false;

  // confirm run is normalWS
  if (aRun->mType != WSType::normalWS) {
    return NS_ERROR_FAILURE;
  }

  // first check for trailing nbsp
  WSPoint thePoint = GetCharBefore(aRun->mEndNode, aRun->mEndOffset);
  if (thePoint.mTextNode && thePoint.mChar == nbsp) {
    // now check that what is to the left of it is compatible with replacing nbsp with space
    WSPoint prevPoint = GetCharBefore(thePoint);
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
          IsBlockNode(nsCOMPtr<nsINode>(GetWSBoundingParent()))) {
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
        thePoint = GetCharBefore(aRun->mEndNode, aRun->mEndOffset);
        prevPoint = GetCharBefore(thePoint);
        rightCheck = true;
      }
    }
    if (leftCheck && rightCheck) {
      // Now replace nbsp with space.  First, insert a space
      nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
      nsAutoString spaceStr(char16_t(32));
      res = mHTMLEditor->InsertTextIntoTextNodeImpl(spaceStr,
                                                    thePoint.mTextNode,
                                                    thePoint.mOffset, true);
      NS_ENSURE_SUCCESS(res, res);

      // Finally, delete that nbsp
      res = DeleteChars(thePoint.mTextNode, thePoint.mOffset + 1,
                        thePoint.mTextNode, thePoint.mOffset + 2);
      NS_ENSURE_SUCCESS(res, res);
    } else if (!mPRE && spaceNBSP && rightCheck) {
      // Don't mess with this preformatted for now.  We have a run of ASCII
      // whitespace (which will render as one space) followed by an nbsp (which
      // is at the end of the whitespace run).  Let's switch their order.  This
      // will ensure that if someone types two spaces after a sentence, and the
      // editor softwraps at this point, the spaces won't be split across lines,
      // which looks ugly and is bad for the moose.

      nsRefPtr<Text> startNode, endNode;
      int32_t startOffset, endOffset;
      GetAsciiWSBounds(eBoth, prevPoint.mTextNode, prevPoint.mOffset + 1,
                       getter_AddRefs(startNode), &startOffset,
                       getter_AddRefs(endNode), &endOffset);

      // Delete that nbsp
      res = DeleteChars(thePoint.mTextNode, thePoint.mOffset,
                        thePoint.mTextNode, thePoint.mOffset + 1);
      NS_ENSURE_SUCCESS(res, res);

      // Finally, insert that nbsp before the ASCII ws run
      nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
      nsAutoString nbspStr(nbsp);
      res = mHTMLEditor->InsertTextIntoTextNodeImpl(nbspStr, startNode,
                                                    startOffset, true);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}

nsresult
nsWSRunObject::CheckTrailingNBSP(WSFragment* aRun, nsINode* aNode,
                                 int32_t aOffset)
{
  // Try to change an nbsp to a space, if possible, just to prevent nbsp
  // proliferation.  This routine is called when we are about to make this
  // point in the ws abut an inserted break or text, so we don't have to worry
  // about what is after it.  What is after it now will end up after the
  // inserted object.
  NS_ENSURE_TRUE(aRun && aNode, NS_ERROR_NULL_POINTER);
  bool canConvert = false;
  WSPoint thePoint = GetCharBefore(aNode, aOffset);
  if (thePoint.mTextNode && thePoint.mChar == nbsp) {
    WSPoint prevPoint = GetCharBefore(thePoint);
    if (prevPoint.mTextNode) {
      if (!nsCRT::IsAsciiSpace(prevPoint.mChar)) {
        canConvert = true;
      }
    } else if (aRun->mLeftType == WSType::text ||
               aRun->mLeftType == WSType::special) {
      canConvert = true;
    }
  }
  if (canConvert) {
    // First, insert a space
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    nsAutoString spaceStr(char16_t(32));
    nsresult res = mHTMLEditor->InsertTextIntoTextNodeImpl(spaceStr,
        thePoint.mTextNode, thePoint.mOffset, true);
    NS_ENSURE_SUCCESS(res, res);

    // Finally, delete that nbsp
    res = DeleteChars(thePoint.mTextNode, thePoint.mOffset + 1,
                      thePoint.mTextNode, thePoint.mOffset + 2);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

nsresult
nsWSRunObject::CheckLeadingNBSP(WSFragment* aRun, nsINode* aNode,
                                int32_t aOffset)
{
  // Try to change an nbsp to a space, if possible, just to prevent nbsp
  // proliferation This routine is called when we are about to make this point
  // in the ws abut an inserted text, so we don't have to worry about what is
  // before it.  What is before it now will end up before the inserted text.
  bool canConvert = false;
  WSPoint thePoint = GetCharAfter(aNode, aOffset);
  if (thePoint.mChar == nbsp) {
    WSPoint tmp = thePoint;
    // we want to be after thePoint
    tmp.mOffset++;
    WSPoint nextPoint = GetCharAfter(tmp);
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
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    nsAutoString spaceStr(char16_t(32));
    nsresult res = mHTMLEditor->InsertTextIntoTextNodeImpl(spaceStr,
        thePoint.mTextNode, thePoint.mOffset, true);
    NS_ENSURE_SUCCESS(res, res);

    // Finally, delete that nbsp
    res = DeleteChars(thePoint.mTextNode, thePoint.mOffset + 1,
                      thePoint.mTextNode, thePoint.mOffset + 2);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}


nsresult
nsWSRunObject::Scrub()
{
  WSFragment *run = mStartRun;
  while (run)
  {
    if (run->mType & (WSType::leadingWS | WSType::trailingWS)) {
      nsresult res = DeleteChars(run->mStartNode, run->mStartOffset,
                                 run->mEndNode, run->mEndOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
    run = run->mRight;
  }
  return NS_OK;
}
