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
#ifndef nsTableCellFrame_h__
#define nsTableCellFrame_h__

#include "nsITableCellLayout.h"
#include "nscore.h"
#include "nsHTMLContainerFrame.h"
#include "nsTableRowFrame.h"  // need to actually include this here to inline GetRowIndex
#include "nsStyleContext.h"
#include "nsIPercentHeightObserver.h"
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "nsTArray.h"

class nsTableFrame;

/**
 * Additional frame-state bits
 */
#define NS_TABLE_CELL_CONTENT_EMPTY       NS_FRAME_STATE_BIT(31)
#define NS_TABLE_CELL_HAD_SPECIAL_REFLOW  NS_FRAME_STATE_BIT(29)
#define NS_TABLE_CELL_HAS_PCT_OVER_HEIGHT NS_FRAME_STATE_BIT(28)

/**
 * nsTableCellFrame
 * data structure to maintain information about a single table cell's frame
 *
 * NOTE:  frames are not ref counted.  We expose addref and release here
 * so we can change that decsion in the future.  Users of nsITableCellLayout
 * should refcount correctly as if this object is being ref counted, though
 * no actual support is under the hood.
 *
 * @author  sclark
 */
class nsTableCellFrame : public nsHTMLContainerFrame,
                         public nsITableCellLayout,
                         public nsIPercentHeightObserver
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsTableCellFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // default constructor supplied by the compiler

  nsTableCellFrame(nsStyleContext* aContext);
  ~nsTableCellFrame();

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  /** @see nsIFrame::DidSetStyleContext */
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

  // table cells contain a block frame which does most of the work, and
  // so these functions should never be called. They assert and return
  // NS_ERROR_NOT_IMPLEMENTED
  NS_IMETHOD AppendFrames(nsIAtom*        aListName,
                          nsFrameList&    aFrameList);
  NS_IMETHOD InsertFrames(nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList);
  NS_IMETHOD RemoveFrame(nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  virtual nsIFrame* GetContentInsertionFrame() {
    return GetFirstChild(nsnull)->GetContentInsertionFrame();
  }

  virtual nsMargin GetUsedMargin() const;

  virtual void NotifyPercentHeight(const nsHTMLReflowState& aReflowState);

  virtual PRBool NeedsToObserve(const nsHTMLReflowState& aReflowState);

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsIFrame* NS_NewTableCellFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  void PaintCellBackground(nsIRenderingContext& aRenderingContext,
                           const nsRect& aDirtyRect, nsPoint aPt,
                           PRUint32 aFlags);

  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);
  virtual IntrinsicWidthOffsetData
    IntrinsicWidthOffsets(nsIRenderingContext* aRenderingContext);

  NS_IMETHOD Reflow(nsPresContext*      aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableCellFrame
   */
  virtual nsIAtom* GetType() const;

  virtual PRBool IsContainingBlock() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  void VerticallyAlignChild(nscoord aMaxAscent);

  /*
   * Get the value of vertical-align adjusted for CSS 2's rules for a
   * table cell, which means the result is always
   * NS_STYLE_VERTICAL_ALIGN_{TOP,MIDDLE,BOTTOM,BASELINE}.
   */
  PRUint8 GetVerticalAlign() const;

  PRBool HasVerticalAlignBaseline() const {
    return GetVerticalAlign() == NS_STYLE_VERTICAL_ALIGN_BASELINE;
  }

  PRBool CellHasVisibleContent(nscoord       height,
                               nsTableFrame* tableFrame,
                               nsIFrame*     kidFrame);

  /**
   * Get the first-line baseline of the cell relative to its top border
   * edge, as if the cell were vertically aligned to the top of the row.
   */
  nscoord GetCellBaseline() const;

  /**
   * return the cell's specified row span. this is what was specified in the
   * content model or in the style info, and is always >= 1.
   * to get the effective row span (the actual value that applies), use GetEffectiveRowSpan()
   * @see nsTableFrame::GetEffectiveRowSpan()
   */
  virtual PRInt32 GetRowSpan();

  // there is no set row index because row index depends on the cell's parent row only

  /*---------------- nsITableCellLayout methods ------------------------*/

  /**
   * return the cell's starting row index (starting at 0 for the first row).
   * for continued cell frames the row index is that of the cell's first-in-flow
   * and the column index (starting at 0 for the first column
   */
  NS_IMETHOD GetCellIndexes(PRInt32 &aRowIndex, PRInt32 &aColIndex);

  /** return the mapped cell's row index (starting at 0 for the first row) */
  virtual nsresult GetRowIndex(PRInt32 &aRowIndex) const;

  /**
   * return the cell's specified col span. this is what was specified in the
   * content model or in the style info, and is always >= 1.
   * to get the effective col span (the actual value that applies), use GetEffectiveColSpan()
   * @see nsTableFrame::GetEffectiveColSpan()
   */
  virtual PRInt32 GetColSpan();

  /** return the cell's column index (starting at 0 for the first column) */
  virtual nsresult GetColIndex(PRInt32 &aColIndex) const;
  void SetColIndex(PRInt32 aColIndex);

  /** return the available width given to this frame during its last reflow */
  inline nscoord GetPriorAvailWidth();

  /** set the available width given to this frame during its last reflow */
  inline void SetPriorAvailWidth(nscoord aPriorAvailWidth);

  /** return the desired size returned by this frame during its last reflow */
  inline nsSize GetDesiredSize();

  /** set the desired size returned by this frame during its last reflow */
  inline void SetDesiredSize(const nsHTMLReflowMetrics & aDesiredSize);

  PRBool GetContentEmpty();
  void SetContentEmpty(PRBool aContentEmpty);

  PRBool HasPctOverHeight();
  void SetHasPctOverHeight(PRBool aValue);

  nsTableCellFrame* GetNextCell() const;

  virtual nsMargin* GetBorderWidth(nsMargin& aBorder) const;

  virtual void PaintBackground(nsIRenderingContext& aRenderingContext,
                               const nsRect&        aDirtyRect,
                               nsPoint              aPt,
                               PRUint32             aFlags);

  void DecorateForSelection(nsIRenderingContext& aRenderingContext,
                            nsPoint              aPt);

protected:
  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  /**
   * GetSelfOverflow says what effect the cell should have on its own
   * overflow area.  In the separated borders model this should just be
   * the frame's size (as it is for most frames), but in the collapsed
   * borders model (for which nsBCTableCellFrame overrides this virtual
   * method), it considers the extents of the collapsed border so we
   * handle invalidation correctly for dynamic border changes.
   */
  virtual void GetSelfOverflow(nsRect& aOverflowArea);

  friend class nsTableRowFrame;

  PRUint32     mColIndex;             // the starting column for this cell

  nscoord      mPriorAvailWidth;      // the avail width during the last reflow
  nsSize       mDesiredSize;          // the last desired width & height
};

