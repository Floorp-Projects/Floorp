/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsTableOuterFrame_h__
#define nsTableOuterFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"
#include "nsITableLayout.h"

struct OuterTableReflowState;
struct nsStyleText;

/* TODO
1. decide if we'll allow subclassing.  If so, decide which methods really need to be virtual.
*/

/**
 * main frame for an nsTable content object, 
 * the nsTableOuterFrame contains 0 or one caption frame, and a nsTableFrame
 * psuedo-frame (referred to as the "inner frame').
 */
class nsTableOuterFrame : public nsHTMLContainerFrame, public nsITableLayout
{
public:

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  /** instantiate a new instance of nsTableOuterFrame.
    * @param aResult    the new object is returned in this out-param
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableOuterFrame(nsIFrame** aResult);

  /**  @see nsIFrame::SetInitialChildList */    
  NS_IMETHOD  SetInitialChildList(nsIPresContext& aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList);

  NS_IMETHOD AppendFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext& aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  /** @see nsIFrame::Paint */
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  /** process a reflow command for the table.
    * This involves reflowing the caption and the inner table.
    * @see nsIFrame::Reflow */
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableOuterFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  /** @see nsIFrame::GetFrameName */
  NS_IMETHOD GetFrameName(nsString& aResult) const;

#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

  /** SetSelected needs to be overridden to talk to inner tableframe
   */
  NS_IMETHOD SetSelected(nsIDOMRange *aRange,PRBool aSelected, nsSpread aSpread);

  /** return the min width of the caption.  Return 0 if there is no caption. 
    * The return value is only meaningful after the caption has had a pass1 reflow.
    */
  nscoord GetMinCaptionWidth();

  /*---------------- nsITableLayout methods ------------------------*/

  /** @see nsITableFrame::GetCellDataAt */
  NS_IMETHOD GetCellDataAt(PRInt32 aRowIndex, PRInt32 aColIndex, 
                           nsIDOMElement* &aCell,   //out params
                           PRInt32& aStartRowIndex, PRInt32& aStartColIndex, 
                           PRInt32& aRowSpan, PRInt32& aColSpan,
                           PRBool& aIsSelected);

  /** @see nsITableFrame::GetTableSize */
  NS_IMETHOD GetTableSize(PRInt32& aRowCount, PRInt32& aColCount);

protected:


  /** Always returns 0, since the outer table frame has no border of its own
    * The inner table frame can answer this question in a meaningful way.
    * @see nsHTMLContainerFrame::GetSkipSides */
  virtual PRIntn GetSkipSides() const;

  /** return PR_TRUE if the table needs to be reflowed.  
    * the outer table needs to be reflowed if the table content has changed,
    * or if the table style attributes or parent max height/width have
    * changed.
    */
  PRBool NeedsReflow(const nsHTMLReflowState& aReflowState);

  /** position the child frame
    * @param  aReflowState      the state of the reflow process
    * @param  aKidFrame         the frame to place. 
    * @param  aKidRect          the computed dimensions of aKidFrame.  The origin of aKidRect 
    *                           is relative to the upper-left origin of this frame.
    * @param  aMaxElementSize   the table's maxElementSize
    *                           may be nsnull, meaning that we're not computing maxElementSize during this reflow
    *                           set to the caption's maxElementSize, if aKidFrame is the caption
    *                           the tables maxElementSize eventually gets set to the max of
    *                           the value here and the value of the inner table, elsewhere during reflow.
    * @param aKidMaxElementSize the maxElementSize of aKidFrame, if available  
    */
  void PlaceChild(OuterTableReflowState& aReflowState,
                  nsIFrame*              aKidFrame,
                  const nsRect&          aKidRect,
                  nsSize*                aMaxElementSize,
                  nsSize&                aKidMaxElementSize);


  /** compute the width available to the table during reflow, based on 
    * the reflow state and the table's style.
    * @return the computed width
    */
  nscoord ComputeAvailableTableWidth(const nsHTMLReflowState& aReflowState);

  /** overridden here to handle special caption-table relationship
    * @see nsContainerFrame::VerifyTree
    */
  NS_IMETHOD VerifyTree() const;

