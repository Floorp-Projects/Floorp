/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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

// class nsWSRunObject represents the entire whitespace situation
// around a given point.  It collects up a list of nodes that contain
// whitespace and categorizes in up to 3 different WSFragments (detailed
// below).  Each WSFragment is a collection of whitespace that is
// either all insignificant, or that is significant.  A WSFragment could
// consist of insignificant whitespace because it is after a block
// boundary or after a break.  Or it could be insignificant because it
// is before a block.  Or it could be significant because it is
// surrounded by text, or starts and ends with nbsps, etc.

// Throughout I refer to LeadingWS, NormalWS, TrailingWS.  LeadingWS & TrailingWS
// are runs of ascii ws that are insignificant (do not render) because they
// are adjacent to block boundaries, or after a break.  NormalWS is ws that
// does cause soem rendering.  Note that not all the ws in a NormalWS run need
// render.  For example, two ascii spaces surrounded by text on both sides
// will only render as one space (in non-preformatted stlye html), yet both
// spaces count as NormalWS.  Together, they render as the one visible space.

class nsWSRunObject
{
  public:

    typedef enum
    {
      kForward,
      kBackward
    } EWSDirection;

    // constructor / destructor -----------------------------------------------
    nsWSRunObject(nsHTMLEditor *aEd);
    nsWSRunObject(nsHTMLEditor *aEd, nsIDOMNode *aNode, PRInt32 aOffset);
    ~nsWSRunObject();
    
    // public methods ---------------------------------------------------------

    // PrepareToJoinBlocks fixes up ws at the end of aLeftParent and the
    // beginning of aRightParent in preperation for them to be joined.
    // example of fixup: trailingws in aLeftParent needs to be removed.
    static nsresult PrepareToJoinBlocks(nsHTMLEditor *aEd, 
                                        nsIDOMNode *aLeftParent,
                                        nsIDOMNode *aRightParent);

    // PrepareToDeleteRange fixes up ws before {aStartNode,aStartOffset}
    // and after {aEndNode,aEndOffset} in preperation for content
    // in that range to be deleted.  Note that the nodes and offsets
    // are adjusted in response to any dom changes we make while 
    // adjusting ws.
    // example of fixup: trailingws before {aStartNode,aStartOffset}
    //                   needs to be removed.
    static nsresult PrepareToDeleteRange(nsHTMLEditor *aHTMLEd, 
                                         nsCOMPtr<nsIDOMNode> *aStartNode,
                                         PRInt32 *aStartOffset, 
                                         nsCOMPtr<nsIDOMNode> *aEndNode,
                                         PRInt32 *aEndOffset);

    // PrepareToDeleteNode fixes up ws before and after aNode in preperation 
    // for aNode to be deleted.
    // example of fixup: trailingws before aNode needs to be removed.
    static nsresult PrepareToDeleteNode(nsHTMLEditor *aHTMLEd, 
                                        nsIDOMNode *aNode);

    // PrepareToSplitAcrossBlocks fixes up ws before and after 
    // {aSplitNode,aSplitOffset} in preperation for a block
    // parent to be split.  Note that the aSplitNode and aSplitOffset
    // are adjusted in response to any dom changes we make while 
    // adjusting ws.
    // example of fixup: normalws before {aSplitNode,aSplitOffset} 
    //                   needs to end with nbsp.
    static nsresult PrepareToSplitAcrossBlocks(nsHTMLEditor *aHTMLEd, 
                                               nsCOMPtr<nsIDOMNode> *aSplitNode, 
                                               PRInt32 *aSplitOffset);

    // InsertBreak inserts a br node at {aInOutParent,aInOutOffset}
    // and makes any needed adjustments to ws around that point.
    // example of fixup: normalws after {aInOutParent,aInOutOffset}
    //                   needs to begin with nbsp.
    nsresult InsertBreak(nsCOMPtr<nsIDOMNode> *aInOutParent, 
                         PRInt32 *aInOutOffset, 
                         nsCOMPtr<nsIDOMNode> *outBRNode, 
                         nsIEditor::EDirection aSelect);

    // InsertText inserts a string at {aInOutParent,aInOutOffset}
    // and makes any needed adjustments to ws around that point.
    // example of fixup: trailingws before {aInOutParent,aInOutOffset}
    //                   needs to be removed.
    nsresult InsertText(const nsAReadableString& aStringToInsert, 
                        nsCOMPtr<nsIDOMNode> *aInOutNode, 
                        PRInt32 *aInOutOffset,
                        nsIDOMDocument *aDoc);

    // DeleteWSBackward deletes a single visible piece of ws before
    // the ws point (the point to create the wsRunObject, passed to 
    // its constructor).  It makes any needed conversion to adjacent
    // ws to retain its significance.
    nsresult DeleteWSBackward();

    // DeleteWSForward deletes a single visible piece of ws after
    // the ws point (the point to create the wsRunObject, passed to 
    // its constructor).  It makes any needed conversion to adjacent
    // ws to retain its significance.
    nsresult DeleteWSForward();