inline nscoord nsTableCellFrame::GetPriorAvailWidth()
{ return mPriorAvailWidth;}

inline void nsTableCellFrame::SetPriorAvailWidth(nscoord aPriorAvailWidth)
{ mPriorAvailWidth = aPriorAvailWidth;}

inline nsSize nsTableCellFrame::GetDesiredSize()
{ return mDesiredSize; }

inline void nsTableCellFrame::SetDesiredSize(const nsHTMLReflowMetrics & aDesiredSize)
{
  mDesiredSize.width = aDesiredSize.width;
  mDesiredSize.height = aDesiredSize.height;
}

inline PRBool nsTableCellFrame::GetContentEmpty()
{
  return (mState & NS_TABLE_CELL_CONTENT_EMPTY) ==
         NS_TABLE_CELL_CONTENT_EMPTY;
}

inline void nsTableCellFrame::SetContentEmpty(PRBool aContentEmpty)
{
  if (aContentEmpty) {
    mState |= NS_TABLE_CELL_CONTENT_EMPTY;
  } else {
    mState &= ~NS_TABLE_CELL_CONTENT_EMPTY;
  }
}

inline PRBool nsTableCellFrame::HasPctOverHeight()
{
  return (mState & NS_TABLE_CELL_HAS_PCT_OVER_HEIGHT) ==
         NS_TABLE_CELL_HAS_PCT_OVER_HEIGHT;
}

inline void nsTableCellFrame::SetHasPctOverHeight(PRBool aValue)
{
  if (aValue) {
    mState |= NS_TABLE_CELL_HAS_PCT_OVER_HEIGHT;
  } else {
    mState &= ~NS_TABLE_CELL_HAS_PCT_OVER_HEIGHT;
  }
}

// nsBCTableCellFrame
class nsBCTableCellFrame : public nsTableCellFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsBCTableCellFrame(nsStyleContext* aContext);

  ~nsBCTableCellFrame();

  virtual nsIAtom* GetType() const;

  virtual nsMargin GetUsedBorder() const;
  virtual PRBool GetBorderRadii(nscoord aRadii[8]) const;

  // Get the *inner half of the border only*, in twips.
  virtual nsMargin* GetBorderWidth(nsMargin& aBorder) const;

  // Get the *inner half of the border only*, in pixels.
  BCPixelSize GetBorderWidth(mozilla::css::Side aSide) const;

  // Set the full (both halves) width of the border
  void SetBorderWidth(mozilla::css::Side aSide, BCPixelSize aPixelValue);

  virtual void GetSelfOverflow(nsRect& aOverflowArea);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  virtual void PaintBackground(nsIRenderingContext& aRenderingContext,
                               const nsRect&        aDirtyRect,
                               nsPoint              aPt,
                               PRUint32             aFlags);

private:

  // These are the entire width of the border (the cell edge contains only
  // the inner half, per the macros in nsTablePainter.h).
  BCPixelSize mTopBorder;
  BCPixelSize mRightBorder;
  BCPixelSize mBottomBorder;
  BCPixelSize mLeftBorder;
};

#endif
