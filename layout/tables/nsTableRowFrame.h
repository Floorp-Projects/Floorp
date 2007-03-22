/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsTableRowFrame_h__
#define nsTableRowFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"
#include "nsTablePainter.h"

class  nsTableFrame;
class  nsTableCellFrame;
struct nsTableCellReflowState;

#define NS_TABLE_ROW_HAS_UNPAGINATED_HEIGHT 0x40000000
// This is also used on rows, from nsTableRowGroupFrame.h
// #define NS_REPEATED_ROW_OR_ROWGROUP      0x10000000

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

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD AppendFrames(nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsIFrame* NS_NewTableRowFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual nsMargin GetUsedMargin() const;
  virtual nsMargin GetUsedBorder() const;
  virtual nsMargin GetUsedPadding() const;

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

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
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  void DidResize();

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::tableRowFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
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

  /* return the row ascent
   */
  nscoord GetRowBaseline();
 
  /** returns the ordinal position of this row in its table */
  virtual PRInt32 GetRowIndex() const;

  /** set this row's starting row index */
  void SetRowIndex (int aRowIndex);

  /** used by row group frame code */
  nscoord ReflowCellFrame(nsPresContext*          aPresContext,
                          const nsHTMLReflowState& aReflowState,
                          PRBool                   aIsTopOfPage,
                          nsTableCellFrame*        aCellFrame,
                          nscoord                  aAvailableHeight,
                          nsReflowStatus&          aStatus);
  /**
    * Collapse the row if required, apply col and colgroup visibility: collapse
    * info to the cells in the row.
    * @return he amount to shift up all following rows
    * @param aRowOffset     - shift the row up by this amount
    * @param aWidth         - new width of the row
    * @param aCollapseGroup - parent rowgroup is collapsed so this row needs
    *                         to be collapsed
    * @param aDidCollapse   - the row has been collapsed
    */
  nscoord CollapseRowIfNecessary(nscoord aRowOffset,
                                 nscoord aWidth,
                                 PRBool  aCollapseGroup,
                                 PRBool& aDidCollapse);

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

  PRBool  HasUnpaginatedHeight();
  void    SetHasUnpaginatedHeight(PRBool aValue);
  nscoord GetUnpaginatedHeight(nsPresContext* aPresContext);
  void    SetUnpaginatedHeight(nsPresContext* aPresContext, nscoord aValue);

  nscoord GetTopBCBorderWidth();
  void    SetTopBCBorderWidth(BCPixelSize aWidth);
  nscoord GetBottomBCBorderWidth();
  void    SetBottomBCBorderWidth(BCPixelSize aWidth);
  nsMargin* GetBCBorderWidth(nsMargin& aBorder);
                             
  /**
   * Gets inner border widths before collapsing with cell borders
   * Caller must get bottom border from next row or from table
   * GetContinuousBCBorderWidth will not overwrite aBorder.bottom
   * see nsTablePainter about continuous borders
   */
  void GetContinuousBCBorderWidth(nsMargin& aBorder);
  /**
   * @returns outer top bc border == prev row's bottom inner
   */
  nscoord GetOuterTopContBCBorderWidth();
  /**
   * Sets full border widths before collapsing with cell borders
   * @param aForSide - side to set; only accepts right, left, and top
   */
  void SetContinuousBCBorderWidth(PRUint8     aForSide,
                                  BCPixelSize aPixelValue);

protected:

  /** protected constructor.
    * @see NewFrame
    */
  nsTableRowFrame(nsStyleContext *aContext);

  void InitChildReflowState(nsPresContext&         aPresContext,
                            const nsSize&           aAvailSize,
                            PRBool                  aBorderCollapse,
                            nsTableCellReflowState& aReflowState);
  
  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  // row-specific methods

  nscoord ComputeCellXOffset(const nsHTMLReflowState& aState,
                             nsIFrame*                aKidFrame,
                             const nsMargin&          aKidMargin) const;
  /**
   * Called for incremental/dirty and resize reflows. If aDirtyOnly is true then
   * only reflow dirty cells.
   */
  NS_IMETHOD ReflowChildren(nsPresContext*          aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsTableFrame&            aTableFrame,
                            nsReflowStatus&          aStatus);

private:
  struct RowBits {
    unsigned mRowIndex:29;
    unsigned mHasFixedHeight:1; // set if the dominating style height on the row or any cell is pixel based
    unsigned mHasPctHeight:1;   // set if the dominating style height on the row or any cell is pct based
    unsigned mFirstInserted:1;  // if true, then it was the top most newly inserted row 
  } mBits;

  // the desired height based on the content of the tallest cell in the row
  nscoord mContentHeight;
  // the height based on a style percentage height on either the row or any cell
  // if mHasPctHeight is set
  nscoord mStylePctHeight;
  // the height based on a style pixel height on the row or any
  // cell if mHasFixedHeight is set
  nscoord mStyleFixedHeight;

  // max-ascent and max-descent amongst all cells that have 'vertical-align: baseline'
  nscoord mMaxCellAscent;  // does include cells with rowspan > 1
  nscoord mMaxCellDescent; // does *not* include cells with rowspan > 1

  // border widths in pixels in the collapsing border model of the *inner*
  // half of the border only
  BCPixelSize mTopBorderWidth;
  BCPixelSize mBottomBorderWidth;
  BCPixelSize mRightContBorderWidth;
  BCPixelSize mTopContBorderWidth;
  BCPixelSize mLeftContBorderWidth;
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
  if (mBits.mHasFixedHeight)
    return mStyleFixedHeight;
  else
    return 0;
}

