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
#ifndef nsTableRowFrame_h__
#define nsTableRowFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"

class  nsTableFrame;
class  nsTableCellFrame;

#ifdef DEBUG_TABLE_REFLOW_TIMING
class nsReflowTimer;
#endif

#define NS_ROW_NEED_SPECIAL_REFLOW    0x20000000
#define NS_ROW_FRAME_PAINT_SKIP_ROW   0x00000001
#define NS_ROW_FRAME_PAINT_SKIP_CELLS 0x00000002

/**
 * nsTableRowFrame is the frame that maps table rows 
 * (HTML tag TR). This class cannot be reused
 * outside of an nsTableRowGroupFrame.  It assumes that its parent is an nsTableRowGroupFrame,  
 * and its children are nsTableCellFrames.
 * 
 * @see nsTableFrame
 * @see nsTableRowGroupFrame
 * @see nsTableCellFrame
 */
class nsTableRowFrame : public nsHTMLContainerFrame
{
public:
  virtual ~nsTableRowFrame();

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  /** instantiate a new instance of nsTableRowFrame.
    * @param aResult    the new object is returned in this out-param
    * @param aContent   the table object to map
    * @param aParent    the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableRowFrame(nsIPresShell* aPresShell, nsIFrame** aResult);

  /** @see nsIFrame::Paint */
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);


  /** ask all children to paint themselves, without clipping (for cells with rowspan>1)
    * @see nsIFrame::Paint 
    */
  virtual void PaintChildren(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer,
                             PRUint32             aFlags = 0);

  NS_IMETHOD GetFrameForPoint(nsIPresContext*   aPresContext,
                              const nsPoint&    aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**        aFrame);

  nsTableCellFrame* GetFirstCell() ;

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
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual void DidResize(nsIPresContext*          aPresContext,
                         const nsHTMLReflowState& aReflowState);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableRowFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, 
                    PRUint32*         aResult) const;
#endif
 
  void UpdateHeight(nscoord           aHeight,
                    nscoord           aAscent,
                    nscoord           aDescent,
                    nsTableFrame*     aTableFrame = nsnull,
                    nsTableCellFrame* aCellFrame  = nsnull);

  void ResetHeight(nscoord aRowStyleHeight);

  // calculate the height, considering content height of the 
  // cells and the style height of the row and cells, excluding pct heights
  nscoord CalcHeight(const nsHTMLReflowState& aReflowState);

  // Support for cells with 'vertical-align: baseline'.

  /** 
   * returns the max-ascent amongst all the cells that have 
   * 'vertical-align: baseline', *including* cells with rowspans.
   * returns 0 if we don't have any cell with 'vertical-align: baseline'
   */
  nscoord GetMaxCellAscent() const;
 
#if 0 // nobody uses this
  /** 
   * returns the max-descent amongst all the cells that have
   * 'vertical-align: baseline', *ignoring* any cell with rowspans.
   * returns 0 if we don't have any cell with 'vertical-align: baseline'
   */
  nscoord GetMaxCellDescent() const;
#endif

  /** returns the ordinal position of this row in its table */
  virtual PRInt32 GetRowIndex() const;

  /** set this row's starting row index */
  void SetRowIndex (int aRowIndex);

  virtual PRBool Contains(nsIPresContext* aPresContext, const nsPoint& aPoint);

  /** used by row group frame code */
  nscoord ReflowCellFrame(nsIPresContext*          aPresContext,
                          const nsHTMLReflowState& aReflowState,
                          nsTableCellFrame*        aCellFrame,
                          nscoord                  aAvailableHeight,
                          nsReflowStatus&          aStatus);

  void InsertCellFrame(nsTableCellFrame* aFrame, 
                       nsTableCellFrame* aPrevSibling);

  void InsertCellFrame(nsTableCellFrame* aFrame,
                       PRInt32           aColIndex);

  void RemoveCellFrame(nsTableCellFrame* aFrame);

  nsresult CalculateCellActualSize(nsIFrame*       aRowFrame,
                                   nscoord&        aDesiredWidth,
                                   nscoord&        aDesiredHeight,
                                   nscoord         aAvailWidth);

  PRBool IsFirstInserted() const;
  void   SetFirstInserted(PRBool aValue);

  PRBool NeedSpecialReflow() const;
  void   SetNeedSpecialReflow(PRBool aValue);

  PRBool GetContentHeight() const;
  void   SetContentHeight(nscoord aTwipValue);

  PRBool HasStyleHeight() const;

  PRBool HasFixedHeight() const;
  void   SetHasFixedHeight(PRBool aValue);

  PRBool HasPctHeight() const;
  void   SetHasPctHeight(PRBool aValue);

  nscoord GetFixedHeight() const;
  void    SetFixedHeight(nscoord aValue);

  float   GetPctHeight() const;
  void    SetPctHeight(float  aPctValue,
                       PRBool aForce = PR_FALSE);

  nscoord GetHeight(nscoord aBasis = 0) const;

  nsTableRowFrame* GetNextRow() const;

