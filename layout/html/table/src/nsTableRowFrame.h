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

  /** instantiate a new instance of nsTableRowFrame.
    * @param aInstancePtrResult  the new object is returned in this out-param
    * @param aContent            the table object to map
    * @param aIndexInParent      which child is the new frame?
    * @param aParent             the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  static nsresult NewFrame(nsIFrame** aInstancePtrResult,
                           nsIContent* aContent,
                           PRInt32     aIndexInParent,
                           nsIFrame*   aParent);

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

  /** calls ResizeReflow for all of its child cells.
    * Cells with rowspan=1 are all set to the same height and stacked horizontally.
    * <P> Cells are not split unless absolutely necessary.
    * <P> Cells are resized in nsTableFrame::BalanceColumnWidths 
    * and nsTableFrame::ShrinkWrapChildren
    *
    * @param aDesiredSize width set to width of the sum of the cells, height set to 
    *                     height of cells with rowspan=1.
    *
    * @see nsIFrame::ResizeReflow
    * @see nsTableFrame::BalanceColumnWidths
    * @see nsTableFrame::ShrinkWrapChildren
    */
  NS_IMETHOD ResizeReflow(nsIPresContext*  aPresContext,
                          nsReflowMetrics& aDesiredSize,
                          const nsSize&    aMaxSize,
                          nsSize*          aMaxElementSize,
                          ReflowStatus&    aStatus);

  /** @see nsIFrame::IncrementalReflow */
  NS_IMETHOD IncrementalReflow(nsIPresContext*  aPresContext,
                               nsReflowMetrics& aDesiredSize,
                               const nsSize&    aMaxSize,
                               nsReflowCommand& aReflowCommand,
                               ReflowStatus&    aStatus);

  /** @see nsContainerFrame::CreateContinuingFrame */
  NS_IMETHOD CreateContinuingFrame(nsIPresContext* aPresContext,
                                   nsIFrame*       aParent,
                                   nsIFrame*&      aContinuingFrame);
  
  /** set mTallestCell to 0 in anticipation of recalculating it */
  void ResetMaxChildHeight();

  /** set mTallestCell to max(mTallestCell, aChildHeight) */ 
  void SetMaxChildHeight(PRInt32 aChildHeight);

  /** returns the tallest child in this row (ignoring any cell with rowspans) */
  PRInt32 GetTallestChild() const;


protected:

  /** protected constructor.
    * @see NewFrame
    */
    nsTableRowFrame(nsIContent* aContent,
                  PRInt32 aIndexInParent,
					        nsIFrame* aParentFrame);

    /** destructor */
    virtual ~nsTableRowFrame();

private:
  PRInt32 mTallestCell; // not my height, but the height of my tallest child

};

#endif