inline float nsTableRowFrame::GetPctHeight() const
{
  if (mBits.mHasPctHeight) 
    return (float)mStylePctHeight / 100.0f;
  else
    return 0.0f;
}

inline PRBool nsTableRowFrame::HasUnpaginatedHeight()
{
  return (mState & NS_TABLE_ROW_HAS_UNPAGINATED_HEIGHT) ==
         NS_TABLE_ROW_HAS_UNPAGINATED_HEIGHT;
}

inline void nsTableRowFrame::SetHasUnpaginatedHeight(PRBool aValue)
{
  if (aValue) {
    mState |= NS_TABLE_ROW_HAS_UNPAGINATED_HEIGHT;
  } else {
    mState &= ~NS_TABLE_ROW_HAS_UNPAGINATED_HEIGHT;
  }
}

inline nscoord nsTableRowFrame::GetTopBCBorderWidth()
{
  return mTopBorderWidth;
}

inline void nsTableRowFrame::SetTopBCBorderWidth(BCPixelSize aWidth)
{
  mTopBorderWidth = aWidth;
}

inline nscoord nsTableRowFrame::GetBottomBCBorderWidth()
{
  return mBottomBorderWidth;
}

inline void nsTableRowFrame::SetBottomBCBorderWidth(BCPixelSize aWidth)
{
  mBottomBorderWidth = aWidth;
}

inline nsMargin* nsTableRowFrame::GetBCBorderWidth(nsMargin& aBorder)
{
  aBorder.left = aBorder.right = 0;

  aBorder.top    = nsPresContext::CSSPixelsToAppUnits(mTopBorderWidth);
  aBorder.bottom = nsPresContext::CSSPixelsToAppUnits(mBottomBorderWidth);

  return &aBorder;
}

inline void
nsTableRowFrame::GetContinuousBCBorderWidth(nsMargin& aBorder)
{
  PRInt32 aPixelsToTwips = nsPresContext::AppUnitsPerCSSPixel();
  aBorder.right = BC_BORDER_LEFT_HALF_COORD(aPixelsToTwips,
                                            mLeftContBorderWidth);
  aBorder.top = BC_BORDER_BOTTOM_HALF_COORD(aPixelsToTwips,
                                            mTopContBorderWidth);
  aBorder.left = BC_BORDER_RIGHT_HALF_COORD(aPixelsToTwips,
                                            mRightContBorderWidth);
}

inline nscoord nsTableRowFrame::GetOuterTopContBCBorderWidth()
{
  PRInt32 aPixelsToTwips = nsPresContext::AppUnitsPerCSSPixel();
  return BC_BORDER_TOP_HALF_COORD(aPixelsToTwips, mTopContBorderWidth);
}

#endif