protected:

  /** protected constructor.
    * @see NewFrame
    */
  nsTableRowFrame();

  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  /** Incremental Reflow attempts to do column balancing with the minimum number of reflow
    * commands to child elements.  This is done by processing the reflow command,
    * rebalancing column widths (if necessary), then comparing the resulting column widths
    * to the prior column widths and reflowing only those cells that require a reflow.
    *
    * @see Reflow
    */
  NS_IMETHOD IncrementalReflow(nsIPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsTableFrame&            aTableFrame,
                               nsReflowStatus&          aStatus);

  NS_IMETHOD IR_TargetIsChild(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsTableFrame&            aTableFrame,
                              nsReflowStatus&          aStatus,
                              nsIFrame*                aNextFrame);

  NS_IMETHOD IR_TargetIsMe(nsIPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsTableFrame&            aTableFrame,
                           nsReflowStatus&          aStatus);

  NS_IMETHOD IR_StyleChanged(nsIPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsTableFrame&            aTableFrame,
                             nsReflowStatus&          aStatus);

  // row-specific methods

  nscoord ComputeCellXOffset(const nsHTMLReflowState& aState,
                             nsIFrame*                aKidFrame,
                             const nsMargin&          aKidMargin) const;
  /**
   * Called for incremental/dirty and resize reflows. If aDirtyOnly is true then
   * only reflow dirty cells.
   */
  NS_IMETHOD ReflowChildren(nsIPresContext*          aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsTableFrame&            aTableFrame,
                            nsReflowStatus&          aStatus,
                            PRBool                   aDirtyOnly = PR_FALSE);

private:
  struct RowBits {
    unsigned mRowIndex:29;
    unsigned mHasFixedHeight:1; // set if the dominating style height on the row or any cell is pixel based
    unsigned mHasPctHeight:1;   // set if the dominating style height on the row or any cell is pct based
    unsigned mFirstInserted:1;  // if true, then it was the top most newly inserted row 
  } mBits;

  nscoord mContentHeight; // the desired height based on the content of the tallest cell in the row
  nscoord mStyleHeight;   // the height based on a style pct on either the row or any cell if mHasPctHeight 
                          // is set, otherwise the height based on a style pixel height on the row or any 
                          // cell if mHasFixedHeight is set

  // max-ascent and max-descent amongst all cells that have 'vertical-align: baseline'
  nscoord mMaxCellAscent;  // does include cells with rowspan > 1
  nscoord mMaxCellDescent; // does *not* include cells with rowspan > 1

#ifdef DEBUG_TABLE_REFLOW_TIMING
public:
  nsReflowTimer* mTimer;
#endif
};

inline PRInt32 nsTableRowFrame::GetRowIndex() const
{
  return PRInt32(mBits.mRowIndex);
}

inline void nsTableRowFrame::SetRowIndex (int aRowIndex)
{
  mBits.mRowIndex = aRowIndex;
}

inline PRBool nsTableRowFrame::IsFirstInserted() const
{
  return PRBool(mBits.mFirstInserted);
}

inline void nsTableRowFrame::SetFirstInserted(PRBool aValue)
{
  mBits.mFirstInserted = aValue;
}

inline PRBool nsTableRowFrame::HasStyleHeight() const
{
  return (PRBool)mBits.mHasFixedHeight || (PRBool)mBits.mHasPctHeight;
}

inline PRBool nsTableRowFrame::HasFixedHeight() const
{
  return (PRBool)mBits.mHasFixedHeight;
}

inline void nsTableRowFrame::SetHasFixedHeight(PRBool aValue)
{
  mBits.mHasFixedHeight = aValue;
}

inline PRBool nsTableRowFrame::HasPctHeight() const
{
  return (PRBool)mBits.mHasPctHeight;
}

inline void nsTableRowFrame::SetHasPctHeight(PRBool aValue)
{
  mBits.mHasPctHeight = aValue;
}

inline nscoord nsTableRowFrame::GetContentHeight() const
{
  return mContentHeight;
}

inline void nsTableRowFrame::SetContentHeight(nscoord aValue)
{
  mContentHeight = aValue;
}

inline nscoord nsTableRowFrame::GetFixedHeight() const
{
  if (mBits.mHasFixedHeight && !mBits.mHasPctHeight) 
    return mStyleHeight;
  else
    return 0;
}

inline float nsTableRowFrame::GetPctHeight() const
{
  if (mBits.mHasPctHeight) 
    return (float)mStyleHeight / 100.0f;
  else
    return 0.0f;
}

inline PRBool nsTableRowFrame::NeedSpecialReflow() const
{
  return (mState & NS_ROW_NEED_SPECIAL_REFLOW) == NS_ROW_NEED_SPECIAL_REFLOW;
}

inline void nsTableRowFrame::SetNeedSpecialReflow(PRBool aValue)
{
  if (aValue) {
    mState |= NS_ROW_NEED_SPECIAL_REFLOW;
  } else {
    mState &= ~NS_ROW_NEED_SPECIAL_REFLOW;
  }
}
#endif
