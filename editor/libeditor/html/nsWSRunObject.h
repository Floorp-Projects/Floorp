/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __wsrunobject_h__
#define __wsrunobject_h__

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsIDOMNode.h"
#include "nsISupportsArray.h"
#include "nsITextContent.h"
#include "nsIEditor.h"

class nsIDOMDocument;
class nsIDOMNode;
class nsHTMLEditor;

class nsWSRunObject
{
  public:

    typedef enum
    {
      kForward,
      kBackward
    } EWSDirection;

    // constructor / destructor
    nsWSRunObject(nsHTMLEditor *aEd);
    nsWSRunObject(nsHTMLEditor *aEd, nsIDOMNode *aNode, PRInt32 aOffset);
    ~nsWSRunObject();
    
    // public methods
    static nsresult PrepareToJoinBlocks(nsHTMLEditor *aEd, 
                                        nsIDOMNode *aLeftParent,
                                        nsIDOMNode *aRightParent);
    static nsresult PrepareToDeleteRange(nsHTMLEditor *aHTMLEd, 
                                         nsCOMPtr<nsIDOMNode> *aStartNode,
                                         PRInt32 *aStartOffset, 
                                         nsCOMPtr<nsIDOMNode> *aEndNode,
                                         PRInt32 *aEndOffset);
    static nsresult PrepareToDeleteNode(nsHTMLEditor *aHTMLEd, 
                                        nsIDOMNode *aNode);
    static nsresult PrepareToSplitAcrossBlocks(nsCOMPtr<nsIDOMNode> *aSplitNode, 
                                               PRInt32 *aSplitOffset);

    nsresult InsertBreak(nsCOMPtr<nsIDOMNode> *aInOutParent, 
                         PRInt32 *aInOutOffset, 
                         nsCOMPtr<nsIDOMNode> *outBRNode, 
                         nsIEditor::EDirection aSelect);
    nsresult InsertText(const nsAReadableString& aStringToInsert, 
                        nsCOMPtr<nsIDOMNode> *aInOutNode, 
                        PRInt32 *aInOutOffset,
                        nsIDOMDocument *aDoc);
    nsresult DeleteWSBackward();
    nsresult DeleteWSForward();
    nsresult PriorVisibleNode(nsIDOMNode *aNode, 
                              PRInt32 aOffset, 
                              nsCOMPtr<nsIDOMNode> *outVisNode, 
                              PRInt32 *outVisOffset,
                              PRInt16 *outType);
    nsresult NextVisibleNode (nsIDOMNode *aNode, 
                              PRInt32 aOffset, 
                              nsCOMPtr<nsIDOMNode> *outVisNode, 
                              PRInt32 *outVisOffset,
                              PRInt16 *outType);
    
    enum {eNone = 0};
    enum {eLeadingWS  = 1};
    enum {eTrailingWS = 1 << 1};
    enum {eNormalWS   = 1 << 2};
    enum {eText       = 1 << 3};
    enum {eSpecial    = 1 << 4};
    enum {eBreak      = 1 << 5};
    enum {eOtherBlock = 1 << 6};
    enum {eThisBlock  = 1 << 7};
    enum {eBlock      = eOtherBlock | eThisBlock};
    
    enum {eBefore = 1};
    enum {eAfter  = 1 << 1};
    enum {eBoth   = eBefore | eAfter};
    
  protected:
  
    struct WSFragment
    {
      nsCOMPtr<nsIDOMNode> mStartNode;
      nsCOMPtr<nsIDOMNode> mEndNode;
      PRInt16 mStartOffset;
      PRInt16 mEndOffset;
      PRInt16 mType, mLeftType, mRightType;
      WSFragment *mLeft, *mRight;

      WSFragment() : mStartNode(0),mEndNode(0),mStartOffset(0),
                     mEndOffset(0),mType(0),mLeftType(0),
                     mRightType(0),mLeft(0),mRight(0) {}
    };
    
    struct WSPoint
    {
      nsCOMPtr<nsITextContent> mTextNode;
      PRInt16 mOffset;
      PRUnichar mChar;
      
