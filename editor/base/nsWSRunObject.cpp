/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// Define _IMPL_NS_LAYOUT to avoid link errors having to do with
// nsTextFragment methods, on Win32 platforms.
//
#define _IMPL_NS_LAYOUT
#include "nsTextFragment.h"
#undef _IMPL_NS_LAYOUT

#include "nsWSRunObject.h"
#include "nsISupportsArray.h"
#include "nsIDOMNode.h"
//#include "nsIModule.h"
#include "nsHTMLEditor.h"
#include "nsHTMLEditUtils.h"
#include "nsTextEditUtils.h"
#include "nsIContent.h"
#include "nsLayoutCID.h"
#include "nsIDOMCharacterData.h"
#include "nsEditorUtils.h"

const PRUnichar nbsp = 160;

static NS_DEFINE_CID(kCRangeCID, NS_RANGE_CID);

static PRBool IsBlockNode(nsIDOMNode* node)
{
  PRBool isBlock (PR_FALSE);
  nsHTMLEditor::NodeIsBlockStatic(node, &isBlock);
  return isBlock;
}

static PRBool IsInlineNode(nsIDOMNode* node)
{
  return !IsBlockNode(node);
}

//- constructor / destructor -----------------------------------------------
nsWSRunObject::nsWSRunObject(nsHTMLEditor *aEd) :
mNode()
,mOffset(0)
,mStartNode()
,mStartOffset(0)
,mStartReason(0)
,mEndNode()
,mEndOffset(0)
,mEndReason(0)
,mFirstNBSPNode()
,mFirstNBSPOffset(0)
,mLastNBSPNode()
,mLastNBSPOffset(0)
,mNodeArray()
,mStartRun(nsnull)
,mEndRun(nsnull)
,mHTMLEditor(aEd)
{
  mNodeArray = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID);
}

