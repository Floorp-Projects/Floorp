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
#ifndef nsTableRowGroupFrame_h__
#define nsTableRowGroupFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"
#include "nsIAtom.h"
#include "nsILineIterator.h"

class nsTableFrame;
class nsTableRowFrame;
class nsTableCellFrame;
#ifdef DEBUG_TABLE_REFLOW_TIMING
class nsReflowTimer;
#endif

struct nsRowGroupReflowState {
  const nsHTMLReflowState& reflowState;  // Our reflow state

  nsTableFrame* tableFrame;

  nsReflowReason reason;

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
    reason = reflowState.reason;
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
// we need a 3rd pass reflow to deal with pct height nested tables 
#define NS_ROWGROUP_NEED_SPECIAL_REFLOW  0x20000000
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

  // default constructor supplied by the compiler

  /** instantiate a new instance of nsTableRowGroupFrame.
    * @param aResult    the new object is returned in this out-param
    * @param aContent   the table object to map
    * @param aParent    the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableRowGroupFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
  virtual ~nsTableRowGroupFrame();

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

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

  /**
   * Find the correct descendant frame.
   * Return PR_TRUE if a frame containing the point is found.
   * @see nsContainerFrame::GetFrameForPoint
   */
  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext, const nsPoint& aPoint, nsFramePaintLayer aWhichLayer, nsIFrame** aFrame);

   /** calls Reflow for all of its child rows.
    * Rows are all set to the same width and stacked vertically.
    * <P> rows are not split unless absolutely necessary.
    *
    * @param aDesiredSize width set to width of rows, height set to 
    *                     sum of height of rows that fit in aMaxSize.height.
    *
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableRowGroupFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  NS_IMETHOD IsPercentageBase(PRBool& aBase) const;

  nsTableRowFrame* GetFirstRow();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

  /** return the number of child rows (not necessarily == number of child frames) */
  PRInt32 GetRowCount();

  /** return the table-relative row index of the first row in this rowgroup.
    * if there are no rows, -1 is returned.
    */
  PRInt32 GetStartRowIndex();

  /**
   * Used for header and footer row group frames that are repeated when
   * splitting a table frame.
   *
   * Performs any table specific initialization
   *
   * @param aHeaderFooterFrame the original header or footer row group frame
   * that was repeated
   */
  nsresult  InitRepeatedFrame(nsIPresContext*       aPresContext,
                              nsTableRowGroupFrame* aHeaderFooterFrame);

  
  /**
   * Get the total height of all the row rects
   */
  nscoord GetHeightOfRows(nsIPresContext* aPresContext);
  nscoord GetHeightBasis(const nsHTMLReflowState& aReflowState);
  
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
#ifdef IBMBIDI
                         PRBool aCouldBeReordered,
#endif
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


