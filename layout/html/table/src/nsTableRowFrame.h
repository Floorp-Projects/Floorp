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
#ifndef nsTableRowFrame_h__
#define nsTableRowFrame_h__

#include "nscore.h"
#include "nsContainerFrame.h"

class  nsTableFrame;
class  nsTableCellFrame;
struct RowReflowState;

/* e8417220-070b-11d2-8f37-006008159b0c */
#define NS_TABLEROWFRAME_CID \
 {0xe8417220, 0x070b, 0x11d2, {0x8f, 0x37, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x0c}}

extern const nsIID kTableRowFrameCID;

/**
 * nsTableRowFrame is the frame that maps table rows 
 * (HTML tag TR). This class cannot be reused
 * outside of an nsTableRowGroupFrame.  It assumes that its parent is an nsTableRowGroupFrame,  
 * and its children are nsTableCellFrames.
 * 
 * @see nsTableFrame
 * @see nsTableRowGroupFrame
 * @see nsTableCellFrame
 *
 * @author  sclark
 */
class nsTableRowFrame : public nsContainerFrame
{
public:
  /** Initialization procedure */
  void Init(PRInt32 aRowIndex);

  /** instantiate a new instance of nsTableRowFrame.
    * @param aInstancePtrResult  the new object is returned in this out-param
    * @param aContent            the table object to map
    * @param aParent             the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  static nsresult NewFrame(nsIFrame** aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent);

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  /** @see nsIFrame::Paint */
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);


  /** ask all children to paint themselves, without clipping (for cells with rowspan>1)
    * @see nsIFrame::Paint 
    */
  virtual void PaintChildren(nsIPresContext&      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect);

  /** calls Reflow for all of its child cells.
    * Cells with rowspan=1 are all set to the same height and stacked horizontally.
    * <P> Cells are not split unless absolutely necessary.
    * <P> Cells are resized in nsTableFrame::BalanceColumnWidths 
    * and nsTableFrame::ShrinkWrapChildren
    *
    * @param aDesiredSize width set to width of the sum of the cells, height set to 
    *                     height of cells with rowspan=1.
    *
    * @see nsIFrame::Reflow
    * @see nsTableFrame::BalanceColumnWidths
    * @see nsTableFrame::ShrinkWrapChildren
    */
  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  /** @see nsContainerFrame::CreateContinuingFrame */
  NS_IMETHOD CreateContinuingFrame(nsIPresContext*  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);
  
  /** set mTallestCell to 0 in anticipation of recalculating it */
  void ResetMaxChildHeight();

  /** set mTallestCell to max(mTallestCell, aChildHeight) */ 
  void SetMaxChildHeight(nscoord aChildHeight, nscoord aCellTopMargin, nscoord aCellBottomMargin);

  /** returns the tallest child in this row (ignoring any cell with rowspans) */
  nscoord GetTallestChild() const;
  nscoord GetChildMaxTopMargin() const;
  nscoord GetChildMaxBottomMargin() const;

  PRInt32 GetMaxColumns() const;

  /** returns the ordinal position of this row in its table */
  virtual PRInt32 GetRowIndex() const;

  /** set this row's starting row index */
  virtual void SetRowIndex (int aRowIndex);

  // For DEBUGGING Purposes Only
  NS_IMETHOD  MoveTo(nscoord aX, nscoord aY);
  NS_IMETHOD  SizeTo(nscoord aWidth, nscoord aHeight);

protected:

  /** protected constructor.
    * @see NewFrame
    */
  nsTableRowFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  /** destructor */
  virtual ~nsTableRowFrame();

  // row-specific methods

  void nsTableRowFrame::GetMinRowSpan();

  void nsTableRowFrame::FixMinCellHeight();



  //overrides 

  nscoord GetTopMarginFor(nsIPresContext*   aCX,
                          RowReflowState&   aState,
                          const nsMargin&   aKidMargin);

  void          PlaceChild( nsIPresContext* aPresContext,
                            RowReflowState& aState,
                            nsIFrame*       aKidFrame,
                            const nsRect&   aKidRect,
                            nsSize*         aMaxElementSize,
                            nsSize&         aKidMaxElementSize);

  /**
   * Reflow the frames we've already created
   *
   * @param   aPresContext presentation context to use
   * @param   aState current inline state
   * @return  true if we successfully reflowed all the mapped children and false
   *            otherwise, e.g. we pushed children to the next in flow
   */
  PRBool        ReflowMappedChildren(nsIPresContext* aPresContext,
                                     RowReflowState& aState,
                                     nsSize*         aMaxElementSize);

  /**
   * Try and pull-up frames from our next-in-flow
   *
   * @param   aPresContext presentation context to use
   * @param   aState current inline state
   * @return  true if we successfully pulled-up all the children and false
   *            otherwise, e.g. child didn't fit
   */
  PRBool        PullUpChildren(nsIPresContext* aPresContext,
                               RowReflowState& aState,
                               nsSize*         aMaxElementSize);

  /**
   * Create new frames for content we haven't yet mapped
   *
   * @param   aPresContext presentation context to use
   * @param   aState current inline state
   * @return  frComplete if all content has been mapped and frNotComplete
   *            if we should be continued
   */
  nsReflowStatus  ReflowUnmappedChildren(nsIPresContext* aPresContext,
                                         RowReflowState& aState,
                                         nsSize*         aMaxElementSize);

private:
  PRInt32  mRowIndex;
  nscoord  mTallestCell;          // not my height, but the height of my tallest child
  nscoord  mCellMaxTopMargin;
  nscoord  mCellMaxBottomMargin;
  PRInt32  mMinRowSpan;           // the smallest row span among all my child cells

};


inline void nsTableRowFrame::Init(PRInt32 aRowIndex)
{
  NS_ASSERTION(0<=aRowIndex, "bad param row index");
  mRowIndex = aRowIndex;
}

inline PRInt32 nsTableRowFrame::GetRowIndex() const
{
  NS_ASSERTION(0<=mRowIndex, "bad state: row index");
  return (mRowIndex);
}

#endif