nsWSRunObject::nsWSRunObject(nsHTMLEditor *aEd, nsIDOMNode *aNode, PRInt32 aOffset) :
mNode(aNode)
,mOffset(aOffset)
,mStartNode()
,mStartOffset(0)
,mStartReason(0)
,mEndNode()
,mEndOffset(0)
,mEndReason(0)
,mFirstNBSPNode()
,mFirstNBSPOffset(0)
,mLastNBSPNode()
,mLastNBSPOffset(0)
,mNodeArray()
,mStartRun(nsnull)
,mEndRun(nsnull)
,mHTMLEditor(aEd)
{
  mNodeArray = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID);
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
nsWSRunObject::PrepareToJoinBlocks(nsHTMLEditor *aHTMLEd, 
                                   nsIDOMNode *aLeftParent, 
                                   nsIDOMNode *aRightParent)
{
  if (!aLeftParent || !aRightParent || !aHTMLEd)
    return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
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
  nsresult res = NS_OK;

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
  nsresult res = NS_OK;

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
  
  // handle any changes needed to ws run after inserted br
  if (!afterRun)
  {
    // dont need to do anything.  just insert break.  ws wont change.
  }
  else if (afterRun->mType & eTrailingWS)
  {
    // dont need to do anything.  just insert break.  ws wont change.
  }
  else if (afterRun->mType == eLeadingWS)
  {
    // delete the leading ws that is after insertion point.  We don't
    // have to (it would still not be significant after br), but it's 
    // just more aesthetically pleasing to.
    res = DeleteChars(*aInOutParent, *aInOutOffset, afterRun->mEndNode, afterRun->mEndOffset);
    NS_ENSURE_SUCCESS(res, res);
  }
  else if (afterRun->mType == eNormalWS)
  {
    // need to determine if break at front of non-nbsp run.  if so
    // convert run to nbsp.
    WSPoint thePoint;
    res = GetCharAfter(*aInOutParent, *aInOutOffset, &thePoint);
    if ( (NS_SUCCEEDED(res)) && (nsCRT::IsAsciiSpace(thePoint.mChar)) )
    {
      WSPoint prevPoint;
      res = GetCharBefore(thePoint, &prevPoint);
      if ( (NS_FAILED(res)) || (!nsCRT::IsAsciiSpace(prevPoint.mChar)) )
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
    // dont need to do anything.  just insert break.  ws wont change.
  }
  else if (beforeRun->mType & eLeadingWS)
  {
    // dont need to do anything.  just insert break.  ws wont change.
  }
  else if (beforeRun->mType == eTrailingWS)
  {
    // need to delete the trailing ws that is before insertion point, because it 
    // would become significant after break inserted.
    res = DeleteChars(beforeRun->mStartNode, beforeRun->mStartOffset, *aInOutParent, *aInOutOffset);
    NS_ENSURE_SUCCESS(res, res);
  }
  else if (beforeRun->mType == eNormalWS)
  {
    // try to change an nbsp to a space, if possible, just to prevent nbsp proliferation
    res = CheckTrailingNBSP(beforeRun, *aInOutParent, *aInOutOffset);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  // ready, aim, fire!
  return mHTMLEditor->CreateBRImpl(aInOutParent, aInOutOffset, outBRNode, aSelect);
}

nsresult 
nsWSRunObject::InsertText(const nsAReadableString& aStringToInsert, 
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
  
  // handle any changes needed to ws run after inserted text
  if (!afterRun)
  {
    // dont need to do anything.  just insert text.  ws wont change.
  }
  else if (afterRun->mType & eTrailingWS)
  {
    // dont need to do anything.  just insert text.  ws wont change.
  }
  else if (afterRun->mType == eLeadingWS)
  {
    // delete the leading ws that is after insertion point, because it 
    // would become significant after text inserted.
    res = DeleteChars(*aInOutParent, *aInOutOffset, afterRun->mEndNode, afterRun->mEndOffset);
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
    // dont need to do anything.  just insert text.  ws wont change.
  }
  else if (beforeRun->mType & eLeadingWS)
  {
    // dont need to do anything.  just insert text.  ws wont change.
  }
  else if (beforeRun->mType == eTrailingWS)
  {
    // need to delete the trailing ws that is before insertion point, because it 
    // would become significant after text inserted.
    res = DeleteChars(beforeRun->mStartNode, beforeRun->mStartOffset, *aInOutParent, *aInOutOffset);
    NS_ENSURE_SUCCESS(res, res);
  }
  else if (beforeRun->mType == eNormalWS)
  {
    // try to change an nbsp to a space, if possible, just to prevent nbsp proliferation
    res = CheckTrailingNBSP(beforeRun, *aInOutParent, *aInOutOffset);
    NS_ENSURE_SUCCESS(res, res);
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
        if (NS_SUCCEEDED(res) && nsCRT::IsAsciiSpace(wspoint.mChar))
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
        if (NS_SUCCEEDED(res) && nsCRT::IsAsciiSpace(wspoint.mChar))
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
  PRInt32 j;
  PRBool prevWS = PR_FALSE;
  for (j=0; j<=lastCharIndex; j++)
  {
    if (nsCRT::IsAsciiSpace(theString[j]))
    {
      if (prevWS)
      {
        theString.SetCharAt(nbsp, j-1);  // j-1 cant be negative because prevWS starts out false
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
    
  WSFragment *run;
  nsresult res = FindRun(aNode, aOffset, &run, PR_FALSE);
  
  // is there a visible run there or earlier?
  while (run)
  {
    if (run->mType == eNormalWS)
    {
      WSPoint point;
      res = GetCharBefore(aNode, aOffset, &point);
      NS_ENSURE_SUCCESS(res, res);
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
  *outVisNode = mStartNode;
  *outVisOffset = mStartOffset;
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
  // Find first visible thing before the point.  position outVisNode/outVisOffset
  // just _before_ that thing.  If we don't find anything return end of ws.
  if (!aNode || !outVisNode || !outVisOffset || !outType)
    return NS_ERROR_NULL_POINTER;
    
  WSFragment *run;
  nsresult res = FindRun(aNode, aOffset, &run, PR_TRUE);
  
  // is there a visible run there or later?
  while (run)
  {
    if (run->mType == eNormalWS)
    {
      WSPoint point;
      res = GetCharAfter(aNode, aOffset, &point);
      NS_ENSURE_SUCCESS(res, res);
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
  *outVisNode = mEndNode;
  *outVisOffset = mEndOffset;
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
  
  nsCOMPtr<nsIDOMNode> blockParent, curStartNode, curEndNode;
  PRInt32 curStartOffset, curEndOffset;
  if (IsBlockNode(mNode)) blockParent = mNode;
  else blockParent = mHTMLEditor->GetBlockNodeParent(mNode);

  // first look backwards to find preceding ws nodes
  if (mHTMLEditor->IsTextNode(mNode))
  {
    nsCOMPtr<nsITextContent> textNode(do_QueryInterface(mNode));
    const nsTextFragment *textFrag;
    res = textNode->GetText(&textFrag);
    NS_ENSURE_SUCCESS(res, res);
    
    res = PrependNodeToList(mNode);
    NS_ENSURE_SUCCESS(res, res);
    if (mOffset)
    {
      PRInt32 pos;
      for (pos=mOffset-1; pos>=0; pos--)
      {
        PRUnichar theChar = textFrag->CharAt(pos);
        if (!nsCRT::IsAsciiSpace(theChar))
        {
          if (theChar != nbsp)
          {
            mStartNode = mNode;
            mStartOffset = pos+1;
            mStartReason = eText;
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
      }
    }
    if (!mStartNode)
    {
      // we didnt find beginning of whitespace.  remember this text node
      // (and offset 0) as the extent to which we have looked back.
      curStartNode = mNode;
      curStartOffset = 0;
    }
  }

  nsCOMPtr<nsIDOMNode> priorNode;
  while (!mStartNode)
  {
    // we haven't found the start of ws yet.  Keep looking
    
    // do we have a curStartNode? If not, get one.
    if (!curStartNode)
    {
      res = GetPreviousWSNode(mNode, mOffset, blockParent, address_of(curStartNode));
      NS_ENSURE_SUCCESS(res, res);
      priorNode = curStartNode;
    }
    else
    {
      res = GetPreviousWSNode(curStartNode, blockParent, address_of(priorNode));
      NS_ENSURE_SUCCESS(res, res);
    }
    if (priorNode)
    {
      if (IsBlockNode(priorNode))
      {
        // we encountered a new block.  therefore no more ws.
        if (mHTMLEditor->IsTextNode(curStartNode))
        {
          mStartNode = curStartNode;
          mStartOffset = curStartOffset;
        }
        else
        {
          mHTMLEditor->GetNodeLocation(curStartNode, address_of(mStartNode), &mStartOffset);
          mStartOffset++;
        }
        mStartReason = eOtherBlock;
      }
      else if (mHTMLEditor->IsTextNode(priorNode))
      {
        res = PrependNodeToList(priorNode);
        NS_ENSURE_SUCCESS(res, res);
        nsCOMPtr<nsITextContent> textNode(do_QueryInterface(priorNode));
        if (!textNode) return NS_ERROR_NULL_POINTER;
        const nsTextFragment *textFrag;
        res = textNode->GetText(&textFrag);
        PRInt32 len;
        res = textNode->GetTextLength(&len);
        NS_ENSURE_SUCCESS(res, res);
        PRInt32 pos;
        for (pos=len-1; pos>=0; pos--)
        {
          PRUnichar theChar = textFrag->CharAt(pos);
          if (!nsCRT::IsAsciiSpace(theChar))
          {
            if (theChar != nbsp)
            {
              mStartNode = priorNode;
              mStartOffset = pos+1;
              mStartReason = eText;
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
        }
        if (!mStartNode)
        {
          // we didnt find beginning of whitespace.  remember this text node
          // (and offset 0) as the extent to which we have looked back.
          curStartNode = priorNode;
          curStartOffset = 0;
        }
      }
      else if (nsTextEditUtils::IsBreak(priorNode))
      {
        // we encountered a break.  therefore no more ws.
        if (mHTMLEditor->IsTextNode(curStartNode))
        {
          mStartNode = curStartNode;
          mStartOffset = curStartOffset;
        }
        else
        {
          mHTMLEditor->GetNodeLocation(curStartNode, address_of(mStartNode), &mStartOffset);
          mStartOffset++;
        }
        mStartReason = eBreak;
      }
      else
      {
        // it's a special node, like <img>, that is not a block and not
        // a break but still serves as a terminator to ws runs.
        if (mHTMLEditor->IsTextNode(curStartNode))
        {
          mStartNode = curStartNode;
          mStartOffset = curStartOffset;
        }
        else
        {
          mHTMLEditor->GetNodeLocation(curStartNode, address_of(mStartNode), &mStartOffset);
          mStartOffset++;
        }
        mStartReason = eSpecial;
      }
    }
    else
    {
      // no prior node means we exhausted blockParent
      if (!curStartNode)
      {
        // we never found anything to work with, so start 
        // is at beginning of block parent
        mStartNode = blockParent;
        mStartOffset = 0;
      }
      else if (mHTMLEditor->IsTextNode(curStartNode))
      {
        mStartNode = curStartNode;
        mStartOffset = curStartOffset;
      }
      else
      {
        mHTMLEditor->GetNodeLocation(curStartNode, address_of(mStartNode), &mStartOffset);
        mStartOffset++;
      }
      mStartReason = eThisBlock;
    } 
  }
  
  // then look ahead to find following ws nodes
  if (mHTMLEditor->IsTextNode(mNode))
  {
    // dont need to put it on list. it already is from code above
    nsCOMPtr<nsITextContent> textNode(do_QueryInterface(mNode));
    const nsTextFragment *textFrag;
    res = textNode->GetText(&textFrag);
    NS_ENSURE_SUCCESS(res, res);
    PRInt32 len;
    textNode->GetTextLength(&len);
    if (mOffset<len)
    {
      PRInt32 pos;
      for (pos=mOffset; pos<len; pos++)
      {
        PRUnichar theChar = textFrag->CharAt(pos);
        if (!nsCRT::IsAsciiSpace(theChar))
        {
          if (theChar != nbsp)
          {
            mEndNode = mNode;
            mEndOffset = pos;
            mEndReason = eText;
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
      }
    }
    if (!mEndNode)
    {
      // we didnt find end of whitespace.  remember this text node
      // (and offset len) as the extent to which we have looked ahead.
      curEndNode = mNode;
      curEndOffset = len;
    }
  }

  nsCOMPtr<nsIDOMNode> nextNode;
  while (!mEndNode)
  {
    // we haven't found the end of ws yet.  Keep looking

    // do we have a curEndNode? If not, get one.
    if (!curEndNode)
    {
      res = GetNextWSNode(mNode, mOffset, blockParent, address_of(curEndNode));
      NS_ENSURE_SUCCESS(res, res);
      nextNode = curEndNode;
    }
    else
    {
      res = GetNextWSNode(curEndNode, blockParent, address_of(nextNode));
      NS_ENSURE_SUCCESS(res, res);
    }
    if (nextNode)
    {
      if (IsBlockNode(nextNode))
      {
        // we encountered a new block.  therefore no more ws.
        if (mHTMLEditor->IsTextNode(curEndNode))
        {
          mEndNode = curEndNode;
          mEndOffset = curEndOffset;
        }
        else
        {
          mHTMLEditor->GetNodeLocation(curEndNode, address_of(mEndNode), &mEndOffset);
        }
        mEndReason = eOtherBlock;
      }
      else if (mHTMLEditor->IsTextNode(nextNode))
      {
        res = AppendNodeToList(nextNode);
        NS_ENSURE_SUCCESS(res, res);
        nsCOMPtr<nsITextContent> textNode(do_QueryInterface(nextNode));
        if (!textNode) return NS_ERROR_NULL_POINTER;
        const nsTextFragment *textFrag;
        res = textNode->GetText(&textFrag);
        NS_ENSURE_SUCCESS(res, res);
        PRInt32 len;
        res = textNode->GetTextLength(&len);
        NS_ENSURE_SUCCESS(res, res);
        PRInt32 pos;
        for (pos=0; pos<len; pos++)
        {
          PRUnichar theChar = textFrag->CharAt(pos);
          if (!nsCRT::IsAsciiSpace(theChar))
          {
            if (theChar != nbsp)
            {
              mEndNode = nextNode;
              mEndOffset = pos;
              mEndReason = eText;
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
        }
        if (!mEndNode)
        {
          // we didnt find end of whitespace.  remember this text node
          // (and offset len) as the extent to which we have looked ahead.
          curEndNode = nextNode;
          curEndOffset = len;
        }
      }
      else if (nsTextEditUtils::IsBreak(nextNode))
      {
        // we encountered a break.  therefore no more ws.
        if (mHTMLEditor->IsTextNode(curEndNode))
        {
          mEndNode = curEndNode;
          mEndOffset = curEndOffset;
        }
        else
        {
          mHTMLEditor->GetNodeLocation(curEndNode, address_of(mEndNode), &mEndOffset);
        }
        mEndReason = eBreak;
      }
      else
      {
        // it's a special node, like <img>, that is not a block and not
        // a break but still serves as a terminator to ws runs.
        if (mHTMLEditor->IsTextNode(curEndNode))
        {
          mEndNode = curEndNode;
          mEndOffset = curEndOffset;
        }
        else
        {
          mHTMLEditor->GetNodeLocation(curEndNode, address_of(mEndNode), &mEndOffset);
        }
        mEndReason = eSpecial;
      }
    }
    else
    {
      // no next node means we exhausted blockParent
      if (!curEndNode)
      {
        // we never found anything to work with, so end 
        // is at end of block parent
        mEndNode = blockParent;
        PRUint32 count;
        mHTMLEditor->GetLengthOfDOMNode(blockParent, count);
        mEndOffset = count;
      }
      else if (mHTMLEditor->IsTextNode(curEndNode))
      {
        mEndNode = curEndNode;
        mEndOffset = curEndOffset;
      }
      else
      {
        mHTMLEditor->GetNodeLocation(curEndNode, address_of(mEndNode), &mEndOffset);
      }
      mEndReason = eThisBlock;
    } 
  }

  return NS_OK;
}

nsresult
nsWSRunObject::GetRuns()
{
  ClearRuns();
  
  // handle some easy cases first
  
  // if we are surrounded by text or special, it's all one
  // big normal ws run
  if ( ((mStartReason == eText) || (mStartReason == eSpecial)) &&
       ((mEndReason == eText) || (mEndReason == eSpecial) || (mEndReason == eBreak)) )
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
    }
    else
    {
      mStartRun->mRightType = mEndReason;
      mStartRun->mEndNode   = mEndNode;
      mStartRun->mEndOffset = mEndOffset;
      mEndRun = mStartRun;
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
  if (!aNode || !mNodeArray) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISupports> isupports (do_QueryInterface(aNode));
  nsresult res = mNodeArray->InsertElementAt(isupports,0);
  return res;
}

nsresult 
nsWSRunObject::AppendNodeToList(nsIDOMNode *aNode)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISupports> isupports (do_QueryInterface(aNode));
  nsresult res = mNodeArray->AppendElement(isupports);
  return res;
}

nsresult 
nsWSRunObject::GetPreviousWSNode(nsIDOMNode *aStartNode, 
                                 nsIDOMNode *aBlockParent, 
                                 nsCOMPtr<nsIDOMNode> *aPriorNode)
{
  // can't really recycle various getnext/prior routines because we have special needs
  // here.  Need to step into inline containers but not block containers.
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
nsWSRunObject::GetNextWSNode(nsIDOMNode *aStartNode, 
                             nsIDOMNode *aBlockParent, 
                             nsCOMPtr<nsIDOMNode> *aNextNode)
{
  // can't really recycle various getnext/prior routines because we have special needs
  // here.  Need to step into inline containers but not block containers.
  if (!aStartNode || !aBlockParent || !aNextNode) return NS_ERROR_NULL_POINTER;
  
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
      // we have exhausted nodes in the block parent.  The convention here is to return null.
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
nsWSRunObject::GetPreviousWSNode(nsIDOMNode *aStartNode,
                                PRInt16      aOffset, 
                                nsIDOMNode  *aBlockParent, 
                                nsCOMPtr<nsIDOMNode> *aPriorNode)
{
  // can't really recycle various getnext/prior routines because we have special needs
  // here.  Need to step into inline containers but not block containers.
  if (!aStartNode || !aBlockParent || !aPriorNode) return NS_ERROR_NULL_POINTER;
  *aPriorNode = 0;
  
  if (mHTMLEditor->IsTextNode(aStartNode))
    return GetPreviousWSNode(aStartNode, aBlockParent, aPriorNode);
  if (!mHTMLEditor->IsContainer(aStartNode))
    return GetPreviousWSNode(aStartNode, aBlockParent, aPriorNode);
  
  nsCOMPtr<nsIContent> priorContent, startContent( do_QueryInterface(aStartNode) );
  if (!aOffset)
  {
    if (aStartNode==aBlockParent)
    {
      // we are at start of the block.
      return NS_OK;
    }
    else
    {
      // we are at start of non-block container
      return GetPreviousWSNode(aStartNode, aBlockParent, aPriorNode);
    }
  }
  
  nsresult res = startContent->ChildAt(aOffset-1, *getter_AddRefs(priorContent));
  NS_ENSURE_SUCCESS(res, res);
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
                             PRInt16     aOffset, 
                             nsIDOMNode *aBlockParent, 
                             nsCOMPtr<nsIDOMNode> *aNextNode)
{
  // can't really recycle various getnext/prior routines because we have special needs
  // here.  Need to step into inline containers but not block containers.
  if (!aStartNode || !aBlockParent || !aNextNode) return NS_ERROR_NULL_POINTER;
  *aNextNode = 0;
  
  if (mHTMLEditor->IsTextNode(aStartNode))
    return GetNextWSNode(aStartNode, aBlockParent, aNextNode);
  if (!mHTMLEditor->IsContainer(aStartNode))
    return GetNextWSNode(aStartNode, aBlockParent, aNextNode);
  
  nsCOMPtr<nsIContent> nextContent, startContent( do_QueryInterface(aStartNode) );
  nsresult res = startContent->ChildAt(aOffset, *getter_AddRefs(nextContent));
  NS_ENSURE_SUCCESS(res, res);
  if (!nextContent)
  {
    if (aStartNode==aBlockParent)
    {
      // we are at end of the block.
      return NS_OK;
    }
    else
    {
      // we are at end of non-block container
      return GetNextWSNode(aStartNode, aBlockParent, aNextNode);
    }
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
  if (afterRun && (afterRun->mType == eLeadingWS))
  {
    res = aEndObject->DeleteChars(aEndObject->mNode, aEndObject->mOffset, afterRun->mEndNode, afterRun->mEndOffset);
    NS_ENSURE_SUCCESS(res, res);
  }
  // adjust normal ws in afterRun if needed
  if (afterRun && (afterRun->mType == eNormalWS))
  {
    if ( (beforeRun && (beforeRun->mType == eLeadingWS)) ||
         (!beforeRun && ((mStartReason & eBlock) || (mStartReason == eBreak))) )
    {
      // make sure leading char of following ws is an nbsp, so that it will show up
      WSPoint point;
      aEndObject->GetCharAfter(aEndObject->mNode, aEndObject->mOffset, &point);
      if (nsCRT::IsAsciiSpace(point.mChar))
      {
        res = ConvertToNBSP(point);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  // trim before run of any trailing ws
  if (beforeRun && (beforeRun->mType == eTrailingWS))
  {
    res = DeleteChars(beforeRun->mStartNode, beforeRun->mStartOffset, mNode, mOffset);
    NS_ENSURE_SUCCESS(res, res);
  }
  else if (beforeRun && (beforeRun->mType == eNormalWS))
  {
    if ( (afterRun && (afterRun->mType == eTrailingWS)) ||
         (afterRun && (afterRun->mType == eNormalWS))   ||
         (!afterRun && ((aEndObject->mEndReason & eBlock))) )
    {
      // make sure trailing char of starting ws is an nbsp, so that it will show up
      WSPoint point;
      GetCharBefore(mNode, mOffset, &point);
      if (nsCRT::IsAsciiSpace(point.mChar))
      {
        nsCOMPtr<nsIDOMNode> wsStartNode, wsEndNode;
        PRInt32 wsStartOffset, wsEndOffset;
        res = GetAsciiWSBounds(eBoth, mNode, mOffset, 
                               address_of(wsStartNode), &wsStartOffset, 
                               address_of(wsEndNode), &wsEndOffset);
        NS_ENSURE_SUCCESS(res, res);
        point.mTextNode = do_QueryInterface(wsStartNode);
        point.mOffset = wsStartOffset;
        res = ConvertToNBSP(point);
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
    if (nsCRT::IsAsciiSpace(point.mChar))
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
    if (nsCRT::IsAsciiSpace(point.mChar))
    {
      nsCOMPtr<nsIDOMNode> wsStartNode, wsEndNode;
      PRInt32 wsStartOffset, wsEndOffset;
      res = GetAsciiWSBounds(eBoth, mNode, mOffset, 
                             address_of(wsStartNode), &wsStartOffset, 
                             address_of(wsEndNode), &wsEndOffset);
      NS_ENSURE_SUCCESS(res, res);
      point.mTextNode = do_QueryInterface(wsStartNode);
      point.mOffset = wsStartOffset;
      res = ConvertToNBSP(point);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return res;
}

nsresult 
nsWSRunObject::DeleteChars(nsIDOMNode *aStartNode, PRInt32 aStartOffset, 
                           nsIDOMNode *aEndNode, PRInt32 aEndOffset)
{
  // MOOSE: this routine needs to be modified to preserve the integrity of the
  // wsFragment info.
  if (!aStartNode || !aEndNode)
    return NS_ERROR_NULL_POINTER;

  if ((aStartNode == aEndNode) && (aStartOffset == aEndOffset))
    return NS_OK;  // nothing to delete
  
  nsresult res = NS_OK;
  nsCOMPtr<nsISupports> isupps(do_QueryInterface(aStartNode));
  PRInt32 idx = mNodeArray->IndexOf(isupps);
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

  PRUint32 count;
  mNodeArray->Count(&count);
  while (idx < count)
  {
    isupps = dont_AddRef(mNodeArray->ElementAt(idx));
    node = do_QueryInterface(isupps);
    if (!node)
      break;  // we ran out of ws nodes; must have been deleting to end
    if (node == aStartNode)
    {
      textnode = do_QueryInterface(isupps);
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
        textnode = do_QueryInterface(isupps);
        res = mHTMLEditor->DeleteText(textnode, 0, (PRUint32)aEndOffset);
        NS_ENSURE_SUCCESS(res, res);
      }
      break;
    }
    else
    {
      if (!range)
      {
        range = do_CreateInstance(kCRangeCID);
        res = range->SetStart(aStartNode, aStartOffset);
        NS_ENSURE_SUCCESS(res, res);
        res = range->SetEnd(aEndNode, aEndOffset);
        NS_ENSURE_SUCCESS(res, res);
      }
      PRBool nodeBefore, nodeAfter;
      nsCOMPtr<nsIContent> content (do_QueryInterface(node));
      res = mHTMLEditor->mRangeHelper->CompareNodeToRange(content, range, &nodeBefore, &nodeAfter);
      NS_ENSURE_SUCCESS(res, res);
      if (nodeAfter)
      {
        break;
      }
      if (!nodeBefore)
      {
        res = mHTMLEditor->DeleteNode(node);
        NS_ENSURE_SUCCESS(res, res);
        mNodeArray->RemoveElement(isupps);
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

  nsCOMPtr<nsISupports> isupps(do_QueryInterface(aNode));
  PRInt32 idx = mNodeArray->IndexOf(isupps);
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

  nsCOMPtr<nsISupports> isupps(do_QueryInterface(aNode));
  PRInt32 idx = mNodeArray->IndexOf(isupps);
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
  
  nsCOMPtr<nsISupports> isupps(do_QueryInterface(aPoint.mTextNode));
  PRInt32 idx = mNodeArray->IndexOf(isupps);
  if (idx == -1) return NS_ERROR_FAILURE;
  PRUint32 numNodes;
  mNodeArray->Count(&numNodes);
  
  PRInt32 len;
  nsresult res = aPoint.mTextNode->GetTextLength(&len);
  NS_ENSURE_SUCCESS(res, res);
  
  if (aPoint.mOffset < len)
  {
    *outPoint = aPoint;
    outPoint->mOffset;
    outPoint->mChar = GetCharAt(aPoint.mTextNode, aPoint.mOffset);
  }
  else if (idx < (PRInt32)(numNodes-1))
  {
    nsCOMPtr<nsISupports> isupps = dont_AddRef(mNodeArray->ElementAt(idx+1));
    if (!isupps) return NS_ERROR_FAILURE;
    outPoint->mTextNode = do_QueryInterface(isupps);
    outPoint->mOffset = 0;
    outPoint->mChar = GetCharAt(outPoint->mTextNode, 0);
  }
  else return NS_ERROR_FAILURE;
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
  nsCOMPtr<nsISupports> isupps(do_QueryInterface(aPoint.mTextNode));
  PRInt32 idx = mNodeArray->IndexOf(isupps);
  if (idx == -1) return NS_ERROR_FAILURE;
  
  if (aPoint.mOffset != 0)
  {
    *outPoint = aPoint;
    outPoint->mOffset--;
    outPoint->mChar = GetCharAt(aPoint.mTextNode, aPoint.mOffset-1);
  }
  else if (idx)
  {
    nsCOMPtr<nsISupports> isupps = dont_AddRef(mNodeArray->ElementAt(idx-1));
    if (!isupps) return NS_ERROR_FAILURE;
    outPoint->mTextNode = do_QueryInterface(isupps);
    PRInt32 len;
    res = outPoint->mTextNode->GetTextLength(&len);
    NS_ENSURE_SUCCESS(res, res);
    if (len)
    {
      outPoint->mOffset = len-1;
      outPoint->mChar = GetCharAt(outPoint->mTextNode, len-1);
    }
  }
  else return NS_ERROR_FAILURE;
  return res;
}

nsresult 
nsWSRunObject::ConvertToNBSP(WSPoint aPoint)
{
  // MOOSE: this routine needs to be modified to preserve the integrity of the
  // wsFragment info.
  if (!aPoint.mTextNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMCharacterData> textNode(do_QueryInterface(aPoint.mTextNode));
  if (!textNode)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(textNode));
  
  // first, insert an nbsp
  nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
  nsAutoString nbspStr(nbsp);
  nsresult res = mHTMLEditor->InsertTextIntoTextNodeImpl(nbspStr, textNode, aPoint.mOffset);
  NS_ENSURE_SUCCESS(res, res);
  
  // next, find range of ws it will replace
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  
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
  PRInt32 startOffset, endOffset;
  
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
        if (NS_FAILED(res)) break;
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
        if (NS_FAILED(res)) break;
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
    PRInt16 comp = mHTMLEditor->mRangeHelper->ComparePoints(aNode, aOffset, run->mStartNode, run->mStartOffset);
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
    comp = mHTMLEditor->mRangeHelper->ComparePoints(aNode, aOffset, run->mEndNode, run->mEndOffset);
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
nsWSRunObject::GetCharAt(nsITextContent *aTextNode, PRInt32 aOffset)
{
  // return 0 if we can't get a char, for whatever reason
  if (!aTextNode)
    return 0;
    
  const nsTextFragment *textFrag;
  nsresult res = aTextNode->GetText(&textFrag);
  NS_ENSURE_SUCCESS(res, 0);
  
  PRInt32 len;
  res = aTextNode->GetTextLength(&len);
  NS_ENSURE_SUCCESS(res, 0);
  if (!len) 
    return 0;
  if (aOffset>=len) 
    return 0;
    
  return textFrag->CharAt(aOffset);
}

nsresult 
nsWSRunObject::GetWSPointAfter(nsIDOMNode *aNode, PRInt32 aOffset, WSPoint *outPoint)
{
  // Note: only to be called if aNode is not a ws node.  
  
  // binary search on wsnodes
  PRUint32 numNodes, curNum, lastNum;
  mNodeArray->Count(&numNodes);
  
  if (!numNodes) 
    return NS_OK; // do nothing if there are no nodes to search
  
  curNum = numNodes/2;
  lastNum = numNodes;
  PRInt16 cmp=0;
  nsCOMPtr<nsISupports> isupps;
  nsCOMPtr<nsIDOMNode>  curNode;
  
  // begin binary search
  // we do this because we need to minimize calls to ComparePoints(),
  // which is mongo expensive
  while (curNum != lastNum)
  {
    PRUint32 savedCur = curNum;
    isupps = dont_AddRef(mNodeArray->ElementAt(curNum));
    curNode = do_QueryInterface(isupps);
    cmp = mHTMLEditor->mRangeHelper->ComparePoints(aNode, aOffset, curNode, 0);
    if (cmp < 0)
    {
      if (lastNum > curNum)
        curNum = curNum/2;
      else
        curNum = (curNum+lastNum)/2;
    }
    else
    {
      if (lastNum > curNum)
        curNum = (curNum+lastNum)/2;
      else
        curNum = (curNum+numNodes)/2;
    }
    lastNum = savedCur;
  }
  
  nsCOMPtr<nsITextContent> textNode(do_QueryInterface(curNode));
  
  if (cmp < 0)
  {
    WSPoint point(textNode,0,0);
    return GetCharAfter(point, outPoint);
  }
  else
  {
    PRInt32 len;
    textNode->GetTextLength(&len);
    WSPoint point(textNode,len,0);
    return GetCharAfter(point, outPoint);
  }
  
  return NS_ERROR_FAILURE;
}

nsresult 
nsWSRunObject::GetWSPointBefore(nsIDOMNode *aNode, PRInt32 aOffset, WSPoint *outPoint)
{
  // Note: only to be called if aNode is not a ws node.  
  
  // binary search on wsnodes
  PRUint32 numNodes, curNum, lastNum;
  mNodeArray->Count(&numNodes);
  
  if (!numNodes) 
    return NS_OK; // do nothing if there are no nodes to search
  
  curNum = numNodes/2;
  lastNum = numNodes;
  PRInt16 cmp=0;
  nsCOMPtr<nsISupports> isupps;
  nsCOMPtr<nsIDOMNode>  curNode;
  
  // begin binary search
  // we do this because we need to minimize calls to ComparePoints(),
  // which is mongo expensive
  while (curNum != lastNum)
  {
    PRUint32 savedCur = curNum;
    isupps = dont_AddRef(mNodeArray->ElementAt(curNum));
    curNode = do_QueryInterface(isupps);
    cmp = mHTMLEditor->mRangeHelper->ComparePoints(aNode, aOffset, curNode, 0);
    if (cmp < 0)
    {
      if (lastNum > curNum)
        curNum = curNum/2;
      else
        curNum = (curNum+lastNum)/2;
    }
    else
    {
      if (lastNum > curNum)
        curNum = (curNum+lastNum)/2;
      else
        curNum = (curNum+numNodes)/2;
    }
    lastNum = savedCur;
  }
  
  nsCOMPtr<nsITextContent> textNode(do_QueryInterface(curNode));
  
  if (cmp > 0)
  {
    PRInt32 len;
    textNode->GetTextLength(&len);
    WSPoint point(textNode,len,0);
    return GetCharBefore(point, outPoint);
  }
  else
  {
    WSPoint point(textNode,0,0);
    return GetCharBefore(point, outPoint);
  }
  
  return NS_ERROR_FAILURE;
}

nsresult
nsWSRunObject::CheckTrailingNBSPOfRun(WSFragment *aRun)
{    
  // try to change an nbsp to a space, if possible, just to prevent nbsp proliferation. 
  // examine what is before and after the trailing nbsp, if any.
  if (!aRun) return NS_ERROR_NULL_POINTER;
  WSPoint thePoint;
  PRBool leftCheck = PR_FALSE;
  PRBool rightCheck = PR_FALSE;
  
  // confirm run is normalWS
  if (aRun->mType != eNormalWS) return NS_ERROR_FAILURE;
  
  // first check for trailing nbsp
  nsresult res = GetCharBefore(aRun->mEndNode, aRun->mEndOffset, &thePoint);
  if (NS_SUCCEEDED(res) && thePoint.mChar == nbsp)
  {
    // now check that what is to the left of it is compatible with replacing nbsp with space
    WSPoint prevPoint;
    res = GetCharBefore(thePoint, &prevPoint);
    if (NS_SUCCEEDED(res))
    {
      if (!nsCRT::IsAsciiSpace(prevPoint.mChar)) leftCheck = PR_TRUE;
    }
    else if (aRun->mLeftType == eText)    leftCheck = PR_TRUE;
    else if (aRun->mLeftType == eSpecial) leftCheck = PR_TRUE;
    if (leftCheck)
    {
      // now check that what is to the right of it is compatible with replacing nbsp with space
      if (aRun->mRightType == eText)    rightCheck = PR_TRUE;
      if (aRun->mRightType == eSpecial) rightCheck = PR_TRUE;
      if (aRun->mRightType == eBreak)   rightCheck = PR_TRUE;
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
      res = mHTMLEditor->InsertTextIntoTextNodeImpl(spaceStr, textNode, thePoint.mOffset);
      NS_ENSURE_SUCCESS(res, res);
  
      // finally, delete that nbsp
      nsCOMPtr<nsIDOMNode> delNode(do_QueryInterface(thePoint.mTextNode));
      res = DeleteChars(delNode, thePoint.mOffset+1, delNode, thePoint.mOffset+2);
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
  if (NS_SUCCEEDED(res) && thePoint.mChar == nbsp)
  {
    WSPoint prevPoint;
    res = GetCharBefore(thePoint, &prevPoint);
    if (NS_SUCCEEDED(res))
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
    res = mHTMLEditor->InsertTextIntoTextNodeImpl(spaceStr, textNode, thePoint.mOffset);
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
    if (NS_SUCCEEDED(res))
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
    res = mHTMLEditor->InsertTextIntoTextNodeImpl(spaceStr, textNode, thePoint.mOffset);
    NS_ENSURE_SUCCESS(res, res);
  
    // finally, delete that nbsp
    nsCOMPtr<nsIDOMNode> delNode(do_QueryInterface(thePoint.mTextNode));
    res = DeleteChars(delNode, thePoint.mOffset+1, delNode, thePoint.mOffset+2);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}