protected:
  nsTableRowGroupFrame();

  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  void PlaceChild(nsIPresContext*        aPresContext,
                  nsRowGroupReflowState& aReflowState,
                  nsIFrame*              aKidFrame,
                  nsHTMLReflowMetrics&   aDesiredSize);

  void CalculateRowHeights(nsIPresContext*          aPresContext, 
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsTableRowFrame*         aStartRowFrameIn = nsnull);

  void DidResizeRows(nsIPresContext&          aPresContext,
                     const nsHTMLReflowState& aReflowState,
                     nsTableRowFrame*         aStartRowFrameIn = nsnull);

  /** Incremental Reflow attempts to do column balancing with the minimum number of reflow
    * commands to child elements.  This is done by processing the reflow command,
    * rebalancing column widths (if necessary), then comparing the resulting column widths
    * to the prior column widths and reflowing only those cells that require a reflow.
    *
    * @see Reflow
    */
  NS_IMETHOD IncrementalReflow(nsIPresContext*        aPresContext,
                               nsHTMLReflowMetrics&   aDesiredSize,
                               nsRowGroupReflowState& aReflowState,
                               nsReflowStatus&        aStatus);

  NS_IMETHOD IR_TargetIsChild(nsIPresContext*        aPresContext,
                              nsHTMLReflowMetrics&   aDesiredSize,
                              nsRowGroupReflowState& aReflowState,
                              nsReflowStatus&        aStatus,
                              nsIFrame*              aNextFrame);

  NS_IMETHOD IR_TargetIsMe(nsIPresContext*        aPresContext,
                           nsHTMLReflowMetrics&   aDesiredSize,
                           nsRowGroupReflowState& aReflowState,
                           nsReflowStatus&        aStatus);

  NS_IMETHOD IR_StyleChanged(nsIPresContext*        aPresContext,
                             nsHTMLReflowMetrics&   aDesiredSize,
                             nsRowGroupReflowState& aReflowState,
                             nsReflowStatus&        aStatus);

  nsresult AdjustSiblingsAfterReflow(nsIPresContext*        aPresContext,
                                     nsRowGroupReflowState& aReflowState,
                                     nsIFrame*              aKidFrame,
                                     nscoord                aDeltaY);
  
  nsresult RecoverState(nsRowGroupReflowState& aReflowState,
                        nsIFrame*              aKidFrame);

  /**
   * Reflow the frames we've already created
   *
   * @param   aPresContext presentation context to use
   * @param   aReflowState current inline state
   * @return  true if we successfully reflowed all the mapped children and false
   *            otherwise, e.g. we pushed children to the next in flow
   */
  NS_METHOD ReflowChildren(nsIPresContext*        aPresContext,
                           nsHTMLReflowMetrics&   aDesiredSize,
                           nsRowGroupReflowState& aReflowState,
                           nsReflowStatus&        aStatus,
                           nsTableRowFrame*       aStartFrame,
                           PRBool                 aDirtyOnly,
                           nsTableRowFrame**      aFirstRowReflowed = nsnull);

  nsresult SplitRowGroup(nsIPresContext*          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsTableFrame*            aTableFrame,
                         nsReflowStatus&          aStatus);

  nscoord SplitSpanningCells(nsIPresContext&          aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsIStyleSet&             aStyleSet,
                             nsTableFrame&            aTableFrame,
                             nsTableRowFrame&         aRowFrame,
                             nscoord                  aRowEndY,
                             nsTableRowFrame*         aContRowFrame);

  void CreateContinuingRowFrame(nsIPresContext& aPresContext,
                                nsIStyleSet&    aStyleSet,
                                nsIFrame&       aRowFrame,
                                nsIFrame**      aContRowFrame);

  PRBool IsSimpleRowFrame(nsTableFrame* aTableFrame, 
                          nsIFrame*     aFrame);

  void GetNextRowSibling(nsIFrame** aRowFrame);

public:
  virtual nsIFrame* GetFirstFrame() { return mFrames.FirstChild(); };
  virtual nsIFrame* GetLastFrame() { return mFrames.LastChild(); };
  virtual void GetNextFrame(nsIFrame*  aFrame, 
                            nsIFrame** aResult) { aFrame->GetNextSibling(aResult); };
  PRBool IsRepeatable() const;
  void   SetRepeatable(PRBool aRepeatable);
  PRBool HasStyleHeight() const;
  void   SetHasStyleHeight(PRBool aValue);
  PRBool NeedSpecialReflow() const;
  void   SetNeedSpecialReflow(PRBool aValue);

#ifdef DEBUG_TABLE_REFLOW_TIMING
public:
  nsReflowTimer* mTimer;
#endif
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

inline PRBool nsTableRowGroupFrame::NeedSpecialReflow() const
{
  return (mState & NS_ROWGROUP_NEED_SPECIAL_REFLOW) == NS_ROWGROUP_NEED_SPECIAL_REFLOW;
}

inline void nsTableRowGroupFrame::SetNeedSpecialReflow(PRBool aValue)
{
  if (aValue) {
    mState |= NS_ROWGROUP_NEED_SPECIAL_REFLOW;
  } else {
    mState &= ~NS_ROWGROUP_NEED_SPECIAL_REFLOW;
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
#endif
