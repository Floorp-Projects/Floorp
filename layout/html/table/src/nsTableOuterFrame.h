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
#include "nsContainerFrame.h"

struct OuterTableReflowState;

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
class nsTableOuterFrame : public nsContainerFrame
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
  NS_NewTableOuterFrame(nsIContent* aContent,
                        nsIFrame*   aParentFrame,
                        nsIFrame*&  aResult);

  NS_IMETHOD  Init(nsIPresContext& aPresContext, nsIFrame* aChildList);

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

  /** return the min width of the caption.  Return 0 if there is no caption. */
  nscoord GetMinCaptionWidth();

  NS_IMETHOD  List(FILE* out = stdout, PRInt32 aIndent = 0, nsIListFilter *aFilter = nsnull) const;

protected:

  /** protected constructor 
    * @see NewFrame
    */
  nsTableOuterFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  /** return PR_TRUE if the table needs to be reflowed.  
    * the outer table needs to be reflowed if the table content has changed,
    * or if the table style attributes or parent max height/width have
    * changed.
    */
  PRBool NeedsReflow(const nsSize& aMaxSize);

  void PlaceChild(OuterTableReflowState& aState,
                  nsIFrame*              aKidFrame,
                  const nsRect&          aKidRect,
                  nsSize*                aMaxElementSize,
                  nsSize&                aKidMaxElementSize);

  nscoord GetTableWidth(const nsHTMLReflowState& aReflowState);

  /** overridden here to handle special caption-table relationship
    * @see nsContainerFrame::VerifyTree
    */
  NS_IMETHOD VerifyTree() const;

  /** overridden here to handle special caption-table relationship
    * @see nsContainerFrame::PrepareContinuingFrame
    */
  void PrepareContinuingFrame(nsIPresContext&    aPresContext,
                              nsIFrame*          aParent,
                              nsIStyleContext* aStyleContext,
                              nsTableOuterFrame* aContFrame);

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

  nsresult RecoverState(OuterTableReflowState& aState, nsIFrame* aKidFrame);
  nsresult IncrementalReflow(nsIPresContext* aPresContext,
                             OuterTableReflowState& aState,
                             nsHTMLReflowMetrics& aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus& aStatus);
  nsresult AdjustSiblingsAfterReflow(nsIPresContext*        aPresContext,
                                     OuterTableReflowState& aState,
                                     nsIFrame*              aKidFrame,
                                     nscoord                aDeltaY);

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




#endif