  /**
   * Remove and delete aChild's next-in-flow(s). Updates the sibling and flow
   * pointers.
   *
   * Updates the child count and content offsets of all containers that are
   * affected
   *
   * Overloaded here because nsContainerFrame makes assumptions about pseudo-frames
   * that are not true for tables.
   *
   * @param   aChild child this child's next-in-flow
   * @return  PR_TRUE if successful and PR_FALSE otherwise
   */
  virtual void DeleteChildsNextInFlow(nsIPresContext& aPresContext, nsIFrame* aChild);

// begin Incremental Reflow methods
  /** prepare aReflowState for an incremental reflow */
  NS_IMETHOD RecoverState(OuterTableReflowState& aReflowState, nsIFrame* aKidFrame);

  /** process an incremental reflow command */
  NS_IMETHOD IncrementalReflow(nsIPresContext&        aPresContext,
                               nsHTMLReflowMetrics&   aDesiredSize,
                               OuterTableReflowState& aReflowState,
                               nsReflowStatus&        aStatus);

  /** process an incremental reflow command targeted at a child of this frame. */
  NS_IMETHOD IR_TargetIsChild(nsIPresContext&        aPresContext,
                              nsHTMLReflowMetrics&   aDesiredSize,
                              OuterTableReflowState& aReflowState,
                              nsReflowStatus&        aStatus,
                              nsIFrame *             aNextFrame);

  /** process an incremental reflow command targeted at the table inner frame. */
  NS_IMETHOD IR_TargetIsInnerTableFrame(nsIPresContext&        aPresContext,
                                        nsHTMLReflowMetrics&   aDesiredSize,
                                        OuterTableReflowState& aReflowState,
                                        nsReflowStatus&        aStatus);

  /** process an incremental reflow command targeted at the caption. */
  NS_IMETHOD IR_TargetIsCaptionFrame(nsIPresContext&        aPresContext,
                                     nsHTMLReflowMetrics&   aDesiredSize,
                                     OuterTableReflowState& aReflowState,
                                     nsReflowStatus&        aStatus);

  /** process an incremental reflow command targeted at this frame.
    * many incremental reflows that are targeted at this outer frame
    * are actually handled by the inner frame.  The logic to decide this
    * is here.
    */
  NS_IMETHOD IR_TargetIsMe(nsIPresContext&        aPresContext,
                           nsHTMLReflowMetrics&   aDesiredSize,
                           OuterTableReflowState& aReflowState,
                           nsReflowStatus&        aStatus);

  /** pass along the incremental reflow command to the inner table. */
  NS_IMETHOD IR_InnerTableReflow(nsIPresContext&        aPresContext,
                                 nsHTMLReflowMetrics&   aDesiredSize,
                                 OuterTableReflowState& aReflowState,
                                 nsReflowStatus&        aStatus);

  /** handle incremental reflow notification that a caption was inserted. */
  NS_IMETHOD IR_CaptionInserted(nsIPresContext&        aPresContext,
                                nsHTMLReflowMetrics&   aDesiredSize,
                                OuterTableReflowState& aReflowState,
                                nsReflowStatus&        aStatus);

  /** handle incremental reflow notification that we have dirty child frames */
  NS_IMETHOD IR_ReflowDirty(nsIPresContext&        aPresContext,
                            nsHTMLReflowMetrics&   aDesiredSize,
                            OuterTableReflowState& aReflowState,
                            nsReflowStatus&        aStatus);

  /** handle incremental reflow notification that the caption style was changed
    * such that it is now left|right instead of top|bottom, or vice versa.
    */
  PRBool IR_CaptionChangedAxis(const nsStyleText* aOldStyle, 
                               const nsStyleText* aNewStyle) const;

  /** set the size and the location of both the inner table frame and the caption. */
  NS_IMETHOD SizeAndPlaceChildren(const nsSize &         aInnerSize, 
                                  const nsSize &         aCaptionSize,
                                  OuterTableReflowState& aReflowState);

// end Incremental Reflow methods

private:
  /** used to keep track of this frame's children */
  nsIFrame *mInnerTableFrame;
  nsIFrame *mCaptionFrame;

  /** used to track caption max element size */
  PRInt32 mMinCaptionWidth;

  nsSize mMaxElementSize;

};

inline nscoord nsTableOuterFrame::GetMinCaptionWidth()
{ return mMinCaptionWidth; }

inline PRIntn nsTableOuterFrame::GetSkipSides() const
{ return 0; }

#endif



