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
#ifndef nsTableRowGroupFrame_h__
#define nsTableRowGroupFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"
#include "nsIAtom.h"
#include "nsILineIterator.h"
#include "nsTablePainter.h"
#include "nsTArray.h"

class nsTableFrame;
class nsTableRowFrame;
class nsTableCellFrame;

struct nsRowGroupReflowState {
  const nsHTMLReflowState& reflowState;  // Our reflow state

  nsTableFrame* tableFrame;

  // The available size (computed from the parent)
  nsSize availSize;

  // Running y-offset
  nscoord y;

  nsRowGroupReflowState(const nsHTMLReflowState& aReflowState,
                        nsTableFrame*            aTableFrame)
      :reflowState(aReflowState), tableFrame(aTableFrame)
  {
    availSize.width  = reflowState.availableWidth;
    availSize.height = reflowState.availableHeight;
    y = 0;  
  }

  ~nsRowGroupReflowState() {}
};

#define NS_ITABLEROWGROUPFRAME_IID    \
{ 0xe940e7bc, 0xb534, 0x11d2,  \
  { 0x95, 0xa2, 0x0, 0x60, 0xb0, 0xc3, 0x44, 0x14 } }

// use the following bits from nsFrame's frame state 

// thead or tfoot should be repeated on every printed page
#define NS_ROWGROUP_REPEATABLE           0x80000000
#define NS_ROWGROUP_HAS_STYLE_HEIGHT     0x40000000
// the next is also used on rows (see nsTableRowGroupFrame::InitRepeatedFrame)
#define NS_REPEATED_ROW_OR_ROWGROUP      0x10000000
#define NS_ROWGROUP_HAS_ROW_CURSOR       0x08000000

#define MIN_ROWS_NEEDING_CURSOR 20

/**
 * nsTableRowGroupFrame is the frame that maps row groups 
 * (HTML tags THEAD, TFOOT, and TBODY). This class cannot be reused
 * outside of an nsTableFrame.  It assumes that its parent is an nsTableFrame, and 
 * its children are nsTableRowFrames.
 * 
 * @see nsTableFrame
 * @see nsTableRowFrame
 */
class nsTableRowGroupFrame : public nsHTMLContainerFrame, public nsILineIteratorNavigator
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsIFrame* NS_NewTableRowGroupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
  virtual ~nsTableRowGroupFrame();

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

  virtual nsMargin GetUsedMargin() const;
  virtual nsMargin GetUsedBorder() const;
  virtual nsMargin GetUsedPadding() const;

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

   /** calls Reflow for all of its child rows.
    * Rows are all set to the same width and stacked vertically.
    * <P> rows are not split unless absolutely necessary.
    *
    * @param aDesiredSize width set to width of rows, height set to 
    *                     sum of height of rows that fit in aMaxSize.height.
    *
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::tableRowGroupFrame
   */
  virtual nsIAtom* GetType() const;

  virtual PRBool IsContainingBlock() const;

  nsTableRowFrame* GetFirstRow();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  /** return the number of child rows (not necessarily == number of child frames) */
  PRInt32 GetRowCount();

  /** return the table-relative row index of the first row in this rowgroup.
    * if there are no rows, -1 is returned.
    */
  PRInt32 GetStartRowIndex();

  /** Adjust the row indices of all rows  whose index is >= aRowIndex.  
    * @param aRowIndex   - start adjusting with this index
    * @param aAdjustment - shift the row index by this amount
    */
  void AdjustRowIndices(PRInt32   aRowIndex,
                        PRInt32   anAdjustment);

  /**
   * Used for header and footer row group frames that are repeated when
   * splitting a table frame.
   *
   * Performs any table specific initialization
   *
   * @param aHeaderFooterFrame the original header or footer row group frame
   * that was repeated
   */
  nsresult  InitRepeatedFrame(nsPresContext*       aPresContext,
                              nsTableRowGroupFrame* aHeaderFooterFrame);

  
  /**
   * Get the total height of all the row rects
   */
  nscoord GetHeightBasis(const nsHTMLReflowState& aReflowState);
  
  nsMargin* GetBCBorderWidth(nsMargin& aBorder);

  /**
   * Gets inner border widths before collapsing with cell borders
   * Caller must get top border from previous row group or from table
   * GetContinuousBCBorderWidth will not overwrite aBorder.top
   * see nsTablePainter about continuous borders
   */
  void GetContinuousBCBorderWidth(nsMargin& aBorder);
  /**
   * Sets full border widths before collapsing with cell borders
   * @param aForSide - side to set; only right, left, and bottom valid
   */
  void SetContinuousBCBorderWidth(PRUint8     aForSide,
                                  BCPixelSize aPixelValue);
  /**
    * Adjust to the effect of visibibility:collapse on the row group and
    * its children
    * @return              additional shift upward that should be applied to
    *                      subsequent rowgroups due to rows and this rowgroup
    *                      being collapsed
    * @param aYTotalOffset the total amount that the rowgroup is shifted up
    * @param aWidth        new width of the rowgroup
    */
  nscoord CollapseRowGroupIfNecessary(nscoord aYTotalOffset,
                                      nscoord        aWidth);