    // PriorVisibleNode returns the first piece of visible thing
    // before {aNode,aOffset}.  If there is no visible ws qualifying
    // it returns what is before the ws run.  Note that 
    // {outVisNode,outVisOffset} is set to just AFTER the visible
    // object.
    nsresult PriorVisibleNode(nsIDOMNode *aNode, 
                              PRInt32 aOffset, 
                              nsCOMPtr<nsIDOMNode> *outVisNode, 
                              PRInt32 *outVisOffset,
                              PRInt16 *outType);

    // NextVisibleNode returns the first piece of visible thing
    // after {aNode,aOffset}.  If there is no visible ws qualifying
    // it returns what is after the ws run.  Note that 
    // {outVisNode,outVisOffset} is set to just BEFORE the visible
    // object.
    nsresult NextVisibleNode (nsIDOMNode *aNode, 
                              PRInt32 aOffset, 
                              nsCOMPtr<nsIDOMNode> *outVisNode, 
                              PRInt32 *outVisOffset,
                              PRInt16 *outType);
    
    // AdjustWhitespace examines the ws object for nbsp's that can
    // be safely converted to regular ascii space and converts them.
    nsresult AdjustWhitespace();
    
    // public enums ---------------------------------------------------------
    enum {eNone = 0};
    enum {eLeadingWS  = 1};          // leading insignificant ws, ie, after block or br
    enum {eTrailingWS = 1 << 1};     // trailing insignificant ws, ie, before block
    enum {eNormalWS   = 1 << 2};     // normal significant ws, ie, after text, image, ...
    enum {eText       = 1 << 3};     // indicates regular (non-ws) text
    enum {eSpecial    = 1 << 4};     // indicates an inline non-container, like image
    enum {eBreak      = 1 << 5};     // indicates a br node
    enum {eOtherBlock = 1 << 6};     // indicates a block other than one ws run is in
    enum {eThisBlock  = 1 << 7};     // indicates the block ws run is in
    enum {eBlock      = eOtherBlock | eThisBlock};   // block found
    
    enum {eBefore = 1};
    enum {eAfter  = 1 << 1};
    enum {eBoth   = eBefore | eAfter};
    
  protected:
    
    // WSFragment struct ---------------------------------------------------------
    // WSFragment represents a single run of ws (all leadingws, or all normalws,
    // or all trailingws, or all leading+trailingws).  Note that this single run may
    // still span multiple nodes.
    struct WSFragment
    {
      nsCOMPtr<nsIDOMNode> mStartNode;  // node where ws run starts
      nsCOMPtr<nsIDOMNode> mEndNode;    // node where ws run ends
      PRInt16 mStartOffset;             // offset where ws run starts
      PRInt16 mEndOffset;               // offset where ws run ends
      PRInt16 mType, mLeftType, mRightType;  // type of ws, and what is to left and right of it
      WSFragment *mLeft, *mRight;            // other ws runs to left or right.  may be null.

      WSFragment() : mStartNode(0),mEndNode(0),mStartOffset(0),
                     mEndOffset(0),mType(0),mLeftType(0),
                     mRightType(0),mLeft(0),mRight(0) {}
    };
    
    // WSPoint struct ------------------------------------------------------------
    // A WSPoint struct represents a unique location within the ws run.  It is 
    // always within a textnode that is one of the nodes stored in the list
    // in the wsRunObject.  For convenience, the character at that point is also 
    // stored in the struct.
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
    
    // protected methods ---------------------------------------------------------
    // tons of utility methods.  
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
    nsresult PrepareToSplitAcrossBlocksPriv();
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
    nsresult CheckTrailingNBSPOfRun(WSFragment *aRun);
    nsresult CheckTrailingNBSP(WSFragment *aRun, nsIDOMNode *aNode, PRInt32 aOffset);
    nsresult CheckLeadingNBSP(WSFragment *aRun, nsIDOMNode *aNode, PRInt32 aOffset);
    
    // member variables ---------------------------------------------------------
    
    nsCOMPtr<nsIDOMNode> mNode;           // the node passed to our constructor
    PRInt32 mOffset;                      // the offset passed to our contructor
    // together, the above represent the point at which we are building up ws info.
    
    nsCOMPtr<nsIDOMNode> mStartNode;      // node/offet where ws starts
    PRInt32 mStartOffset;                 // ...
    PRInt16 mStartReason;                 // reason why ws starts (eText, eOtherBlock, etc)
    
    nsCOMPtr<nsIDOMNode> mEndNode;        // node/offet where ws ends
    PRInt32 mEndOffset;                   // ...
    PRInt16 mEndReason;                   // reason why ws ends (eText, eOtherBlock, etc)
    
    nsCOMPtr<nsIDOMNode> mFirstNBSPNode;  // location of first nbsp in ws run, if any
    PRInt32 mFirstNBSPOffset;             // ...
    
    nsCOMPtr<nsIDOMNode> mLastNBSPNode;   // location of last nbsp in ws run, if any
    PRInt32 mLastNBSPOffset;              // ...
    
    nsCOMPtr<nsISupportsArray> mNodeArray;//the list of nodes containing ws in this run
    
    WSFragment *mStartRun;                // the first WSFragment in the run
    WSFragment *mEndRun;                  // the last WSFragment in the run, may be same as first
    
    nsHTMLEditor *mHTMLEditor;            // non-owning.
};

#endif

