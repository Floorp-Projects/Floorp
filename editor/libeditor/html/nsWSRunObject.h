/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __wsrunobject_h__
#define __wsrunobject_h__

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIEditor.h"
#include "nsINode.h"
#include "nscore.h"

class nsHTMLEditor;
class nsIDOMDocument;
class nsIDOMNode;
struct DOMPoint;

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

/**
 * A type-safe bitfield indicating various types of whitespace or other things.
 * Used as a member variable in nsWSRunObject and WSFragment.
 *
 * XXX: If this idea is useful in other places, we should generalize it using a
 * template.
 */
class WSType {
public:
  enum Enum {
    none       = 0,
    leadingWS  = 1,      // leading insignificant ws, ie, after block or br
    trailingWS = 1 << 1, // trailing insignificant ws, ie, before block
    normalWS   = 1 << 2, // normal significant ws, ie, after text, image, ...
    text       = 1 << 3, // indicates regular (non-ws) text
    special    = 1 << 4, // indicates an inline non-container, like image
    br         = 1 << 5, // indicates a br node
    otherBlock = 1 << 6, // indicates a block other than one ws run is in
    thisBlock  = 1 << 7, // indicates the block ws run is in
    block      = otherBlock | thisBlock // block found
  };

  /**
   * Implicit constructor, because the enums are logically just WSTypes
   * themselves, and are only a separate type because there's no other obvious
   * way to name specific WSType values.
   */
  WSType(const Enum& aEnum = none) : mEnum(aEnum) {}
  // operator==, &, and | need to access mEnum
  friend bool operator==(const WSType& aLeft, const WSType& aRight);
  friend const WSType operator&(const WSType& aLeft, const WSType& aRight);
  friend const WSType operator|(const WSType& aLeft, const WSType& aRight);
  WSType& operator=(const WSType& aOther) {
    // This handles self-assignment fine
    mEnum = aOther.mEnum;
    return *this;
  }
  WSType& operator&=(const WSType& aOther) {
    mEnum &= aOther.mEnum;
    return *this;
  }
  WSType& operator|=(const WSType& aOther) {
    mEnum |= aOther.mEnum;
    return *this;
  }
private:
  uint16_t mEnum;
  void bool_conversion_helper() {}
public:
  // Allow boolean conversion with no numeric conversion
  typedef void (WSType::*bool_type)();
  operator bool_type() const
  {
    return mEnum ? &WSType::bool_conversion_helper : nullptr;
  }
};

/**
 * These are declared as global functions so "WSType::Enum == WSType" et al.
 * will work using the implicit constructor.
 */
inline bool operator==(const WSType& aLeft, const WSType& aRight)
{
  return aLeft.mEnum == aRight.mEnum;
}
inline bool operator!=(const WSType& aLeft, const WSType& aRight)
{
  return !(aLeft == aRight);
}
inline const WSType operator&(const WSType& aLeft, const WSType& aRight)
{
  WSType ret;
  ret.mEnum = aLeft.mEnum & aRight.mEnum;
  return ret;
}
inline const WSType operator|(const WSType& aLeft, const WSType& aRight)
{
  WSType ret;
  ret.mEnum = aLeft.mEnum | aRight.mEnum;
  return ret;
}

/**
 * Make sure that & and | of WSType::Enum creates a WSType instead of an int,
 * because operators between WSType and int shouldn't work
 */
inline const WSType operator&(const WSType::Enum& aLeft,
                              const WSType::Enum& aRight)
{
  return WSType(aLeft) & WSType(aRight);
}
inline const WSType operator|(const WSType::Enum& aLeft,
                              const WSType::Enum& aRight)
{
  return WSType(aLeft) | WSType(aRight);
}


class NS_STACK_CLASS nsWSRunObject
{
  public:

    // public enums ---------------------------------------------------------
    enum BlockBoundary
    {
      kBeforeBlock,
      kBlockStart,
      kBlockEnd,
      kAfterBlock
    };

    enum {eBefore = 1};
    enum {eAfter  = 1 << 1};
    enum {eBoth   = eBefore | eAfter};

    // constructor / destructor -----------------------------------------------
    nsWSRunObject(nsHTMLEditor *aEd, nsIDOMNode *aNode, int32_t aOffset);
    ~nsWSRunObject();
    
    // public methods ---------------------------------------------------------

