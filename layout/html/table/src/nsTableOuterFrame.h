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

struct OuterTableReflowState;
struct nsStyleText;

/**
 * main frame for an nsTable content object, 
 * the nsTableOuterFrame contains 0 or one caption frame, and a nsTableFrame
 * psuedo-frame (referred to as the "inner frame').
 * <P> Unlike other frames that handle continuing across breaks, nsTableOuterFrame
 * has no notion of "unmapped" children.  All children (caption and inner table)
 * have frames created in Pass 1, so from the layout process' point of view, they
 * are always mapped
 *
 */
class nsTableOuterFrame : public nsHTMLContainerFrame
{
public:

  /** instantiate a new instance of nsTableOuterFrame.
    * @param aResult    the new object is returned in this out-param
    * @param aContent   the table object to map
    * @param aParent    the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableOuterFrame(nsIFrame*& aResult);

  NS_IMETHOD  SetInitialChildList(nsIPresContext& aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList);

  /** @see nsIFrame::Paint */
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  /** @see nsContainerFrame */
  NS_IMETHOD CreateContinuingFrame(nsIPresContext&  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  /** return the min width of the caption.  Return 0 if there is no caption. */
  nscoord GetMinCaptionWidth();

  NS_IMETHOD  List(FILE* out = stdout, PRInt32 aIndent = 0, nsIListFilter *aFilter = nsnull) const;

protected:

  /** protected constructor 
    * @see NewFrame
    */
  nsTableOuterFrame();

  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  /** return PR_TRUE if the table needs to be reflowed.  
    * the outer table needs to be reflowed if the table content has changed,
    * or if the table style attributes or parent max height/width have
    * changed.
    */
  PRBool NeedsReflow(const nsHTMLReflowState& aReflowState, const nsSize& aMaxSize);

  void PlaceChild(OuterTableReflowState& aReflowState,
                  nsIFrame*              aKidFrame,
                  const nsRect&          aKidRect,
                  nsSize*                aMaxElementSize,
                  nsSize&                aKidMaxElementSize);

  nscoord GetTableWidth(const nsHTMLReflowState& aReflowState);

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
  PRBool DeleteChildsNextInFlow(nsIPresContext& aPresContext, nsIFrame* aChild);

// begin Incremental Reflow methods
  NS_IMETHOD RecoverState(OuterTableReflowState& aReflowState, nsIFrame* aKidFrame);

  NS_IMETHOD IncrementalReflow(nsIPresContext&        aPresContext,
                               nsHTMLReflowMetrics&   aDesiredSize,
                               OuterTableReflowState& aReflowState,
                               nsReflowStatus&        aStatus);

  NS_IMETHOD IR_TargetIsChild(nsIPresContext&        aPresContext,
                              nsHTMLReflowMetrics&   aDesiredSize,
                              OuterTableReflowState& aReflowState,
                              nsReflowStatus&        aStatus,
                              nsIFrame *             aNextFrame);

  NS_IMETHOD IR_TargetIsInnerTableFrame(nsIPresContext&        aPresContext,
                                        nsHTMLReflowMetrics&   aDesiredSize,
                                        OuterTableReflowState& aReflowState,
                                        nsReflowStatus&        aStatus);

  NS_IMETHOD IR_TargetIsCaptionFrame(nsIPresContext&        aPresContext,
                                     nsHTMLReflowMetrics&   aDesiredSize,
                                     OuterTableReflowState& aReflowState,
                                     nsReflowStatus&        aStatus);

  NS_IMETHOD IR_TargetIsMe(nsIPresContext&        aPresContext,
                           nsHTMLReflowMetrics&   aDesiredSize,
                           OuterTableReflowState& aReflowState,
                           nsReflowStatus&        aStatus);

  NS_IMETHOD IR_InnerTableReflow(nsIPresContext&        aPresContext,
                                 nsHTMLReflowMetrics&   aDesiredSize,
                                 OuterTableReflowState& aReflowState,
                                 nsReflowStatus&        aStatus);

  NS_IMETHOD IR_CaptionInserted(nsIPresContext&        aPresContext,
                                nsHTMLReflowMetrics&   aDesiredSize,
                                OuterTableReflowState& aReflowState,
                                nsReflowStatus&        aStatus,
                                nsIFrame *             aCaptionFrame,
                                PRBool                 aReplace);

  NS_IMETHOD IR_CaptionRemoved(nsIPresContext&        aPresContext,
                               nsHTMLReflowMetrics&   aDesiredSize,
                               OuterTableReflowState& aReflowState,
                               nsReflowStatus&        aStatus);

  PRBool IR_CaptionChangedAxis(const nsStyleText* aOldStyle, 
                               const nsStyleText* aNewStyle) const;

  NS_IMETHOD SizeAndPlaceChildren(const nsSize &         aInnerSize, 
                                  const nsSize &         aCaptionSize,
                                  OuterTableReflowState& aReflowState);

  NS_IMETHOD AdjustSiblingsAfterReflow(nsIPresContext&        aPresContext,
                                       OuterTableReflowState& aReflowState,
                                       nsIFrame*              aKidFrame,
                                       nscoord                aDeltaY);

// end Incremental Reflow methods

private:
  /** used to keep track of this frame's children */
  nsIFrame *mInnerTableFrame;
  nsIFrame *mCaptionFrame;

  /** used to track caption max element size */
  PRInt32 mMinCaptionWidth;

  /** used to cache reflow results so we can optimize out reflow in some circumstances */
  nsHTMLReflowMetrics mDesiredSize;
  nsSize mMaxElementSize;

};

inline nscoord nsTableOuterFrame::GetMinCaptionWidth()
{ return mMinCaptionWidth; }

inline PRIntn nsTableOuterFrame::GetSkipSides() const
{ return 0; }

#endif



