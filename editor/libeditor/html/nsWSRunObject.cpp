/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTextFragment.h"
#include "nsWSRunObject.h"
#include "nsIDOMNode.h"
#include "nsHTMLEditor.h"
#include "nsTextEditUtils.h"
#include "nsIContent.h"
#include "nsIDOMCharacterData.h"
#include "nsCRT.h"
#include "nsIRangeUtils.h"

const PRUnichar nbsp = 160;

static PRBool IsBlockNode(nsIDOMNode* node)
{
  PRBool isBlock (PR_FALSE);
  nsHTMLEditor::NodeIsBlockStatic(node, &isBlock);
  return isBlock;
}

//- constructor / destructor -----------------------------------------------
nsWSRunObject::nsWSRunObject(nsHTMLEditor *aEd, nsIDOMNode *aNode, PRInt32 aOffset) :
mNode(aNode)
,mOffset(aOffset)
,mPRE(PR_FALSE)
,mStartNode()
,mStartOffset(0)
,mStartReason(0)
,mStartReasonNode()
,mEndNode()
,mEndOffset(0)
,mEndReason(0)
,mEndReasonNode()
,mFirstNBSPNode()
,mFirstNBSPOffset(0)
,mLastNBSPNode()
,mLastNBSPOffset(0)
,mNodeArray()
,mStartRun(nsnull)
,mEndRun(nsnull)
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
nsWSRunObject::ScrubBlockBoundary(nsHTMLEditor *aHTMLEd, 
                                  nsCOMPtr<nsIDOMNode> *aBlock,
                                  BlockBoundary aBoundary,
                                  PRInt32 *aOffset)
{
  if (!aBlock || !aHTMLEd)
    return NS_ERROR_NULL_POINTER;
  if ((aBoundary == kBlockStart) || (aBoundary == kBlockEnd))
    return ScrubBlockBoundaryInner(aHTMLEd, aBlock, aBoundary);
  
  // else we are scrubbing an outer boundary - just before or after
  // a block element.
  if (!aOffset) 
    return NS_ERROR_NULL_POINTER;
  nsAutoTrackDOMPoint tracker(aHTMLEd->mRangeUpdater, aBlock, aOffset);
  nsWSRunObject theWSObj(aHTMLEd, *aBlock, *aOffset);
  return theWSObj.Scrub();
}

nsresult 
nsWSRunObject::PrepareToJoinBlocks(nsHTMLEditor *aHTMLEd, 
                                   nsIDOMNode *aLeftParent, 
                                   nsIDOMNode *aRightParent)
{
  if (!aLeftParent || !aRightParent || !aHTMLEd)
    return NS_ERROR_NULL_POINTER;
  PRUint32 count;
  aHTMLEd->GetLengthOfDOMNode(aLeftParent, count);
  nsWSRunObject leftWSObj(aHTMLEd, aLeftParent, count);
  nsWSRunObject rightWSObj(aHTMLEd, aRightParent, 0);

  return leftWSObj.PrepareToDeleteRangePriv(&rightWSObj);
}

nsresult 
nsWSRunObject::PrepareToDeleteRange(nsHTMLEditor *aHTMLEd, 
                                    nsCOMPtr<nsIDOMNode> *aStartNode,
                                    PRInt32 *aStartOffset, 
                                    nsCOMPtr<nsIDOMNode> *aEndNode,
                                    PRInt32 *aEndOffset)
{
  if (!aStartNode || !aEndNode || !*aStartNode || !*aEndNode || !aStartOffset || !aEndOffset || !aHTMLEd)
    return NS_ERROR_NULL_POINTER;

  nsAutoTrackDOMPoint trackerStart(aHTMLEd->mRangeUpdater, aStartNode, aStartOffset);
  nsAutoTrackDOMPoint trackerEnd(aHTMLEd->mRangeUpdater, aEndNode, aEndOffset);
  
  nsWSRunObject leftWSObj(aHTMLEd, *aStartNode, *aStartOffset);
  nsWSRunObject rightWSObj(aHTMLEd, *aEndNode, *aEndOffset);

  return leftWSObj.PrepareToDeleteRangePriv(&rightWSObj);
}

nsresult 
nsWSRunObject::PrepareToDeleteNode(nsHTMLEditor *aHTMLEd, 
                                   nsIDOMNode *aNode)
{
  if (!aNode || !aHTMLEd)
    return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  res = aHTMLEd->GetNodeLocation(aNode, address_of(parent), &offset);
  NS_ENSURE_SUCCESS(res, res);
  
  nsWSRunObject leftWSObj(aHTMLEd, parent, offset);
  nsWSRunObject rightWSObj(aHTMLEd, parent, offset+1);

  return leftWSObj.PrepareToDeleteRangePriv(&rightWSObj);
}

nsresult 
nsWSRunObject::PrepareToSplitAcrossBlocks(nsHTMLEditor *aHTMLEd, 
                                          nsCOMPtr<nsIDOMNode> *aSplitNode, 
                                          PRInt32 *aSplitOffset)
{
  if (!aSplitNode || !aSplitOffset || !*aSplitNode || !aHTMLEd)
    return NS_ERROR_NULL_POINTER;

  nsAutoTrackDOMPoint tracker(aHTMLEd->mRangeUpdater, aSplitNode, aSplitOffset);
  
  nsWSRunObject wsObj(aHTMLEd, *aSplitNode, *aSplitOffset);

  return wsObj.PrepareToSplitAcrossBlocksPriv();
}

//--------------------------------------------------------------------------------------------
//   public instance methods
//--------------------------------------------------------------------------------------------