    // ScrubBlockBoundary removes any non-visible whitespace at the specified
    // location relative to a block node.  
    static nsresult ScrubBlockBoundary(nsHTMLEditor *aHTMLEd, 
                                       nsCOMPtr<nsIDOMNode> *aBlock,
                                       BlockBoundary aBoundary,
                                       int32_t *aOffset = 0);
    
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
                                         int32_t *aStartOffset, 
                                         nsCOMPtr<nsIDOMNode> *aEndNode,
                                         int32_t *aEndOffset);

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
                                               int32_t *aSplitOffset);

    // InsertBreak inserts a br node at {aInOutParent,aInOutOffset}
    // and makes any needed adjustments to ws around that point.
    // example of fixup: normalws after {aInOutParent,aInOutOffset}
    //                   needs to begin with nbsp.
    nsresult InsertBreak(nsCOMPtr<nsIDOMNode> *aInOutParent, 
                         int32_t *aInOutOffset, 
                         nsCOMPtr<nsIDOMNode> *outBRNode, 
                         nsIEditor::EDirection aSelect);

    // InsertText inserts a string at {aInOutParent,aInOutOffset}
    // and makes any needed adjustments to ws around that point.
    // example of fixup: trailingws before {aInOutParent,aInOutOffset}
    //                   needs to be removed.
    nsresult InsertText(const nsAString& aStringToInsert, 
                        nsCOMPtr<nsIDOMNode> *aInOutNode, 
                        int32_t *aInOutOffset,
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
    void PriorVisibleNode(nsIDOMNode *aNode,
                          int32_t aOffset,
                          nsCOMPtr<nsIDOMNode> *outVisNode,
                          int32_t *outVisOffset,
                          WSType *outType);

    // NextVisibleNode returns the first piece of visible thing
    // after {aNode,aOffset}.  If there is no visible ws qualifying
    // it returns what is after the ws run.  Note that 
    // {outVisNode,outVisOffset} is set to just BEFORE the visible
    // object.
    void NextVisibleNode(nsIDOMNode *aNode,
                         int32_t aOffset,
                         nsCOMPtr<nsIDOMNode> *outVisNode,
                         int32_t *outVisOffset,
                         WSType *outType);
    
    // AdjustWhitespace examines the ws object for nbsp's that can
    // be safely converted to regular ascii space and converts them.
    nsresult AdjustWhitespace();

  protected:
    
    // WSFragment struct ---------------------------------------------------------
    // WSFragment represents a single run of ws (all leadingws, or all normalws,
    // or all trailingws, or all leading+trailingws).  Note that this single run may
    // still span multiple nodes.
    struct WSFragment
    {
      nsCOMPtr<nsIDOMNode> mStartNode;  // node where ws run starts
      nsCOMPtr<nsIDOMNode> mEndNode;    // node where ws run ends
      int32_t mStartOffset;             // offset where ws run starts
      int32_t mEndOffset;               // offset where ws run ends
      // type of ws, and what is to left and right of it
      WSType mType, mLeftType, mRightType;
      // other ws runs to left or right.  may be null.
      WSFragment *mLeft, *mRight;

      WSFragment() : mStartNode(0), mEndNode(0),
                     mStartOffset(0), mEndOffset(0),
                     mType(), mLeftType(), mRightType(),
                     mLeft(0), mRight(0)
      {
      }
    };
    
    // WSPoint struct ------------------------------------------------------------
    // A WSPoint struct represents a unique location within the ws run.  It is 
    // always within a textnode that is one of the nodes stored in the list
    // in the wsRunObject.  For convenience, the character at that point is also 
    // stored in the struct.
    struct NS_STACK_CLASS WSPoint
    {
      nsCOMPtr<nsIContent> mTextNode;
      uint32_t mOffset;
      PRUnichar mChar;

      WSPoint() : mTextNode(0),mOffset(0),mChar(0) {}
      WSPoint(nsIDOMNode *aNode, int32_t aOffset, PRUnichar aChar) : 
                     mTextNode(do_QueryInterface(aNode)),mOffset(aOffset),mChar(aChar)
      {
        if (!mTextNode->IsNodeOfType(nsINode::eDATA_NODE)) {
          // Not sure if this is needed, but it'll maintain the same
          // functionality
          mTextNode = nullptr;
        }
      }
      WSPoint(nsIContent *aTextNode, int32_t aOffset, PRUnichar aChar) : 
                     mTextNode(aTextNode),mOffset(aOffset),mChar(aChar) {}
    };    

    enum AreaRestriction
    {
      eAnywhere, eOutsideUserSelectAll
    };    
    
    // protected methods ---------------------------------------------------------
    // tons of utility methods.  

    /**
     * Return the node which we will handle white-space under. This is the
     * closest block within the DOM subtree we're editing, or if none is
     * found, the (inline) root of the editable subtree.
     */
    already_AddRefed<nsIDOMNode> GetWSBoundingParent();

    nsresult GetWSNodes();
    void     GetRuns();
    void     ClearRuns();
    void     MakeSingleWSRun(WSType aType);
    nsresult PrependNodeToList(nsIDOMNode *aNode);
    nsresult AppendNodeToList(nsIDOMNode *aNode);
    nsresult GetPreviousWSNode(nsIDOMNode *aStartNode, 
                               nsIDOMNode *aBlockParent, 
                               nsCOMPtr<nsIDOMNode> *aPriorNode);
    nsresult GetPreviousWSNode(nsIDOMNode *aStartNode,
                               int32_t      aOffset,
                               nsIDOMNode  *aBlockParent, 
                               nsCOMPtr<nsIDOMNode> *aPriorNode);
    nsresult GetPreviousWSNode(DOMPoint aPoint,
                               nsIDOMNode  *aBlockParent, 
                               nsCOMPtr<nsIDOMNode> *aPriorNode);
    nsresult GetNextWSNode(nsIDOMNode *aStartNode, 
                           nsIDOMNode *aBlockParent, 
                           nsCOMPtr<nsIDOMNode> *aNextNode);
    nsresult GetNextWSNode(nsIDOMNode *aStartNode,
                           int32_t     aOffset,
                           nsIDOMNode *aBlockParent, 
                           nsCOMPtr<nsIDOMNode> *aNextNode);
    nsresult GetNextWSNode(DOMPoint aPoint,
                           nsIDOMNode  *aBlockParent, 
                           nsCOMPtr<nsIDOMNode> *aNextNode);
    nsresult PrepareToDeleteRangePriv(nsWSRunObject* aEndObject);
    nsresult PrepareToSplitAcrossBlocksPriv();
    nsresult DeleteChars(nsIDOMNode *aStartNode, int32_t aStartOffset, 
                         nsIDOMNode *aEndNode, int32_t aEndOffset,
                         AreaRestriction aAR = eAnywhere);
    WSPoint  GetCharAfter(nsIDOMNode *aNode, int32_t aOffset);
    WSPoint  GetCharBefore(nsIDOMNode *aNode, int32_t aOffset);
    WSPoint  GetCharAfter(const WSPoint &aPoint);
    WSPoint  GetCharBefore(const WSPoint &aPoint);
    nsresult ConvertToNBSP(WSPoint aPoint,
                           AreaRestriction aAR = eAnywhere);
    void     GetAsciiWSBounds(int16_t aDir, nsIDOMNode *aNode, int32_t aOffset,
                                nsCOMPtr<nsIDOMNode> *outStartNode, int32_t *outStartOffset,
                                nsCOMPtr<nsIDOMNode> *outEndNode, int32_t *outEndOffset);
    void     FindRun(nsIDOMNode *aNode, int32_t aOffset, WSFragment **outRun, bool after);
    PRUnichar GetCharAt(nsIContent *aTextNode, int32_t aOffset);
    WSPoint  GetWSPointAfter(nsIDOMNode *aNode, int32_t aOffset);
    WSPoint  GetWSPointBefore(nsIDOMNode *aNode, int32_t aOffset);
    nsresult CheckTrailingNBSPOfRun(WSFragment *aRun);
    nsresult CheckTrailingNBSP(WSFragment *aRun, nsIDOMNode *aNode, int32_t aOffset);
    nsresult CheckLeadingNBSP(WSFragment *aRun, nsIDOMNode *aNode, int32_t aOffset);
    
    static nsresult ScrubBlockBoundaryInner(nsHTMLEditor *aHTMLEd, 
                                       nsCOMPtr<nsIDOMNode> *aBlock,
                                       BlockBoundary aBoundary);
    nsresult Scrub();
    
    // member variables ---------------------------------------------------------
    
    nsCOMPtr<nsIDOMNode> mNode;           // the node passed to our constructor
    int32_t mOffset;                      // the offset passed to our contructor
    // together, the above represent the point at which we are building up ws info.
    
    bool    mPRE;                         // true if we are in preformatted whitespace context
    nsCOMPtr<nsIDOMNode> mStartNode;      // node/offset where ws starts
    int32_t mStartOffset;                 // ...
    WSType mStartReason;                  // reason why ws starts (eText, eOtherBlock, etc)
    nsCOMPtr<nsIDOMNode> mStartReasonNode;// the node that implicated by start reason
    
    nsCOMPtr<nsIDOMNode> mEndNode;        // node/offset where ws ends
    int32_t mEndOffset;                   // ...
    WSType mEndReason;                    // reason why ws ends (eText, eOtherBlock, etc)
    nsCOMPtr<nsIDOMNode> mEndReasonNode;  // the node that implicated by end reason
    
    nsCOMPtr<nsIDOMNode> mFirstNBSPNode;  // location of first nbsp in ws run, if any
    int32_t mFirstNBSPOffset;             // ...
    
    nsCOMPtr<nsIDOMNode> mLastNBSPNode;   // location of last nbsp in ws run, if any
    int32_t mLastNBSPOffset;              // ...
    
    nsCOMArray<nsIDOMNode> mNodeArray;//the list of nodes containing ws in this run
    
    WSFragment *mStartRun;                // the first WSFragment in the run
    WSFragment *mEndRun;                  // the last WSFragment in the run, may be same as first
    
    nsHTMLEditor *mHTMLEditor;            // non-owning.
    
    friend class nsHTMLEditRules;  // opening this class up for pillaging
    friend class nsHTMLEditor;     // opening this class up for more pillaging
};

#endif