// nsILineIterator methods
public:
  NS_IMETHOD GetNumLines(PRInt32* aResult);
  NS_IMETHOD GetDirection(PRBool* aIsRightToLeft);
  
  NS_IMETHOD GetLine(PRInt32 aLineNumber,
                     nsIFrame** aFirstFrameOnLine,
                     PRInt32* aNumFramesOnLine,
                     nsRect& aLineBounds,
                     PRUint32* aLineFlags);
  
  NS_IMETHOD FindLineContaining(nsIFrame* aFrame, PRInt32* aLineNumberResult);
  NS_IMETHOD FindLineAt(nscoord aY, PRInt32* aLineNumberResult);
  
  NS_IMETHOD FindFrameAt(PRInt32 aLineNumber,
                         nscoord aX,
                         nsIFrame** aFrameFound,
                         PRBool* aXIsBeforeFirstFrame,
                         PRBool* aXIsAfterLastFrame);

#ifdef IBMBIDI
  NS_IMETHOD CheckLineOrder(PRInt32                  aLine,
                            PRBool                   *aIsReordered,
                            nsIFrame                 **aFirstVisual,
                            nsIFrame                 **aLastVisual);
#endif
  NS_IMETHOD GetNextSiblingOnLine(nsIFrame*& aFrame, PRInt32 aLineNumber);

  // row cursor methods to speed up searching for the row(s)
  // containing a point. The basic idea is that we set the cursor
  // property if the rows' y and yMosts are non-decreasing (considering only
  // rows with nonempty overflowAreas --- empty overflowAreas never participate
  // in event handling or painting), and the rowgroup has sufficient number of
  // rows. The cursor property points to a "recently used" row. If we get a
  // series of requests that work on rows "near" the cursor, then we can find
  // those nearby rows quickly by starting our search at the cursor.
  // This code is based on the line cursor code in nsBlockFrame. It's more general
  // though, and could be extracted and used elsewhere.
  struct FrameCursorData {
    nsTArray<nsIFrame*> mFrames;
    PRUint32            mCursorIndex;
    nscoord             mOverflowAbove;
    nscoord             mOverflowBelow;
    
    FrameCursorData()
      : mFrames(MIN_ROWS_NEEDING_CURSOR), mCursorIndex(0), mOverflowAbove(0),
        mOverflowBelow(0) {}

    PRBool AppendFrame(nsIFrame* aFrame);
    
    void FinishBuildingCursor() {
      mFrames.Compact();
    }
  };

  // Clear out row cursor because we're disturbing the rows (e.g., Reflow)
  void ClearRowCursor();

  /**
   * Get the first row that might contain y-coord 'aY', or nsnull if you must search
   * all rows.
   * The actual row returned might not contain 'aY', but if not, it is guaranteed
   * to be before any row which does contain 'aY'.
   * aOverflowAbove is the maximum over all rows of -row.GetOverflowRect().y.
   * To find all rows that intersect the vertical interval aY/aYMost, call
   * GetFirstRowContaining(aY, &overflowAbove), and then iterate through all
   * rows until reaching a row where row->GetRect().y - overflowAbove >= aYMost.
   * That row and all subsequent rows cannot intersect the interval.
   */
  nsIFrame* GetFirstRowContaining(nscoord aY, nscoord* aOverflowAbove);

  /**
   * Set up the row cursor. After this, call AppendFrame for every
   * child frame in sibling order. Ensure that the child frame y and YMost values
   * form non-decreasing sequences (should always be true for table rows);
   * if this is violated, call ClearRowCursor(). If we return nsnull, then we
   * decided not to use a cursor or we already have one set up.
   */
  FrameCursorData* SetupRowCursor();