      WSPoint() : mTextNode(0),mOffset(0),mChar(0) {}
      WSPoint(nsIDOMNode *aNode, PRInt32 aOffset, PRUnichar aChar) : 
                     mTextNode(do_QueryInterface(aNode)),mOffset(aOffset),mChar(aChar) {}
      WSPoint(nsITextContent *aTextNode, PRInt32 aOffset, PRUnichar aChar) : 
                     mTextNode(aTextNode),mOffset(aOffset),mChar(aChar) {}
    };
    
    // protected methods
    nsresult GetWSNodes();
    nsresult GetRuns();
    void     ClearRuns();
    nsresult MakeSingleWSRun(PRInt16 aType);
    nsresult PrependNodeToList(nsIDOMNode *aNode);
    nsresult AppendNodeToList(nsIDOMNode *aNode);
    nsresult GetPreviousWSNode(nsIDOMNode *aStartNode, 
                               nsIDOMNode *aBlockParent, 
                               nsCOMPtr<nsIDOMNode> *aPriorNode);
    nsresult GetNextWSNode(nsIDOMNode *aStartNode, 
                           nsIDOMNode *aBlockParent, 
                           nsCOMPtr<nsIDOMNode> *aNextNode);
    nsresult GetPreviousWSNode(nsIDOMNode *aStartNode,
                               PRInt16      aOffset, 
                               nsIDOMNode  *aBlockParent, 
                               nsCOMPtr<nsIDOMNode> *aPriorNode);
    nsresult GetNextWSNode(nsIDOMNode *aStartNode,
                           PRInt16     aOffset, 
                           nsIDOMNode *aBlockParent, 
                           nsCOMPtr<nsIDOMNode> *aNextNode);
    nsresult PrepareToDeleteRangePriv(nsWSRunObject* aEndObject);
    nsresult DeleteChars(nsIDOMNode *aStartNode, PRInt32 aStartOffset, 
                         nsIDOMNode *aEndNode, PRInt32 aEndOffset);
    nsresult GetCharAfter(nsIDOMNode *aNode, PRInt32 aOffset, WSPoint *outPoint);
    nsresult GetCharBefore(nsIDOMNode *aNode, PRInt32 aOffset, WSPoint *outPoint);
    nsresult GetCharAfter(WSPoint &aPoint, WSPoint *outPoint);
    nsresult GetCharBefore(WSPoint &aPoint, WSPoint *outPoint);
    nsresult ConvertToNBSP(WSPoint aPoint);
    nsresult GetAsciiWSBounds(PRInt16 aDir, nsIDOMNode *aNode, PRInt32 aOffset,
                                nsCOMPtr<nsIDOMNode> *outStartNode, PRInt32 *outStartOffset,
                                nsCOMPtr<nsIDOMNode> *outEndNode, PRInt32 *outEndOffset);
    nsresult FindRun(nsIDOMNode *aNode, PRInt32 aOffset, WSFragment **outRun, PRBool after);
    PRUnichar GetCharAt(nsITextContent *aTextNode, PRInt32 aOffset);
    nsresult GetWSPointAfter(nsIDOMNode *aNode, PRInt32 aOffset, WSPoint *outPoint);
    nsresult GetWSPointBefore(nsIDOMNode *aNode, PRInt32 aOffset, WSPoint *outPoint);
    nsresult CheckTrailingNBSP(WSFragment *aRun, nsIDOMNode *aNode, PRInt32 aOffset);
    nsresult CheckLeadingNBSP(WSFragment *aRun, nsIDOMNode *aNode, PRInt32 aOffset);
    
    // member variables
    nsCOMPtr<nsIDOMNode> mNode;
    PRInt32 mOffset;
    nsCOMPtr<nsIDOMNode> mStartNode;
    PRInt32 mStartOffset;
    PRInt16 mStartReason;
    nsCOMPtr<nsIDOMNode> mEndNode;
    PRInt32 mEndOffset;
    PRInt16 mEndReason;
    nsCOMPtr<nsIDOMNode> mFirstNBSPNode;
    PRInt32 mFirstNBSPOffset;
    nsCOMPtr<nsIDOMNode> mLastNBSPNode;
    PRInt32 mLastNBSPOffset;
    nsCOMPtr<nsISupportsArray> mNodeArray;
    WSFragment *mStartRun;
    WSFragment *mEndRun;
    nsHTMLEditor *mHTMLEditor;  // non-owning.
};

#endif