nsresult 
nsWSRunObject::InsertBreak(nsCOMPtr<nsIDOMNode> *aInOutParent, 
                           PRInt32 *aInOutOffset, 
                           nsCOMPtr<nsIDOMNode> *outBRNode, 
                           nsIEditor::EDirection aSelect)
{
  // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
  // meanwhile, the pre case is handled in WillInsertText in nsHTMLEditRules.cpp
  if (!aInOutParent || !aInOutOffset || !outBRNode)
    return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;
  WSFragment *beforeRun, *afterRun;
  res = FindRun(*aInOutParent, *aInOutOffset, &beforeRun, PR_FALSE);
  res = FindRun(*aInOutParent, *aInOutOffset, &afterRun, PR_TRUE);
  
  {
    // some scoping for nsAutoTrackDOMPoint.  This will track our insertion point
    // while we tweak any surrounding whitespace
    nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, aInOutParent, aInOutOffset);

    // handle any changes needed to ws run after inserted br
    if (!afterRun)
    {
      // don't need to do anything.  just insert break.  ws won't change.
    }
    else if (afterRun->mType & eTrailingWS)
    {
      // don't need to do anything.  just insert break.  ws won't change.
    }
    else if (afterRun->mType & eLeadingWS)
    {
      // delete the leading ws that is after insertion point.  We don't
      // have to (it would still not be significant after br), but it's 
      // just more aesthetically pleasing to.
      res = DeleteChars(*aInOutParent, *aInOutOffset, afterRun->mEndNode, afterRun->mEndOffset,
                        eOutsideUserSelectAll);
      NS_ENSURE_SUCCESS(res, res);
    }
    else if (afterRun->mType == eNormalWS)
    {
      // need to determine if break at front of non-nbsp run.  if so
      // convert run to nbsp.
      WSPoint thePoint;
      res = GetCharAfter(*aInOutParent, *aInOutOffset, &thePoint);
      if ( (NS_SUCCEEDED(res)) && thePoint.mTextNode && (nsCRT::IsAsciiSpace(thePoint.mChar)) )
      {
        WSPoint prevPoint;
        res = GetCharBefore(thePoint, &prevPoint);
        if ( (NS_FAILED(res)) || (prevPoint.mTextNode && !nsCRT::IsAsciiSpace(prevPoint.mChar)) )
        {
          // we are at start of non-nbsps.  convert to a single nbsp.
          res = ConvertToNBSP(thePoint);
          NS_ENSURE_SUCCESS(res, res);
        }
      }
    }
    
    // handle any changes needed to ws run before inserted br
    if (!beforeRun)
    {
      // don't need to do anything.  just insert break.  ws won't change.
    }
    else if (beforeRun->mType & eLeadingWS)
    {
      // don't need to do anything.  just insert break.  ws won't change.
    }
    else if (beforeRun->mType & eTrailingWS)
    {
      // need to delete the trailing ws that is before insertion point, because it 
      // would become significant after break inserted.
      res = DeleteChars(beforeRun->mStartNode, beforeRun->mStartOffset, *aInOutParent, *aInOutOffset,
                        eOutsideUserSelectAll);
      NS_ENSURE_SUCCESS(res, res);
    }
    else if (beforeRun->mType == eNormalWS)
    {
      // try to change an nbsp to a space, if possible, just to prevent nbsp proliferation
      res = CheckTrailingNBSP(beforeRun, *aInOutParent, *aInOutOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  
  // ready, aim, fire!
  return mHTMLEditor->CreateBRImpl(aInOutParent, aInOutOffset, outBRNode, aSelect);
}

nsresult 
nsWSRunObject::InsertText(const nsAString& aStringToInsert, 
                          nsCOMPtr<nsIDOMNode> *aInOutParent, 
                          PRInt32 *aInOutOffset,
                          nsIDOMDocument *aDoc)
{
  // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
  // meanwhile, the pre case is handled in WillInsertText in nsHTMLEditRules.cpp

  // MOOSE: for now, just getting the ws logic straight.  This implementation
  // is very slow.  Will need to replace edit rules impl with a more efficient
  // text sink here that does the minimal amount of searching/replacing/copying

  if (!aInOutParent || !aInOutOffset || !aDoc)
    return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;
  if (aStringToInsert.IsEmpty()) return res;
  
  // string copying sux.  
  nsAutoString theString(aStringToInsert);
  
  WSFragment *beforeRun, *afterRun;
  res = FindRun(*aInOutParent, *aInOutOffset, &beforeRun, PR_FALSE);
  res = FindRun(*aInOutParent, *aInOutOffset, &afterRun, PR_TRUE);
  
  {
    // some scoping for nsAutoTrackDOMPoint.  This will track our insertion point
    // while we tweak any surrounding whitespace
    nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, aInOutParent, aInOutOffset);

    // handle any changes needed to ws run after inserted text
    if (!afterRun)
    {
      // don't need to do anything.  just insert text.  ws won't change.
    }
    else if (afterRun->mType & eTrailingWS)
    {
      // don't need to do anything.  just insert text.  ws won't change.
    }
    else if (afterRun->mType & eLeadingWS)
    {
      // delete the leading ws that is after insertion point, because it 
      // would become significant after text inserted.
      res = DeleteChars(*aInOutParent, *aInOutOffset, afterRun->mEndNode, afterRun->mEndOffset,
                         eOutsideUserSelectAll);
      NS_ENSURE_SUCCESS(res, res);
    }
    else if (afterRun->mType == eNormalWS)
    {
      // try to change an nbsp to a space, if possible, just to prevent nbsp proliferation
      res = CheckLeadingNBSP(afterRun, *aInOutParent, *aInOutOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
    
    // handle any changes needed to ws run before inserted text
    if (!beforeRun)
    {
      // don't need to do anything.  just insert text.  ws won't change.
    }
    else if (beforeRun->mType & eLeadingWS)
    {
      // don't need to do anything.  just insert text.  ws won't change.
    }
    else if (beforeRun->mType & eTrailingWS)
    {
      // need to delete the trailing ws that is before insertion point, because it 
      // would become significant after text inserted.
      res = DeleteChars(beforeRun->mStartNode, beforeRun->mStartOffset, *aInOutParent, *aInOutOffset,
                        eOutsideUserSelectAll);
      NS_ENSURE_SUCCESS(res, res);
    }
    else if (beforeRun->mType == eNormalWS)
    {
      // try to change an nbsp to a space, if possible, just to prevent nbsp proliferation
      res = CheckTrailingNBSP(beforeRun, *aInOutParent, *aInOutOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  
  // next up, tweak head and tail of string as needed.
  // first the head:
  // there are a variety of circumstances that would require us to convert a 
  // leading ws char into an nbsp:
  
  if (nsCRT::IsAsciiSpace(theString[0]))
  {
    // we have a leading space
    if (beforeRun)
    {
      if (beforeRun->mType & eLeadingWS) 
      {
        theString.SetCharAt(nbsp, 0);
      }
      else if (beforeRun->mType & eNormalWS) 
      {
        WSPoint wspoint;
        res = GetCharBefore(*aInOutParent, *aInOutOffset, &wspoint);
        if (NS_SUCCEEDED(res) && wspoint.mTextNode && nsCRT::IsAsciiSpace(wspoint.mChar))
        {
          theString.SetCharAt(nbsp, 0);
        }
      }
    }
    else
    {
      if ((mStartReason & eBlock) || (mStartReason == eBreak))
      {
        theString.SetCharAt(nbsp, 0);
      }
    }
  }

  // then the tail
  PRUint32 lastCharIndex = theString.Length()-1;

  if (nsCRT::IsAsciiSpace(theString[lastCharIndex]))
  {
    // we have a leading space
    if (afterRun)
    {
      if (afterRun->mType & eTrailingWS)
      {
        theString.SetCharAt(nbsp, lastCharIndex);
      }
      else if (afterRun->mType & eNormalWS) 
      {
        WSPoint wspoint;
        res = GetCharAfter(*aInOutParent, *aInOutOffset, &wspoint);
        if (NS_SUCCEEDED(res) && wspoint.mTextNode && nsCRT::IsAsciiSpace(wspoint.mChar))
        {
          theString.SetCharAt(nbsp, lastCharIndex);
        }
      }
    }
    else
    {
      if ((mEndReason & eBlock))
      {
        theString.SetCharAt(nbsp, lastCharIndex);
      }
    }
  }
  
  // next scan string for adjacent ws and convert to nbsp/space combos
  // MOOSE: don't need to convert tabs here since that is done by WillInsertText() 
  // before we are called.  Eventually, all that logic will be pushed down into
  // here and made more efficient.
  PRUint32 j;
  PRBool prevWS = PR_FALSE;
  for (j=0; j<=lastCharIndex; j++)
  {
    if (nsCRT::IsAsciiSpace(theString[j]))
    {
      if (prevWS)
      {
        theString.SetCharAt(nbsp, j-1);  // j-1 can't be negative because prevWS starts out false
      }
      else
      {
        prevWS = PR_TRUE;
      }
    }
    else
    {
      prevWS = PR_FALSE;
    }
  }
  
  // ready, aim, fire!
  res = mHTMLEditor->InsertTextImpl(theString, aInOutParent, aInOutOffset, aDoc);
  return NS_OK;
}

nsresult 
nsWSRunObject::DeleteWSBackward()
{
  nsresult res = NS_OK;
  WSPoint point;
  res = GetCharBefore(mNode, mOffset, &point);  
  NS_ENSURE_SUCCESS(res, res);
  if (!point.mTextNode) return NS_OK;  // nothing to delete
  
  if (mPRE)  // easy case, preformatted ws
  {
    if (nsCRT::IsAsciiSpace(point.mChar) || (point.mChar == nbsp))
    {
      nsCOMPtr<nsIDOMNode> node(do_QueryInterface(point.mTextNode));
      PRInt32 startOffset = point.mOffset;
      PRInt32 endOffset = point.mOffset+1;
      return DeleteChars(node, startOffset, node, endOffset);
    }
  }
  
  // callers job to insure that previous char is really ws.
  // If it is normal ws, we need to delete the whole run
  if (nsCRT::IsAsciiSpace(point.mChar))
  {
    nsCOMPtr<nsIDOMNode> startNode, endNode, node(do_QueryInterface(point.mTextNode));
    PRInt32 startOffset, endOffset;
    res = GetAsciiWSBounds(eBoth, node, point.mOffset+1, address_of(startNode), 
                         &startOffset, address_of(endNode), &endOffset);
    NS_ENSURE_SUCCESS(res, res);
    
    // adjust surrounding ws
    res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor, address_of(startNode), &startOffset, 
                                              address_of(endNode), &endOffset);
    NS_ENSURE_SUCCESS(res, res);
    
    // finally, delete that ws
    return DeleteChars(startNode, startOffset, endNode, endOffset);
  }
  else if (point.mChar == nbsp)
  {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(point.mTextNode));
    // adjust surrounding ws
    PRInt32 startOffset = point.mOffset;
    PRInt32 endOffset = point.mOffset+1;
    res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor, address_of(node), &startOffset, 
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
  nsresult res = NS_OK;
  WSPoint point;
  res = GetCharAfter(mNode, mOffset, &point);  
  NS_ENSURE_SUCCESS(res, res);
  if (!point.mTextNode) return NS_OK;  // nothing to delete
  
  if (mPRE)  // easy case, preformatted ws
  {
    if (nsCRT::IsAsciiSpace(point.mChar) || (point.mChar == nbsp))
    {
      nsCOMPtr<nsIDOMNode> node(do_QueryInterface(point.mTextNode));
      PRInt32 startOffset = point.mOffset;
      PRInt32 endOffset = point.mOffset+1;
      return DeleteChars(node, startOffset, node, endOffset);
    }
  }
  
  // callers job to insure that next char is really ws.
  // If it is normal ws, we need to delete the whole run
  if (nsCRT::IsAsciiSpace(point.mChar))
  {
    nsCOMPtr<nsIDOMNode> startNode, endNode, node(do_QueryInterface(point.mTextNode));
    PRInt32 startOffset, endOffset;
    res = GetAsciiWSBounds(eBoth, node, point.mOffset+1, address_of(startNode), 
                         &startOffset, address_of(endNode), &endOffset);
    NS_ENSURE_SUCCESS(res, res);
    
    // adjust surrounding ws
    res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor, address_of(startNode), &startOffset, 
                                              address_of(endNode), &endOffset);
    NS_ENSURE_SUCCESS(res, res);
    
    // finally, delete that ws
    return DeleteChars(startNode, startOffset, endNode, endOffset);
  }
  else if (point.mChar == nbsp)
  {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(point.mTextNode));
    // adjust surrounding ws
    PRInt32 startOffset = point.mOffset;
    PRInt32 endOffset = point.mOffset+1;
    res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor, address_of(node), &startOffset, 
                                              address_of(node), &endOffset);
    NS_ENSURE_SUCCESS(res, res);
    
    // finally, delete that ws
    return DeleteChars(node, startOffset, node, endOffset);
  
  }
  return NS_OK;
}

nsresult 
nsWSRunObject::PriorVisibleNode(nsIDOMNode *aNode, 
                                PRInt32 aOffset, 
                                nsCOMPtr<nsIDOMNode> *outVisNode, 
                                PRInt32 *outVisOffset,
                                PRInt16 *outType)
{
  // Find first visible thing before the point.  position outVisNode/outVisOffset
  // just _after_ that thing.  If we don't find anything return start of ws.
  if (!aNode || !outVisNode || !outVisOffset || !outType)
    return NS_ERROR_NULL_POINTER;
    
  *outType = eNone;
  WSFragment *run;
  FindRun(aNode, aOffset, &run, PR_FALSE);
  
  // is there a visible run there or earlier?
  while (run)
  {
    if (run->mType == eNormalWS)
    {
      WSPoint point;
      GetCharBefore(aNode, aOffset, &point);
      if (point.mTextNode)
      {
        *outVisNode = do_QueryInterface(point.mTextNode);
        *outVisOffset = point.mOffset+1;
        if (nsCRT::IsAsciiSpace(point.mChar) || (point.mChar==nbsp))
        {
          *outType = eNormalWS;
        }
        else if (!point.mChar)
        {
          // MOOSE: not possible?
          *outType = eNone;
        }
        else
        {
          *outType = eText;
        }
        return NS_OK;
      }
      // else if no text node then keep looking.  We should eventually fall out of loop
    }

    run = run->mLeft;
  }
  
  // if we get here then nothing in ws data to find.  return start reason
  *outVisNode = mStartReasonNode;
  *outVisOffset = mStartOffset;  // this really isn't meaningful if mStartReasonNode!=mStartNode
  *outType = mStartReason;
  return NS_OK;
}


nsresult 
nsWSRunObject::NextVisibleNode (nsIDOMNode *aNode, 
                                PRInt32 aOffset, 
                                nsCOMPtr<nsIDOMNode> *outVisNode, 
                                PRInt32 *outVisOffset,
                                PRInt16 *outType)
{
  // Find first visible thing after the point.  position outVisNode/outVisOffset
  // just _before_ that thing.  If we don't find anything return end of ws.
  if (!aNode || !outVisNode || !outVisOffset || !outType)
    return NS_ERROR_NULL_POINTER;
    
  WSFragment *run;
  FindRun(aNode, aOffset, &run, PR_TRUE);
  
  // is there a visible run there or later?
  while (run)
  {
    if (run->mType == eNormalWS)
    {
      WSPoint point;
      GetCharAfter(aNode, aOffset, &point);
      if (point.mTextNode)
      {
        *outVisNode = do_QueryInterface(point.mTextNode);
        *outVisOffset = point.mOffset;
        if (nsCRT::IsAsciiSpace(point.mChar) || (point.mChar==nbsp))
        {
          *outType = eNormalWS;
        }
        else if (!point.mChar)
        {
          // MOOSE: not possible?
          *outType = eNone;
        }
        else
        {
          *outType = eText;
        }
        return NS_OK;
      }
      // else if no text node then keep looking.  We should eventually fall out of loop
    }

    run = run->mRight;
  }
  
  // if we get here then nothing in ws data to find.  return end reason
  *outVisNode = mEndReasonNode;
  *outVisOffset = mEndOffset; // this really isn't meaningful if mEndReasonNode!=mEndNode
  *outType = mEndReason;
  return NS_OK;
}

nsresult 
nsWSRunObject::AdjustWhitespace()
{
  // this routine examines a run of ws and tries to get rid of some unneeded nbsp's,
  // replacing them with regualr ascii space if possible.  Keeping things simple
  // for now and just trying to fix up the trailing ws in the run.
  if (!mLastNBSPNode) return NS_OK; // nothing to do!
  nsresult res = NS_OK;
  WSFragment *curRun = mStartRun;
  while (curRun)
  {
    // look for normal ws run
    if (curRun->mType == eNormalWS)
    {
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

nsresult
nsWSRunObject::GetWSNodes()
{
  // collect up an array of nodes that are contiguous with the insertion point
  // and which contain only whitespace.  Stop if you reach non-ws text or a new 
  // block boundary.
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> blockParent;
  DOMPoint start(mNode, mOffset), end(mNode, mOffset);
  if (IsBlockNode(mNode)) blockParent = mNode;
  else blockParent = mHTMLEditor->GetBlockNodeParent(mNode);

  // first look backwards to find preceding ws nodes
  if (mHTMLEditor->IsTextNode(mNode))
  {
    nsCOMPtr<nsIContent> textNode(do_QueryInterface(mNode));
    const nsTextFragment *textFrag = textNode->GetText();
    
    res = PrependNodeToList(mNode);
    NS_ENSURE_SUCCESS(res, res);
    if (mOffset)
    {
      PRInt32 pos;
      for (pos=mOffset-1; pos>=0; pos--)
      {
        // sanity bounds check the char position.  bug 136165
        if (pos >= textFrag->GetLength())
        {
          NS_NOTREACHED("looking beyond end of text fragment");
          continue;
        }
        PRUnichar theChar = textFrag->CharAt(pos);
        if (!nsCRT::IsAsciiSpace(theChar))
        {
          if (theChar != nbsp)
          {
            mStartNode = mNode;
            mStartOffset = pos+1;
            mStartReason = eText;
            mStartReasonNode = mNode;
            break;
          }
          // as we look backwards update our earliest found nbsp
          mFirstNBSPNode = mNode;
          mFirstNBSPOffset = pos;
          // also keep track of latest nbsp so far
          if (!mLastNBSPNode)
          {
            mLastNBSPNode = mNode;
            mLastNBSPOffset = pos;
          }
        }
        start.SetPoint(mNode,pos);
      }
    }
  }

  nsCOMPtr<nsIDOMNode> priorNode;
  while (!mStartNode)
  {
    // we haven't found the start of ws yet.  Keep looking
    res = GetPreviousWSNode(start, blockParent, address_of(priorNode));
    NS_ENSURE_SUCCESS(res, res);
    if (priorNode)
    {
      if (IsBlockNode(priorNode))
      {
        start.GetPoint(mStartNode, mStartOffset);
        mStartReason = eOtherBlock;
        mStartReasonNode = priorNode;
      }
      else if (mHTMLEditor->IsTextNode(priorNode))
      {
        res = PrependNodeToList(priorNode);
        NS_ENSURE_SUCCESS(res, res);
        nsCOMPtr<nsIContent> textNode(do_QueryInterface(priorNode));
        const nsTextFragment *textFrag;
        if (!textNode || !(textFrag = textNode->GetText())) {
          return NS_ERROR_NULL_POINTER;
        }
        PRUint32 len = textNode->TextLength();

        if (len < 1)
        {
          // Zero length text node. Set start point to it
          // so we can get past it!
          start.SetPoint(priorNode,0);
        }
        else
        {
          PRInt32 pos;
          for (pos=len-1; pos>=0; pos--)
          {
            // sanity bounds check the char position.  bug 136165
            if (pos >= textFrag->GetLength())
            {
              NS_NOTREACHED("looking beyond end of text fragment");
              continue;
            }
            PRUnichar theChar = textFrag->CharAt(pos);
            if (!nsCRT::IsAsciiSpace(theChar))
            {
              if (theChar != nbsp)
              {
                mStartNode = priorNode;
                mStartOffset = pos+1;
                mStartReason = eText;
                mStartReasonNode = priorNode;
                break;
              }
              // as we look backwards update our earliest found nbsp
              mFirstNBSPNode = priorNode;
              mFirstNBSPOffset = pos;
              // also keep track of latest nbsp so far
              if (!mLastNBSPNode)
              {
                mLastNBSPNode = priorNode;
                mLastNBSPOffset = pos;
              }
            }
            start.SetPoint(priorNode,pos);
          }
        }
      }
      else
      {
        // it's a break or a special node, like <img>, that is not a block and not
        // a break but still serves as a terminator to ws runs.
        start.GetPoint(mStartNode, mStartOffset);
        if (nsTextEditUtils::IsBreak(priorNode))
          mStartReason = eBreak;
        else
          mStartReason = eSpecial;
        mStartReasonNode = priorNode;
      }
    }
    else
    {
      // no prior node means we exhausted blockParent
      start.GetPoint(mStartNode, mStartOffset);
      mStartReason = eThisBlock;
      mStartReasonNode = blockParent;
    } 
  }
  
  // then look ahead to find following ws nodes
  if (mHTMLEditor->IsTextNode(mNode))
  {
    // don't need to put it on list. it already is from code above
    nsCOMPtr<nsIContent> textNode(do_QueryInterface(mNode));
    const nsTextFragment *textFrag = textNode->GetText();

    PRUint32 len = textNode->TextLength();
    if (mOffset<len)
    {
      PRInt32 pos;
      for (pos=mOffset; pos<len; pos++)
      {
        // sanity bounds check the char position.  bug 136165
        if ((pos<0) || (pos>=textFrag->GetLength()))
        {
          NS_NOTREACHED("looking beyond end of text fragment");
          continue;
        }
        PRUnichar theChar = textFrag->CharAt(pos);
        if (!nsCRT::IsAsciiSpace(theChar))
        {
          if (theChar != nbsp)
          {
            mEndNode = mNode;
            mEndOffset = pos;
            mEndReason = eText;
            mEndReasonNode = mNode;
            break;
          }
          // as we look forwards update our latest found nbsp
          mLastNBSPNode = mNode;
          mLastNBSPOffset = pos;
          // also keep track of earliest nbsp so far
          if (!mFirstNBSPNode)
          {
            mFirstNBSPNode = mNode;
            mFirstNBSPOffset = pos;
          }
        }
        end.SetPoint(mNode,pos);
      }
    }
  }

  nsCOMPtr<nsIDOMNode> nextNode;
  while (!mEndNode)
  {
    // we haven't found the end of ws yet.  Keep looking
    res = GetNextWSNode(end, blockParent, address_of(nextNode));
    NS_ENSURE_SUCCESS(res, res);
    if (nextNode)
    {
      if (IsBlockNode(nextNode))
      {
        // we encountered a new block.  therefore no more ws.
        end.GetPoint(mEndNode, mEndOffset);
        mEndReason = eOtherBlock;
        mEndReasonNode = nextNode;
      }
      else if (mHTMLEditor->IsTextNode(nextNode))
      {
        res = AppendNodeToList(nextNode);
        NS_ENSURE_SUCCESS(res, res);
        nsCOMPtr<nsIContent> textNode(do_QueryInterface(nextNode));
        const nsTextFragment *textFrag;
        if (!textNode || !(textFrag = textNode->GetText())) {
          return NS_ERROR_NULL_POINTER;
        }
        PRUint32 len = textNode->TextLength();

        if (len < 1)
        {
          // Zero length text node. Set end point to it
          // so we can get past it!
          end.SetPoint(nextNode,0);
        }
        else
        {
          PRInt32 pos;
          for (pos=0; pos<len; pos++)
          {
            // sanity bounds check the char position.  bug 136165
            if (pos >= textFrag->GetLength())
            {
              NS_NOTREACHED("looking beyond end of text fragment");
              continue;
            }
            PRUnichar theChar = textFrag->CharAt(pos);
            if (!nsCRT::IsAsciiSpace(theChar))
            {
              if (theChar != nbsp)
              {
                mEndNode = nextNode;
                mEndOffset = pos;
                mEndReason = eText;
                mEndReasonNode = nextNode;
                break;
              }
              // as we look forwards update our latest found nbsp
              mLastNBSPNode = nextNode;
              mLastNBSPOffset = pos;
              // also keep track of earliest nbsp so far
              if (!mFirstNBSPNode)
              {
                mFirstNBSPNode = nextNode;
                mFirstNBSPOffset = pos;
              }
            }
            end.SetPoint(nextNode,pos+1);
          }
        }
      }
      else
      {
        // we encountered a break or a special node, like <img>, 
        // that is not a block and not a break but still 
        // serves as a terminator to ws runs.
        end.GetPoint(mEndNode, mEndOffset);
        if (nsTextEditUtils::IsBreak(nextNode))
          mEndReason = eBreak;
        else
          mEndReason = eSpecial;
        mEndReasonNode = nextNode;
      }
    }
    else
    {
      // no next node means we exhausted blockParent
      end.GetPoint(mEndNode, mEndOffset);
      mEndReason = eThisBlock;
      mEndReasonNode = blockParent;
    } 
  }

  return NS_OK;
}

nsresult
nsWSRunObject::GetRuns()
{
  ClearRuns();
  
  // handle some easy cases first
  mHTMLEditor->IsPreformatted(mNode, &mPRE);
  // if it's preformatedd, or if we are surrounded by text or special, it's all one
  // big normal ws run
  if ( mPRE || (((mStartReason == eText) || (mStartReason == eSpecial)) &&
       ((mEndReason == eText) || (mEndReason == eSpecial) || (mEndReason == eBreak))) )
  {
    return MakeSingleWSRun(eNormalWS);
  }

  // if we are before or after a block (or after a break), and there are no nbsp's,
  // then it's all non-rendering ws.
  if ( !(mFirstNBSPNode || mLastNBSPNode) &&
      ( (mStartReason & eBlock) || (mStartReason == eBreak) || (mEndReason & eBlock) ) )
  {
    PRInt16 wstype = eNone;
    if ((mStartReason & eBlock) || (mStartReason == eBreak))
      wstype = eLeadingWS;
    if (mEndReason & eBlock) 
      wstype |= eTrailingWS;
    return MakeSingleWSRun(wstype);
  }
  
  // otherwise a little trickier.  shucks.
  mStartRun = new WSFragment();
  if (!mStartRun) return NS_ERROR_NULL_POINTER;
  mStartRun->mStartNode = mStartNode;
  mStartRun->mStartOffset = mStartOffset;
  
  if ( (mStartReason & eBlock) || (mStartReason == eBreak) )
  {
    // set up mStartRun
    mStartRun->mType = eLeadingWS;
    mStartRun->mEndNode = mFirstNBSPNode;
    mStartRun->mEndOffset = mFirstNBSPOffset;
    mStartRun->mLeftType = mStartReason;
    mStartRun->mRightType = eNormalWS;
    
    // set up next run
    WSFragment *normalRun = new WSFragment();
    if (!normalRun) return NS_ERROR_NULL_POINTER;
    mStartRun->mRight = normalRun;
    normalRun->mType = eNormalWS;
    normalRun->mStartNode = mFirstNBSPNode;
    normalRun->mStartOffset = mFirstNBSPOffset;
    normalRun->mLeftType = eLeadingWS;
    normalRun->mLeft = mStartRun;
    if (mEndReason != eBlock)
    {
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
        normalRun->mRightType = eTrailingWS;
        
        // set up next run
        WSFragment *lastRun = new WSFragment();
        if (!lastRun) return NS_ERROR_NULL_POINTER;
        lastRun->mType = eTrailingWS;
        lastRun->mStartNode = mLastNBSPNode;
        lastRun->mStartOffset = mLastNBSPOffset+1;
        lastRun->mEndNode = mEndNode;
        lastRun->mEndOffset = mEndOffset;
        lastRun->mLeftType = eNormalWS;
        lastRun->mLeft = normalRun;
        lastRun->mRightType = mEndReason;
        mEndRun = lastRun;
        normalRun->mRight = lastRun;
      }
    }
  }
  else // mStartReason is not eBlock or eBreak
  {
    // set up mStartRun
    mStartRun->mType = eNormalWS;
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
      if (!lastRun) return NS_ERROR_NULL_POINTER;
      lastRun->mType = eTrailingWS;
      lastRun->mStartNode = mLastNBSPNode;
      lastRun->mStartOffset = mLastNBSPOffset+1;
      lastRun->mLeftType = eNormalWS;
      lastRun->mLeft = mStartRun;
      lastRun->mRightType = mEndReason;
      mEndRun = lastRun;
      mStartRun->mRight = lastRun;
      mStartRun->mRightType = eTrailingWS;
    }
  }
  
  return NS_OK;
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

nsresult 
nsWSRunObject::MakeSingleWSRun(PRInt16 aType)
{
  mStartRun = new WSFragment();
  if (!mStartRun) return NS_ERROR_NULL_POINTER;

  mStartRun->mStartNode   = mStartNode;
  mStartRun->mStartOffset = mStartOffset;
  mStartRun->mType        = aType;
  mStartRun->mEndNode     = mEndNode;
  mStartRun->mEndOffset   = mEndOffset;
  mStartRun->mLeftType    = mStartReason;
  mStartRun->mRightType   = mEndReason;
  
  mEndRun  = mStartRun;
  
  return NS_OK;
}

nsresult 
nsWSRunObject::PrependNodeToList(nsIDOMNode *aNode)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  if (!mNodeArray.InsertObjectAt(aNode, 0))
    return NS_ERROR_FAILURE;
  return NS_OK;
}

nsresult 
nsWSRunObject::AppendNodeToList(nsIDOMNode *aNode)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  if (!mNodeArray.AppendObject(aNode))
    return NS_ERROR_FAILURE;
  return NS_OK;
}

nsresult 
nsWSRunObject::GetPreviousWSNode(nsIDOMNode *aStartNode, 
                                 nsIDOMNode *aBlockParent, 
                                 nsCOMPtr<nsIDOMNode> *aPriorNode)
{
  // can't really recycle various getnext/prior routines because we
  // have special needs here.  Need to step into inline containers but
  // not block containers.
  if (!aStartNode || !aBlockParent || !aPriorNode) return NS_ERROR_NULL_POINTER;
  
  nsresult res = aStartNode->GetPreviousSibling(getter_AddRefs(*aPriorNode));
  NS_ENSURE_SUCCESS(res, res);
  nsCOMPtr<nsIDOMNode> temp, curNode = aStartNode;
  while (!*aPriorNode)
  {
    // we have exhausted nodes in parent of aStartNode.
    res = curNode->GetParentNode(getter_AddRefs(temp));
    NS_ENSURE_SUCCESS(res, res);
    if (!temp) return NS_ERROR_NULL_POINTER;
    if (temp == aBlockParent)
    {
      // we have exhausted nodes in the block parent.  The convention here is to return null.
      *aPriorNode = nsnull;
      return NS_OK;
    }
    // we have a parent: look for previous sibling
    res = temp->GetPreviousSibling(getter_AddRefs(*aPriorNode));
    NS_ENSURE_SUCCESS(res, res);
    curNode = temp;
  }
  // we have a prior node.  If it's a block, return it.
  if (IsBlockNode(*aPriorNode))
    return NS_OK;
  // else if it's a container, get deep rightmost child
  else if (mHTMLEditor->IsContainer(*aPriorNode))
  {
    temp = mHTMLEditor->GetRightmostChild(*aPriorNode);
    if (temp)
      *aPriorNode = temp;
    return NS_OK;
  }
  // else return the node itself
  return NS_OK;
}

nsresult 
nsWSRunObject::GetPreviousWSNode(DOMPoint aPoint,
                                 nsIDOMNode *aBlockParent, 
                                 nsCOMPtr<nsIDOMNode> *aPriorNode)
{
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  aPoint.GetPoint(node, offset);
  return GetPreviousWSNode(node,offset,aBlockParent,aPriorNode);
}

nsresult 
nsWSRunObject::GetPreviousWSNode(nsIDOMNode *aStartNode,
                                 PRInt16 aOffset, 
                                 nsIDOMNode *aBlockParent, 
                                 nsCOMPtr<nsIDOMNode> *aPriorNode)
{
  // can't really recycle various getnext/prior routines because we
  // have special needs here.  Need to step into inline containers but
  // not block containers.
  if (!aStartNode || !aBlockParent || !aPriorNode)
    return NS_ERROR_NULL_POINTER;
  *aPriorNode = 0;

  if (mHTMLEditor->IsTextNode(aStartNode))
    return GetPreviousWSNode(aStartNode, aBlockParent, aPriorNode);
  if (!mHTMLEditor->IsContainer(aStartNode))
    return GetPreviousWSNode(aStartNode, aBlockParent, aPriorNode);
  
  if (!aOffset)
  {
    if (aStartNode==aBlockParent)
    {
      // we are at start of the block.
      return NS_OK;
    }

    // we are at start of non-block container
    return GetPreviousWSNode(aStartNode, aBlockParent, aPriorNode);
  }

  nsCOMPtr<nsIContent> startContent( do_QueryInterface(aStartNode) );

  nsIContent *priorContent = startContent->GetChildAt(aOffset - 1);
  if (!priorContent) 
    return NS_ERROR_NULL_POINTER;
  *aPriorNode = do_QueryInterface(priorContent);
  // we have a prior node.  If it's a block, return it.
  if (IsBlockNode(*aPriorNode))
    return NS_OK;
  // else if it's a container, get deep rightmost child
  else if (mHTMLEditor->IsContainer(*aPriorNode))
  {
    nsCOMPtr<nsIDOMNode> temp;
    temp = mHTMLEditor->GetRightmostChild(*aPriorNode);
    if (temp)
      *aPriorNode = temp;
    return NS_OK;
  }
  // else return the node itself
  return NS_OK;
}

nsresult 
nsWSRunObject::GetNextWSNode(nsIDOMNode *aStartNode, 
                             nsIDOMNode *aBlockParent, 
                             nsCOMPtr<nsIDOMNode> *aNextNode)
{
  // can't really recycle various getnext/prior routines because we
  // have special needs here.  Need to step into inline containers but
  // not block containers.
  if (!aStartNode || !aBlockParent || !aNextNode)
    return NS_ERROR_NULL_POINTER;
  
  *aNextNode = 0;
  nsresult res = aStartNode->GetNextSibling(getter_AddRefs(*aNextNode));
  NS_ENSURE_SUCCESS(res, res);
  nsCOMPtr<nsIDOMNode> temp, curNode = aStartNode;
  while (!*aNextNode)
  {
    // we have exhausted nodes in parent of aStartNode.
    res = curNode->GetParentNode(getter_AddRefs(temp));
    NS_ENSURE_SUCCESS(res, res);
    if (!temp) return NS_ERROR_NULL_POINTER;
    if (temp == aBlockParent)
    {
      // we have exhausted nodes in the block parent.  The convention
      // here is to return null.
      *aNextNode = nsnull;
      return NS_OK;
    }
    // we have a parent: look for next sibling
    res = temp->GetNextSibling(getter_AddRefs(*aNextNode));
    NS_ENSURE_SUCCESS(res, res);
    curNode = temp;
  }
  // we have a next node.  If it's a block, return it.
  if (IsBlockNode(*aNextNode))
    return NS_OK;
  // else if it's a container, get deep leftmost child
  else if (mHTMLEditor->IsContainer(*aNextNode))
  {
    temp = mHTMLEditor->GetLeftmostChild(*aNextNode);
    if (temp)
      *aNextNode = temp;
    return NS_OK;
  }
  // else return the node itself
  return NS_OK;
}

nsresult 
nsWSRunObject::GetNextWSNode(DOMPoint aPoint,
                             nsIDOMNode *aBlockParent, 
                             nsCOMPtr<nsIDOMNode> *aNextNode)
{
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  aPoint.GetPoint(node, offset);
  return GetNextWSNode(node,offset,aBlockParent,aNextNode);
}

nsresult 
nsWSRunObject::GetNextWSNode(nsIDOMNode *aStartNode,
                             PRInt16 aOffset, 
                             nsIDOMNode *aBlockParent, 
                             nsCOMPtr<nsIDOMNode> *aNextNode)
{
  // can't really recycle various getnext/prior routines because we have special needs
  // here.  Need to step into inline containers but not block containers.
  if (!aStartNode || !aBlockParent || !aNextNode)
    return NS_ERROR_NULL_POINTER;
  *aNextNode = 0;

  if (mHTMLEditor->IsTextNode(aStartNode))
    return GetNextWSNode(aStartNode, aBlockParent, aNextNode);
  if (!mHTMLEditor->IsContainer(aStartNode))
    return GetNextWSNode(aStartNode, aBlockParent, aNextNode);
  
  nsCOMPtr<nsIContent> startContent( do_QueryInterface(aStartNode) );
  nsIContent *nextContent = startContent->GetChildAt(aOffset);
  if (!nextContent)
  {
    if (aStartNode==aBlockParent)
    {
      // we are at end of the block.
      return NS_OK;
    }

    // we are at end of non-block container
    return GetNextWSNode(aStartNode, aBlockParent, aNextNode);
  }
  
  *aNextNode = do_QueryInterface(nextContent);
  // we have a next node.  If it's a block, return it.
  if (IsBlockNode(*aNextNode))
    return NS_OK;
  // else if it's a container, get deep leftmost child
  else if (mHTMLEditor->IsContainer(*aNextNode))
  {
    nsCOMPtr<nsIDOMNode> temp;
    temp = mHTMLEditor->GetLeftmostChild(*aNextNode);
    if (temp)
      *aNextNode = temp;
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
  
  if (!aEndObject)
    return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  // get the runs before and after selection
  WSFragment *beforeRun, *afterRun;
  res = FindRun(mNode, mOffset, &beforeRun, PR_FALSE);
  NS_ENSURE_SUCCESS(res, res);
  res = aEndObject->FindRun(aEndObject->mNode, aEndObject->mOffset, &afterRun, PR_TRUE);
  NS_ENSURE_SUCCESS(res, res);
  
  // trim after run of any leading ws
  if (afterRun && (afterRun->mType & eLeadingWS))
  {
    res = aEndObject->DeleteChars(aEndObject->mNode, aEndObject->mOffset, afterRun->mEndNode, afterRun->mEndOffset,
                                  eOutsideUserSelectAll);
    NS_ENSURE_SUCCESS(res, res);
  }
  // adjust normal ws in afterRun if needed
  if (afterRun && (afterRun->mType == eNormalWS) && !aEndObject->mPRE)
  {
    if ( (beforeRun && (beforeRun->mType & eLeadingWS)) ||
         (!beforeRun && ((mStartReason & eBlock) || (mStartReason == eBreak))) )
    {
      // make sure leading char of following ws is an nbsp, so that it will show up
      WSPoint point;
      aEndObject->GetCharAfter(aEndObject->mNode, aEndObject->mOffset, &point);
      if (point.mTextNode && nsCRT::IsAsciiSpace(point.mChar))
      {
        res = aEndObject->ConvertToNBSP(point, eOutsideUserSelectAll);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  // trim before run of any trailing ws
  if (beforeRun && (beforeRun->mType & eTrailingWS))
  {
    res = DeleteChars(beforeRun->mStartNode, beforeRun->mStartOffset, mNode, mOffset,
                      eOutsideUserSelectAll);
    NS_ENSURE_SUCCESS(res, res);
  }
  else if (beforeRun && (beforeRun->mType == eNormalWS) && !mPRE)
  {
    if ( (afterRun && (afterRun->mType & eTrailingWS)) ||
         (afterRun && (afterRun->mType == eNormalWS))   ||
         (!afterRun && ((aEndObject->mEndReason & eBlock))) )
    {
      // make sure trailing char of starting ws is an nbsp, so that it will show up
      WSPoint point;
      GetCharBefore(mNode, mOffset, &point);
      if (point.mTextNode && nsCRT::IsAsciiSpace(point.mChar))
      {
        nsCOMPtr<nsIDOMNode> wsStartNode, wsEndNode;
        PRInt32 wsStartOffset, wsEndOffset;
        res = GetAsciiWSBounds(eBoth, mNode, mOffset, 
                               address_of(wsStartNode), &wsStartOffset, 
                               address_of(wsEndNode), &wsEndOffset);
        NS_ENSURE_SUCCESS(res, res);
        point.mTextNode = do_QueryInterface(wsStartNode);
        if (!point.mTextNode->IsNodeOfType(nsINode::eDATA_NODE)) {
          // Not sure if this is needed, but it'll maintain the same
          // functionality
          point.mTextNode = nsnull;
        }
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
  res = FindRun(mNode, mOffset, &beforeRun, PR_FALSE);
  NS_ENSURE_SUCCESS(res, res);
  res = FindRun(mNode, mOffset, &afterRun, PR_TRUE);
  
  // adjust normal ws in afterRun if needed
  if (afterRun && (afterRun->mType == eNormalWS))
  {
    // make sure leading char of following ws is an nbsp, so that it will show up
    WSPoint point;
    GetCharAfter(mNode, mOffset, &point);
    if (point.mTextNode && nsCRT::IsAsciiSpace(point.mChar))
    {
      res = ConvertToNBSP(point);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  // adjust normal ws in beforeRun if needed
  if (beforeRun && (beforeRun->mType == eNormalWS))
  {
    // make sure trailing char of starting ws is an nbsp, so that it will show up
    WSPoint point;
    GetCharBefore(mNode, mOffset, &point);
    if (point.mTextNode && nsCRT::IsAsciiSpace(point.mChar))
    {
      nsCOMPtr<nsIDOMNode> wsStartNode, wsEndNode;
      PRInt32 wsStartOffset, wsEndOffset;
      res = GetAsciiWSBounds(eBoth, mNode, mOffset, 
                             address_of(wsStartNode), &wsStartOffset, 
                             address_of(wsEndNode), &wsEndOffset);
      NS_ENSURE_SUCCESS(res, res);
      point.mTextNode = do_QueryInterface(wsStartNode);
      if (!point.mTextNode->IsNodeOfType(nsINode::eDATA_NODE)) {
        // Not sure if this is needed, but it'll maintain the same
        // functionality
        point.mTextNode = nsnull;
      }
      point.mOffset = wsStartOffset;
      res = ConvertToNBSP(point);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return res;
}

nsresult 
nsWSRunObject::DeleteChars(nsIDOMNode *aStartNode, PRInt32 aStartOffset, 
                           nsIDOMNode *aEndNode, PRInt32 aEndOffset,
                           AreaRestriction aAR)
{
  // MOOSE: this routine needs to be modified to preserve the integrity of the
  // wsFragment info.
  if (!aStartNode || !aEndNode)
    return NS_ERROR_NULL_POINTER;

  if (aAR == eOutsideUserSelectAll)
  {
    nsCOMPtr<nsIDOMNode> san = mHTMLEditor->FindUserSelectAllNode(aStartNode);
    if (san)
      return NS_OK;
    
    if (aStartNode != aEndNode)
    {
      san = mHTMLEditor->FindUserSelectAllNode(aEndNode);
      if (san)
        return NS_OK;
    }
  }

  if ((aStartNode == aEndNode) && (aStartOffset == aEndOffset))
    return NS_OK;  // nothing to delete
  
  nsresult res = NS_OK;
  PRInt32 idx = mNodeArray.IndexOf(aStartNode);
  if (idx==-1) idx = 0; // if our strarting point wasn't one of our ws text nodes,
                        // then just go through them from the beginning.
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIDOMCharacterData> textnode;
  nsCOMPtr<nsIDOMRange> range;

  if (aStartNode == aEndNode)
  {
    textnode = do_QueryInterface(aStartNode);
    if (textnode)
    {
      return mHTMLEditor->DeleteText(textnode, (PRUint32)aStartOffset, 
                                     (PRUint32)(aEndOffset-aStartOffset));
    }
  }

  PRInt32 count = mNodeArray.Count();
  while (idx < count)
  {
    node = mNodeArray[idx];
    if (!node)
      break;  // we ran out of ws nodes; must have been deleting to end
    if (node == aStartNode)
    {
      textnode = do_QueryInterface(node);
      PRUint32 len;
      textnode->GetLength(&len);
      if (aStartOffset<len)
      {
        res = mHTMLEditor->DeleteText(textnode, (PRUint32)aStartOffset, len-aStartOffset);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
    else if (node == aEndNode)
    {
      if (aEndOffset)
      {
        textnode = do_QueryInterface(node);
        res = mHTMLEditor->DeleteText(textnode, 0, (PRUint32)aEndOffset);
        NS_ENSURE_SUCCESS(res, res);
      }
      break;
    }
    else
    {
      if (!range)
      {
        range = do_CreateInstance("@mozilla.org/content/range;1");
        if (!range) return NS_ERROR_OUT_OF_MEMORY;
        res = range->SetStart(aStartNode, aStartOffset);
        NS_ENSURE_SUCCESS(res, res);
        res = range->SetEnd(aEndNode, aEndOffset);
        NS_ENSURE_SUCCESS(res, res);
      }
      PRBool nodeBefore, nodeAfter;
      nsCOMPtr<nsIContent> content (do_QueryInterface(node));
      res = mHTMLEditor->sRangeHelper->CompareNodeToRange(content, range, &nodeBefore, &nodeAfter);
      NS_ENSURE_SUCCESS(res, res);
      if (nodeAfter)
      {
        break;
      }
      if (!nodeBefore)
      {
        res = mHTMLEditor->DeleteNode(node);
        NS_ENSURE_SUCCESS(res, res);
        mNodeArray.RemoveObject(node);
        --count;
        --idx;
      }
    }
    idx++;
  }
  return res;
}

nsresult 
nsWSRunObject::GetCharAfter(nsIDOMNode *aNode, PRInt32 aOffset, WSPoint *outPoint)
{
  if (!aNode || !outPoint)
    return NS_ERROR_NULL_POINTER;

  PRInt32 idx = mNodeArray.IndexOf(aNode);
  if (idx == -1) 
  {
    // use range comparisons to get right ws node
    return GetWSPointAfter(aNode, aOffset, outPoint);
  }
  else
  {
    // use wspoint version of GetCharAfter()
    WSPoint point(aNode,aOffset,0);
    return GetCharAfter(point, outPoint);
  }
  
  return NS_ERROR_FAILURE;
}

nsresult 
nsWSRunObject::GetCharBefore(nsIDOMNode *aNode, PRInt32 aOffset, WSPoint *outPoint)
{
  if (!aNode || !outPoint)
    return NS_ERROR_NULL_POINTER;

  PRInt32 idx = mNodeArray.IndexOf(aNode);
  if (idx == -1) 
  {
    // use range comparisons to get right ws node
    return GetWSPointBefore(aNode, aOffset, outPoint);
  }
  else
  {
    // use wspoint version of GetCharBefore()
    WSPoint point(aNode,aOffset,0);
    return GetCharBefore(point, outPoint);
  }
  
  return NS_ERROR_FAILURE;
}

nsresult 
nsWSRunObject::GetCharAfter(WSPoint &aPoint, WSPoint *outPoint)
{
  if (!aPoint.mTextNode || !outPoint)
    return NS_ERROR_NULL_POINTER;
  
  outPoint->mTextNode = nsnull;
  outPoint->mOffset = 0;
  outPoint->mChar = 0;

  nsCOMPtr<nsIDOMNode> pointTextNode(do_QueryInterface(aPoint.mTextNode));
  PRInt32 idx = mNodeArray.IndexOf(pointTextNode);
  if (idx == -1) return NS_OK;  // can't find point, but it's not an error
  PRInt32 numNodes = mNodeArray.Count();
  
  if (aPoint.mOffset < aPoint.mTextNode->TextLength())
  {
    *outPoint = aPoint;
    outPoint->mChar = GetCharAt(aPoint.mTextNode, aPoint.mOffset);
  }
  else if (idx < (PRInt32)(numNodes-1))
  {
    nsIDOMNode* node = mNodeArray[idx+1];
    if (!node) return NS_ERROR_FAILURE;
    outPoint->mTextNode = do_QueryInterface(node);
    if (!outPoint->mTextNode->IsNodeOfType(nsINode::eDATA_NODE)) {
      // Not sure if this is needed, but it'll maintain the same
      // functionality
      outPoint->mTextNode = nsnull;
    }
    outPoint->mOffset = 0;
    outPoint->mChar = GetCharAt(outPoint->mTextNode, 0);
  }
  return NS_OK;
}

nsresult 
nsWSRunObject::GetCharBefore(WSPoint &aPoint, WSPoint *outPoint)
{
  if (!aPoint.mTextNode || !outPoint)
    return NS_ERROR_NULL_POINTER;
  
  outPoint->mTextNode = nsnull;
  outPoint->mOffset = 0;
  outPoint->mChar = 0;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> pointTextNode(do_QueryInterface(aPoint.mTextNode));
  PRInt32 idx = mNodeArray.IndexOf(pointTextNode);
  if (idx == -1) return NS_OK;  // can't find point, but it's not an error
  
  if (aPoint.mOffset != 0)
  {
    *outPoint = aPoint;
    outPoint->mOffset--;
    outPoint->mChar = GetCharAt(aPoint.mTextNode, aPoint.mOffset-1);
  }
  else if (idx)
  {
    nsIDOMNode* node = mNodeArray[idx-1];
    if (!node) return NS_ERROR_FAILURE;
    outPoint->mTextNode = do_QueryInterface(node);

    PRUint32 len = outPoint->mTextNode->TextLength();

    if (len)
    {
      outPoint->mOffset = len-1;
      outPoint->mChar = GetCharAt(outPoint->mTextNode, len-1);
    }
  }
  return NS_OK;
}

nsresult 
nsWSRunObject::ConvertToNBSP(WSPoint aPoint, AreaRestriction aAR)
{
  // MOOSE: this routine needs to be modified to preserve the integrity of the
  // wsFragment info.
  if (!aPoint.mTextNode)
    return NS_ERROR_NULL_POINTER;

  if (aAR == eOutsideUserSelectAll)
  {
    nsCOMPtr<nsIDOMNode> domnode = do_QueryInterface(aPoint.mTextNode);
    if (domnode)
    {
      nsCOMPtr<nsIDOMNode> san = mHTMLEditor->FindUserSelectAllNode(domnode);
      if (san)
        return NS_OK;
    }
  }

  nsCOMPtr<nsIDOMCharacterData> textNode(do_QueryInterface(aPoint.mTextNode));
  if (!textNode)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(textNode));
  
  // first, insert an nbsp
  nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
  nsAutoString nbspStr(nbsp);
  nsresult res = mHTMLEditor->InsertTextIntoTextNodeImpl(nbspStr, textNode, aPoint.mOffset, PR_TRUE);
  NS_ENSURE_SUCCESS(res, res);
  
  // next, find range of ws it will replace
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset=0, endOffset=0;
  
  res = GetAsciiWSBounds(eAfter, node, aPoint.mOffset+1, address_of(startNode), 
                         &startOffset, address_of(endNode), &endOffset);
  NS_ENSURE_SUCCESS(res, res);
  
  // finally, delete that replaced ws, if any
  if (startNode)
  {
    res = DeleteChars(startNode, startOffset, endNode, endOffset);
  }
  
  return res;
}

nsresult
nsWSRunObject::GetAsciiWSBounds(PRInt16 aDir, nsIDOMNode *aNode, PRInt32 aOffset,
                                nsCOMPtr<nsIDOMNode> *outStartNode, PRInt32 *outStartOffset,
                                nsCOMPtr<nsIDOMNode> *outEndNode, PRInt32 *outEndOffset)
{
  if (!aNode || !outStartNode || !outEndNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset=0, endOffset=0;
  
  nsresult res = NS_OK;
  
  if (aDir & eAfter)
  {
    WSPoint point, tmp;
    res = GetCharAfter(aNode, aOffset, &point);
    if (NS_SUCCEEDED(res) && point.mTextNode)
    {  // we found a text node, at least
      endNode = do_QueryInterface(point.mTextNode);
      endOffset = point.mOffset;
      startNode = endNode;
      startOffset = endOffset;
      
      // scan ahead to end of ascii ws
      while (nsCRT::IsAsciiSpace(point.mChar))
      {
        endNode = do_QueryInterface(point.mTextNode);
        point.mOffset++;  // endOffset is _after_ ws
        endOffset = point.mOffset;
        tmp = point;
        res = GetCharAfter(tmp, &point);
        if (NS_FAILED(res) || !point.mTextNode) break;
      }
    }
  }
  
  if (aDir & eBefore)
  {
    WSPoint point, tmp;
    res = GetCharBefore(aNode, aOffset, &point);
    if (NS_SUCCEEDED(res) && point.mTextNode)
    {  // we found a text node, at least
      startNode = do_QueryInterface(point.mTextNode);
      startOffset = point.mOffset+1;
      if (!endNode)
      {
        endNode = startNode;
        endOffset = startOffset;
      }
      
      // scan back to start of ascii ws
      while (nsCRT::IsAsciiSpace(point.mChar))
      {
        startNode = do_QueryInterface(point.mTextNode);
        startOffset = point.mOffset;
        tmp = point;
        res = GetCharBefore(tmp, &point);
        if (NS_FAILED(res) || !point.mTextNode) break;
      }
    }
  }  
  
  *outStartNode = startNode;
  *outStartOffset = startOffset;
  *outEndNode = endNode;
  *outEndOffset = endOffset;

  return NS_OK;
}

nsresult
nsWSRunObject::FindRun(nsIDOMNode *aNode, PRInt32 aOffset, WSFragment **outRun, PRBool after)
{
  // given a dompoint, find the ws run that is before or after it, as caller needs
  if (!aNode || !outRun)
    return NS_ERROR_NULL_POINTER;
    
  nsresult res = NS_OK;
  WSFragment *run = mStartRun;
  while (run)
  {
    PRInt16 comp = mHTMLEditor->sRangeHelper->ComparePoints(aNode, aOffset, run->mStartNode, run->mStartOffset);
    if (comp <= 0)
    {
      if (after)
      {
        *outRun = run;
        return res;
      }
      else // before
      {
        *outRun = nsnull;
        return res;
      }
    }
    comp = mHTMLEditor->sRangeHelper->ComparePoints(aNode, aOffset, run->mEndNode, run->mEndOffset);
    if (comp < 0)
    {
      *outRun = run;
      return res;
    }
    else if (comp == 0)
    {
      if (after)
      {
        *outRun = run->mRight;
        return res;
      }
      else // before
      {
        *outRun = run;
        return res;
      }
    }
    if (!run->mRight)
    {
      if (after)
      {
        *outRun = nsnull;
        return res;
      }
      else // before
      {
        *outRun = run;
        return res;
      }
    }
    run = run->mRight;
  }
  return res;
}

PRUnichar 
nsWSRunObject::GetCharAt(nsIContent *aTextNode, PRInt32 aOffset)
{
  // return 0 if we can't get a char, for whatever reason
  if (!aTextNode)
    return 0;

  PRUint32 len = aTextNode->TextLength();
  if (aOffset < 0 || aOffset >= len) 
    return 0;
    
  return aTextNode->GetText()->CharAt(aOffset);
}

nsresult 
nsWSRunObject::GetWSPointAfter(nsIDOMNode *aNode, PRInt32 aOffset, WSPoint *outPoint)
{
  // Note: only to be called if aNode is not a ws node.  
  
  // binary search on wsnodes
  PRInt32 numNodes, firstNum, curNum, lastNum;
  numNodes = mNodeArray.Count();
  
  if (!numNodes) 
    return NS_OK; // do nothing if there are no nodes to search

  firstNum = 0;
  curNum = numNodes/2;
  lastNum = numNodes;
  PRInt16 cmp=0;
  nsCOMPtr<nsIDOMNode>  curNode;
  
  // begin binary search
  // we do this because we need to minimize calls to ComparePoints(),
  // which is mongo expensive
  while (curNum != lastNum)
  {
    curNode = mNodeArray[curNum];
    cmp = mHTMLEditor->sRangeHelper->ComparePoints(aNode, aOffset, curNode, 0);
    if (cmp < 0)
      lastNum = curNum;
    else
      firstNum = curNum + 1;
    curNum = (lastNum - firstNum) / 2 + firstNum;
    NS_ASSERTION(firstNum <= curNum && curNum <= lastNum, "Bad binary search");
  }

  // When the binary search is complete, we always know that the current node
  // is the same as the end node, which is always past our range. Therefore,
  // we've found the node immediately after the point of interest.
  if (curNum == mNodeArray.Count()) {
    // they asked for past our range (it's after the last node). GetCharAfter
    // will do the work for us when we pass it the last index of the last node.
    nsCOMPtr<nsIContent> textNode(do_QueryInterface(mNodeArray[curNum-1]));
    WSPoint point(textNode, textNode->TextLength(), 0);
    return GetCharAfter(point, outPoint);
  } else {
    // The char after the point of interest is the first character of our range.
    nsCOMPtr<nsIContent> textNode(do_QueryInterface(mNodeArray[curNum]));
    WSPoint point(textNode, 0, 0);
    return GetCharAfter(point, outPoint);
  }
}

nsresult 
nsWSRunObject::GetWSPointBefore(nsIDOMNode *aNode, PRInt32 aOffset, WSPoint *outPoint)
{
  // Note: only to be called if aNode is not a ws node.  
  
  // binary search on wsnodes
  PRInt32 numNodes, firstNum, curNum, lastNum;
  numNodes = mNodeArray.Count();
  
  if (!numNodes) 
    return NS_OK; // do nothing if there are no nodes to search
  
  firstNum = 0;
  curNum = numNodes/2;
  lastNum = numNodes;
  PRInt16 cmp=0;
  nsCOMPtr<nsIDOMNode>  curNode;
  
  // begin binary search
  // we do this because we need to minimize calls to ComparePoints(),
  // which is mongo expensive
  while (curNum != lastNum)
  {
    curNode = mNodeArray[curNum];
    cmp = mHTMLEditor->sRangeHelper->ComparePoints(aNode, aOffset, curNode, 0);
    if (cmp < 0)
      lastNum = curNum;
    else
      firstNum = curNum + 1;
    curNum = (lastNum - firstNum) / 2 + firstNum;
    NS_ASSERTION(firstNum <= curNum && curNum <= lastNum, "Bad binary search");
  }

  // When the binary search is complete, we always know that the current node
  // is the same as the end node, which is always past our range. Therefore,
  // we've found the node immediately after the point of interest.
  if (curNum == mNodeArray.Count()) {
    // get the point before the end of the last node, we can pass the length
    // of the node into GetCharBefore, and it will return the last character.
    nsCOMPtr<nsIContent> textNode(do_QueryInterface(mNodeArray[curNum - 1]));
    WSPoint point(textNode, textNode->TextLength(), 0);
    return GetCharBefore(point, outPoint);
  } else {
    // we can just ask the current node for the point immediately before it,
    // it will handle moving to the previous node (if any) and returning the
    // appropriate character
    nsCOMPtr<nsIContent> textNode(do_QueryInterface(mNodeArray[curNum]));
    WSPoint point(textNode, 0, 0);
    return GetCharBefore(point, outPoint);
  }
}

nsresult
nsWSRunObject::CheckTrailingNBSPOfRun(WSFragment *aRun)
{    
  // try to change an nbsp to a space, if possible, just to prevent nbsp proliferation. 
  // examine what is before and after the trailing nbsp, if any.
  if (!aRun) return NS_ERROR_NULL_POINTER;
  WSPoint thePoint;
  PRBool leftCheck = PR_FALSE;
  PRBool spaceNBSP = PR_FALSE;
  PRBool rightCheck = PR_FALSE;
  
  // confirm run is normalWS
  if (aRun->mType != eNormalWS) return NS_ERROR_FAILURE;
  
  // first check for trailing nbsp
  nsresult res = GetCharBefore(aRun->mEndNode, aRun->mEndOffset, &thePoint);
  if (NS_SUCCEEDED(res) && thePoint.mTextNode && thePoint.mChar == nbsp)
  {
    // now check that what is to the left of it is compatible with replacing nbsp with space
    WSPoint prevPoint;
    res = GetCharBefore(thePoint, &prevPoint);
    if (NS_SUCCEEDED(res) && prevPoint.mTextNode)
    {
      if (!nsCRT::IsAsciiSpace(prevPoint.mChar)) leftCheck = PR_TRUE;
      else spaceNBSP = PR_TRUE;
    }
    else if (aRun->mLeftType == eText)    leftCheck = PR_TRUE;
    else if (aRun->mLeftType == eSpecial) leftCheck = PR_TRUE;
    if (leftCheck || spaceNBSP)
    {
      // now check that what is to the right of it is compatible with replacing nbsp with space
      if (aRun->mRightType == eText)    rightCheck = PR_TRUE;
      if (aRun->mRightType == eSpecial) rightCheck = PR_TRUE;
      if (aRun->mRightType == eBreak)   rightCheck = PR_TRUE;
      if (aRun->mRightType & eBlock)
      {
        // we are at a block boundary.  Insert a <br>.  Why?  Well, first note that
        // the br will have no visible effect since it is up against a block boundary.
        // |foo<br><p>bar|  renders like |foo<p>bar| and similarly
        // |<p>foo<br></p>bar| renders like |<p>foo</p>bar|.  What this <br> addition
        // gets us is the ability to convert a trailing nbsp to a space.  Consider:
        // |<body>foo. '</body>|, where ' represents selection.  User types space attempting
        // to put 2 spaces after the end of their sentence.  We used to do this as:
        // |<body>foo. &nbsp</body>|  This caused problems with soft wrapping: the nbsp
        // would wrap to the next line, which looked attrocious.  If you try to do:
        // |<body>foo.&nbsp </body>| instead, the trailing space is invisible because it 
        // is against a block boundary.  If you do: |<body>foo.&nbsp&nbsp</body>| then
        // you get an even uglier soft wrapping problem, where foo is on one line until
        // you type the final space, and then "foo  " jumps down to the next line.  Ugh.
        // The best way I can find out of this is to throw in a harmless <br>
        // here, which allows us to do: |<body>foo.&nbsp <br></body>|, which doesn't
        // cause foo to jump lines, doesn't cause spaces to show up at the beginning of 
        // soft wrapped lines, and lets the user see 2 spaces when they type 2 spaces.
        
        nsCOMPtr<nsIDOMNode> brNode;
        res = mHTMLEditor->CreateBR(aRun->mEndNode, aRun->mEndOffset, address_of(brNode));
        NS_ENSURE_SUCCESS(res, res);
        
        // refresh thePoint, prevPoint
        res = GetCharBefore(aRun->mEndNode, aRun->mEndOffset, &thePoint);
        NS_ENSURE_SUCCESS(res, res);
        res = GetCharBefore(thePoint, &prevPoint);
        NS_ENSURE_SUCCESS(res, res);
        rightCheck = PR_TRUE;
      }
    }
    if (leftCheck && rightCheck)
    {
      // now replace nbsp with space
      // first, insert a space
      nsCOMPtr<nsIDOMCharacterData> textNode(do_QueryInterface(thePoint.mTextNode));
      if (!textNode)
        return NS_ERROR_NULL_POINTER;
      nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
      nsAutoString spaceStr(PRUnichar(32));
      res = mHTMLEditor->InsertTextIntoTextNodeImpl(spaceStr, textNode, thePoint.mOffset, PR_TRUE);
      NS_ENSURE_SUCCESS(res, res);
  
      // finally, delete that nbsp
      nsCOMPtr<nsIDOMNode> delNode(do_QueryInterface(thePoint.mTextNode));
      res = DeleteChars(delNode, thePoint.mOffset+1, delNode, thePoint.mOffset+2);
      NS_ENSURE_SUCCESS(res, res);
    }
    else if (!mPRE && spaceNBSP && rightCheck)  // don't mess with this preformatted for now.
    {
      // we have a run of ascii whitespace (which will render as one space)
      // followed by an nbsp (which is at the end of the whitespace run).  Let's
      // switch their order.  This will insure that if someone types two spaces
      // after a sentence, and the editor softwraps at this point, the spaces wont
      // be split across lines, which looks ugly and is bad for the moose.
      
      nsCOMPtr<nsIDOMNode> startNode, endNode, thenode(do_QueryInterface(prevPoint.mTextNode));
      PRInt32 startOffset, endOffset;
      res = GetAsciiWSBounds(eBoth, thenode, prevPoint.mOffset+1, address_of(startNode), 
                           &startOffset, address_of(endNode), &endOffset);
      NS_ENSURE_SUCCESS(res, res);
      
      //  delete that nbsp
      nsCOMPtr<nsIDOMNode> delNode(do_QueryInterface(thePoint.mTextNode));
      res = DeleteChars(delNode, thePoint.mOffset, delNode, thePoint.mOffset+1);
      NS_ENSURE_SUCCESS(res, res);
      
      // finally, insert that nbsp before the ascii ws run
      nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
      nsAutoString nbspStr(nbsp);
      nsCOMPtr<nsIDOMCharacterData> textNode(do_QueryInterface(startNode));
      res = mHTMLEditor->InsertTextIntoTextNodeImpl(nbspStr, textNode, startOffset, PR_TRUE);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}

nsresult
nsWSRunObject::CheckTrailingNBSP(WSFragment *aRun, nsIDOMNode *aNode, PRInt32 aOffset)
{    
  // try to change an nbsp to a space, if possible, just to prevent nbsp proliferation. 
  // this routine is called when we about to make this point in the ws abut an inserted break
  // or text, so we don't have to worry about what is after it.  What is after it now will 
  // end up after the inserted object.   
  if (!aRun || !aNode) return NS_ERROR_NULL_POINTER;
  WSPoint thePoint;
  PRBool canConvert = PR_FALSE;
  nsresult res = GetCharBefore(aNode, aOffset, &thePoint);
  if (NS_SUCCEEDED(res) && thePoint.mTextNode && thePoint.mChar == nbsp)
  {
    WSPoint prevPoint;
    res = GetCharBefore(thePoint, &prevPoint);
    if (NS_SUCCEEDED(res) && prevPoint.mTextNode)
    {
      if (!nsCRT::IsAsciiSpace(prevPoint.mChar)) canConvert = PR_TRUE;
    }
    else if (aRun->mLeftType == eText)    canConvert = PR_TRUE;
    else if (aRun->mLeftType == eSpecial) canConvert = PR_TRUE;
  }
  if (canConvert)
  {
    // first, insert a space
    nsCOMPtr<nsIDOMCharacterData> textNode(do_QueryInterface(thePoint.mTextNode));
    if (!textNode)
      return NS_ERROR_NULL_POINTER;
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    nsAutoString spaceStr(PRUnichar(32));
    res = mHTMLEditor->InsertTextIntoTextNodeImpl(spaceStr, textNode, thePoint.mOffset, PR_TRUE);
    NS_ENSURE_SUCCESS(res, res);
  
    // finally, delete that nbsp
    nsCOMPtr<nsIDOMNode> delNode(do_QueryInterface(thePoint.mTextNode));
    res = DeleteChars(delNode, thePoint.mOffset+1, delNode, thePoint.mOffset+2);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

nsresult
nsWSRunObject::CheckLeadingNBSP(WSFragment *aRun, nsIDOMNode *aNode, PRInt32 aOffset)
{    
  // try to change an nbsp to a space, if possible, just to prevent nbsp proliferation    
  // this routine is called when we about to make this point in the ws abut an inserted
  // text, so we don't have to worry about what is before it.  What is before it now will 
  // end up before the inserted text.   
  WSPoint thePoint;
  PRBool canConvert = PR_FALSE;
  nsresult res = GetCharAfter(aNode, aOffset, &thePoint);
  if (NS_SUCCEEDED(res) && thePoint.mChar == nbsp)
  {
    WSPoint nextPoint, tmp=thePoint;
    tmp.mOffset++; // we want to be after thePoint
    res = GetCharAfter(tmp, &nextPoint);
    if (NS_SUCCEEDED(res) && nextPoint.mTextNode)
    {
      if (!nsCRT::IsAsciiSpace(nextPoint.mChar)) canConvert = PR_TRUE;
    }
    else if (aRun->mRightType == eText)    canConvert = PR_TRUE;
    else if (aRun->mRightType == eSpecial) canConvert = PR_TRUE;
    else if (aRun->mRightType == eBreak)   canConvert = PR_TRUE;
  }
  if (canConvert)
  {
    // first, insert a space
    nsCOMPtr<nsIDOMCharacterData> textNode(do_QueryInterface(thePoint.mTextNode));
    if (!textNode)
      return NS_ERROR_NULL_POINTER;
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    nsAutoString spaceStr(PRUnichar(32));
    res = mHTMLEditor->InsertTextIntoTextNodeImpl(spaceStr, textNode, thePoint.mOffset, PR_TRUE);
    NS_ENSURE_SUCCESS(res, res);
  
    // finally, delete that nbsp
    nsCOMPtr<nsIDOMNode> delNode(do_QueryInterface(thePoint.mTextNode));
    res = DeleteChars(delNode, thePoint.mOffset+1, delNode, thePoint.mOffset+2);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}


nsresult
nsWSRunObject::ScrubBlockBoundaryInner(nsHTMLEditor *aHTMLEd, 
                                       nsCOMPtr<nsIDOMNode> *aBlock,
                                       BlockBoundary aBoundary)
{
  if (!aBlock || !aHTMLEd)
    return NS_ERROR_NULL_POINTER;
  PRInt32 offset=0;
  if (aBoundary == kBlockEnd)
  {
    PRUint32 uOffset;
    aHTMLEd->GetLengthOfDOMNode(*aBlock, uOffset); 
    offset = uOffset;
  }
  nsWSRunObject theWSObj(aHTMLEd, *aBlock, offset);
  return theWSObj.Scrub();    
}


nsresult
nsWSRunObject::Scrub()
{
  WSFragment *run = mStartRun;
  while (run)
  {
    if (run->mType & (eLeadingWS|eTrailingWS) )
    {
      nsresult res = DeleteChars(run->mStartNode, run->mStartOffset, run->mEndNode, run->mEndOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
    run = run->mRight;
  }
  return NS_OK;
}