protected:
  nsTableRowGroupFrame(nsStyleContext* aContext);

  void InitChildReflowState(nsPresContext&    aPresContext, 
                            PRBool             aBorderCollapse,
                            nsHTMLReflowState& aReflowState);
  
  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  void PlaceChild(nsPresContext*        aPresContext,
                  nsRowGroupReflowState& aReflowState,
                  nsIFrame*              aKidFrame,
                  nsHTMLReflowMetrics&   aDesiredSize);

  void CalculateRowHeights(nsPresContext*          aPresContext, 
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState);

  void DidResizeRows(nsHTMLReflowMetrics& aDesiredSize);

  void SlideChild(nsRowGroupReflowState& aReflowState,
                  nsIFrame*              aKidFrame);
  
  /**
   * Reflow the frames we've already created
   *
   * @param   aPresContext presentation context to use
   * @param   aReflowState current inline state
   * @return  true if we successfully reflowed all the mapped children and false
   *            otherwise, e.g. we pushed children to the next in flow
   */
  NS_METHOD ReflowChildren(nsPresContext*        aPresContext,
                           nsHTMLReflowMetrics&   aDesiredSize,
                           nsRowGroupReflowState& aReflowState,
                           nsReflowStatus&        aStatus,
                           PRBool*                aPageBreakBeforeEnd = nsnull);

  nsresult SplitRowGroup(nsPresContext*          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsTableFrame*            aTableFrame,
                         nsReflowStatus&          aStatus);

  void SplitSpanningCells(nsPresContext&          aPresContext,
                          const nsHTMLReflowState& aReflowState,
                          nsTableFrame&            aTableFrame,
                          nsTableRowFrame&         aFirstRow, 
                          nsTableRowFrame&         aLastRow,  
                          PRBool                   aFirstRowIsTopOfPage,
                          nscoord                  aAvailHeight,
                          nsTableRowFrame*&        aContRowFrame,
                          nsTableRowFrame*&        aFirstTruncatedRow,
                          nscoord&                 aDesiredHeight);

  void CreateContinuingRowFrame(nsPresContext& aPresContext,
                                nsIFrame&       aRowFrame,
                                nsIFrame**      aContRowFrame);

  PRBool IsSimpleRowFrame(nsTableFrame* aTableFrame, 
                          nsIFrame*     aFrame);

  void GetNextRowSibling(nsIFrame** aRowFrame);

  void UndoContinuedRow(nsPresContext*  aPresContext,
                        nsTableRowFrame* aRow);
                        
private:
  // border widths in pixels in the collapsing border model
  BCPixelSize mRightContBorderWidth;
  BCPixelSize mBottomContBorderWidth;
  BCPixelSize mLeftContBorderWidth;

public:
  virtual nsIFrame* GetFirstFrame() { return mFrames.FirstChild(); };
  virtual nsIFrame* GetLastFrame() { return mFrames.LastChild(); };
  virtual void GetNextFrame(nsIFrame*  aFrame, 
                            nsIFrame** aResult) { *aResult = aFrame->GetNextSibling(); };
  PRBool IsRepeatable() const;
  void   SetRepeatable(PRBool aRepeatable);
  PRBool HasStyleHeight() const;
  void   SetHasStyleHeight(PRBool aValue);
};


inline PRBool nsTableRowGroupFrame::IsRepeatable() const
{
  return (mState & NS_ROWGROUP_REPEATABLE) == NS_ROWGROUP_REPEATABLE;
}

inline void nsTableRowGroupFrame::SetRepeatable(PRBool aRepeatable)
{
  if (aRepeatable) {
    mState |= NS_ROWGROUP_REPEATABLE;
  } else {
    mState &= ~NS_ROWGROUP_REPEATABLE;
  }
}

inline PRBool nsTableRowGroupFrame::HasStyleHeight() const
{
  return (mState & NS_ROWGROUP_HAS_STYLE_HEIGHT) == NS_ROWGROUP_HAS_STYLE_HEIGHT;
}

inline void nsTableRowGroupFrame::SetHasStyleHeight(PRBool aValue)
{
  if (aValue) {
    mState |= NS_ROWGROUP_HAS_STYLE_HEIGHT;
  } else {
    mState &= ~NS_ROWGROUP_HAS_STYLE_HEIGHT;
  }
}

inline void
nsTableRowGroupFrame::GetContinuousBCBorderWidth(nsMargin& aBorder)
{
  PRInt32 aPixelsToTwips = nsPresContext::AppUnitsPerCSSPixel();
  aBorder.right = BC_BORDER_LEFT_HALF_COORD(aPixelsToTwips,
                                            mRightContBorderWidth);
  aBorder.bottom = BC_BORDER_TOP_HALF_COORD(aPixelsToTwips,
                                            mBottomContBorderWidth);
  aBorder.left = BC_BORDER_RIGHT_HALF_COORD(aPixelsToTwips,
                                            mLeftContBorderWidth);
  return;
}
#endif
