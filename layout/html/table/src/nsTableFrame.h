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
#ifndef nsTableFrame_h__
#define nsTableFrame_h__

#include "nscore.h"
#include "nsVoidArray.h"
#include "nsHTMLContainerFrame.h"
#include "nsStyleCoord.h"
#include "nsStyleConsts.h"
#include "nsIStyleContext.h"
#include "nsITableLayout.h"
#include "nsTableColFrame.h"
#include "nsTableColGroupFrame.h"

class nsTableCellMap;
class nsTableCellFrame;
class nsTableColFrame;
class nsTableRowGroupFrame;
class nsTableRowFrame;
class nsTableColGroupFrame;
class nsTableBorderCollapser;
class nsITableLayoutStrategy;
class nsHTMLValue;

struct nsTableReflowState;
struct nsStylePosition;

#ifdef DEBUG_TABLE_REFLOW_TIMING
#ifdef WIN32
#include <windows.h>
#endif
class nsReflowTimer
{
public:
  nsReflowTimer(nsIFrame* aFrame) {
    mFrame = aFrame;
    mNextSibling = nsnull;
    aFrame->GetFrameType(&mFrameType);
    mReflowType = -1;
		Reset();
	}

  void Destroy() {
    PRInt32 numChildren = mChildren.Count();
    for (PRInt32 childX = 0; childX < numChildren; childX++) {
      ((nsReflowTimer*)mChildren.ElementAt(childX))->Destroy();
    }
    NS_IF_RELEASE(mFrameType);
    if (mNextSibling) { // table frames have 3 auxillary timers
      delete mNextSibling->mNextSibling->mNextSibling;
      delete mNextSibling->mNextSibling;
      delete mNextSibling;
    }
    delete this;
  }

  void Print(PRUint32 aIndent,
             char*    aHeader = 0)  {
		if (aHeader) {
	    printf("%s", aHeader);
		}
    printf(" elapsed=%d numStarts=%d \n", Elapsed(), mNumStarts);	  
  }

  PRUint32 Elapsed() {
    return mTotalTime;
	}

  void Reset() {
		mTotalTime = mNumStarts = 0;
    mStarted = PR_FALSE;
	}

  void Start() {
    NS_ASSERTION(!mStarted, "started timer without stopping");
#ifdef WIN32
    mStartTime = GetTickCount();
#else
    mStartTime = 0;
#endif
    mStarted = PR_TRUE;
    mNumStarts++;
	}

  void Stop() {
    NS_ASSERTION(mStarted, "stopped timer without starting");
		mTotalTime += GetTickCount() - mStartTime;
    mStarted = PR_FALSE;
	}
  PRUint32        mTotalTime;
  PRUint32        mStartTime;
  PRUint32        mNumStarts;
  PRBool          mStarted;
  const nsIFrame* mFrame;
  nsIAtom*        mFrameType; // needed for frame summary timer
  nsReflowReason  mReason;
  nsVoidArray     mChildren;
  PRInt32         mCount;
  // reflow state/reflow metrics data
  nscoord         mAvailWidth;
  nscoord         mComputedWidth;
  nscoord         mComputedHeight;
  nscoord         mMaxElementWidth;
  nscoord         mMaxWidth; // preferred width
  nscoord         mDesiredWidth;
  nscoord         mDesiredHeight;        
  nsReflowStatus  mStatus;
  nsReflowTimer*  mNextSibling;
  PRInt32         mReflowType;

private:
  ~nsReflowTimer() {}

};

#endif

/**
 * Child list name indices
 * @see #GetAdditionalChildListName()
 */
#define NS_TABLE_FRAME_COLGROUP_LIST_INDEX 0
#define NS_TABLE_FRAME_LAST_LIST_INDEX    NS_TABLE_FRAME_COLGROUP_LIST_INDEX

/* ============================================================================ */

/** nsTableFrame maps the inner portion of a table (everything except captions.)
  * Used as a pseudo-frame within nsTableOuterFrame, it may also be used
  * stand-alone as the top-level frame.
  *
  * The flowed child list contains row group frames. There is also an additional
  * named child list:
  * - "ColGroup-list" which contains the col group frames
  *
  * @see nsLayoutAtoms::colGroupList
  *
  * TODO: make methods virtual so nsTableFrame can be used as a base class in the future.
  */
class nsTableFrame : public nsHTMLContainerFrame, public nsITableLayout
{
public:

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  /** nsTableOuterFrame has intimate knowledge of the inner table frame */
  friend class nsTableOuterFrame;

  /** instantiate a new instance of nsTableFrame.
    * @param aResult    the new object is returned in this out-param
    * @param aContent   the table object to map
    * @param aParent    the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableFrame(nsIPresShell* aPresShell, nsIFrame** aResult);

  /** sets defaults for table-specific style.
    * @see nsIFrame::Init 
    */
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);


  static nscoord RoundToPixel(nscoord aValue,
                              float   aPixelToTwips,
                              PRBool  aRoundUp = PR_FALSE);

  NS_IMETHOD IsPercentageBase(PRBool& aBase) const;

  static nsresult AppendDirtyReflowCommand(nsIPresShell* aPresShell,
                                           nsIFrame*     aFrame);
  static nsIPresShell* GetPresShellNoAddref(nsIPresContext* aPresContext);

  static void RePositionViews(nsIPresContext* aPresContext,
                              nsIFrame*       aFrame);

  nsPoint GetFirstSectionOrigin(const nsHTMLReflowState& aReflowState) const;
  /*
   * Notification that aAttribute has changed for content inside a table (cell, row, etc)
   */
  void AttributeChangedFor(nsIPresContext* aPresContext, 
                           nsIFrame*       aFrame,
                           nsIContent*     aContent, 
                           nsIAtom*        aAttribute); 

  /** @see nsIFrame::Destroy */
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

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

  nsMargin GetBorderPadding(const nsHTMLReflowState& aReflowState) const;

  /** helper method to find the table parent of any table frame object */
  // TODO: today, this depends on display types.  This should be changed to rely
  // on stronger criteria, like an inner table frame atom
  static NS_METHOD GetTableFrame(nsIFrame*      aSourceFrame, 
                                 nsTableFrame*& aTableFrame);

  // Return the closest sibling of aPriorChildFrame (including aPriroChildFrame)
  // of type aChildType.
  static nsIFrame* GetFrameAtOrBefore(nsIPresContext* aPresContext,
                                      nsIFrame*       aParentFrame,
                                      nsIFrame*       aPriorChildFrame,
                                      nsIAtom*        aChildType);
  PRBool IsAutoWidth(PRBool* aIsPctWidth = nsnull);
  PRBool IsAutoHeight();
  
  /** @return PR_TRUE if aDisplayType represents a rowgroup of any sort
    * (header, footer, or body)
    */
  PRBool IsRowGroup(PRInt32 aDisplayType) const;

  /** Initialize the table frame with a set of children.
    * @see nsIFrame::SetInitialChildList 
    */
  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  /** return the first child belonging to the list aListName. 
    * @see nsIFrame::FirstChild
    */
  NS_IMETHOD FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const;

  /** @see nsIFrame::GetAdditionalChildListName */
  NS_IMETHOD  GetAdditionalChildListName(PRInt32   aIndex,
                                         nsIAtom** aListName) const;

  /** @see nsIFrame::Paint */
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);

  /** nsIFrame method overridden to handle table specifics
  */
  NS_IMETHOD SetSelected(nsIPresContext* aPresContext,
                         nsIDOMRange *aRange,
                         PRBool aSelected,
                         nsSpread aSpread);

  /** inner tables are reflowed in two steps.
    * <pre>
    * if mFirstPassValid is false, this is our first time through since content was last changed
    *   set pass to 1
    *   do pass 1
    *     get min/max info for all cells in an infinite space
    *   do column balancing
    *   set mFirstPassValid to true
    *   do pass 2
    *     use column widths to Reflow cells
    * </pre>
    *
    * @see ResizeReflowPass1
    * @see ResizeReflowPass2
    * @see BalanceColumnWidths
    * @see nsIFrameReflow::Reflow
    */
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  nsresult ReflowTable(nsIPresContext*          aPresContext,
                       nsHTMLReflowMetrics&     aDesiredSize,
                       const nsHTMLReflowState& aReflowState,
                       nscoord                  aAvailHeight,
                       nsReflowReason           aReason,
                       PRBool&                  aDoCollapse,
                       PRBool&                  aDidBalance,
                       nsReflowStatus&          aStatus);

  NS_IMETHOD GetParentStyleContextProvider(nsIPresContext* aPresContext,
                                           nsIFrame** aProviderFrame, 
                                           nsContextProviderRelationship& aRelationship);

  static nsMargin GetPadding(const nsHTMLReflowState& aReflowState,
                             const nsTableCellFrame*  aCellFrame);

  static nsMargin GetPadding(const nsSize&           aBasis,
                             const nsTableCellFrame* aCellFrame);

  nsFrameList& GetColGroups();

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableFrame
   */
  NS_IMETHOD  GetFrameType(nsIAtom** aType) const;

#ifdef DEBUG
  /** @see nsIFrame::GetFrameName */
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

  /** return the width of the column at aColIndex    */
  virtual PRInt32 GetColumnWidth(PRInt32 aColIndex);

  /** set the width of the column at aColIndex to aWidth    */
  virtual void SetColumnWidth(PRInt32 aColIndex, nscoord aWidth);

  /** helper to get the border collapse style value */
  virtual PRUint8 GetBorderCollapseStyle();

  /** helper to get the cell spacing X style value */
  virtual nscoord GetCellSpacingX();

  /** helper to get the cell spacing Y style value */
  virtual nscoord GetCellSpacingY();
        
  /** return the row span of a cell, taking into account row span magic at the bottom
    * of a table. The row span equals the number of rows spanned by aCell starting at
    * aStartRowIndex, and can be smaller if aStartRowIndex is greater than the row
    * index in which aCell originates.
    *
    * @param aStartRowIndex the cell
    * @param aCell          the cell
    *
    * @return  the row span, correcting for row spans that extend beyond the bottom
    *          of the table.
    */
  virtual PRInt32  GetEffectiveRowSpan(PRInt32                 aStartRowIndex,
                                       const nsTableCellFrame& aCell) const;
  virtual PRInt32  GetEffectiveRowSpan(const nsTableCellFrame& aCell) const;

  /** return the col span of a cell, taking into account col span magic at the edge
    * of a table.
    *
    * @param aCell      the cell
    *
    * @return  the col span, correcting for col spans that extend beyond the edge
    *          of the table.
    */
  virtual PRInt32  GetEffectiveColSpan(const nsTableCellFrame& aCell) const;

  /** return the value of the COLS attribute, adjusted for the 
    * actual number of columns in the table
    */
  PRInt32 GetEffectiveCOLSAttribute();

  /** return the column frame associated with aColIndex */
  nsTableColFrame* GetColFrame(PRInt32 aColIndex) const;

  void InsertCol(nsIPresContext&  aPresContext,
                 nsTableColFrame& aColFrame,
                 PRInt32          aColIndex);

  nsTableColGroupFrame* CreateAnonymousColGroupFrame(nsIPresContext&     aPresContext,
                                                     nsTableColGroupType aType);

  PRInt32 DestroyAnonymousColFrames(nsIPresContext& aPresContext,
                                    PRInt32 aNumFrames);

  void CreateAnonymousColFrames(nsIPresContext& aPresContext,
                                PRInt32         aNumColsToAdd,
                                nsTableColType  aColType,
                                PRBool          aDoAppend,
                                nsIFrame*       aPrevCol = nsnull);

  void CreateAnonymousColFrames(nsIPresContext&       aPresContext,
                                nsTableColGroupFrame& aColGroupFrame,
                                PRInt32               aNumColsToAdd,
                                nsTableColType        aColType,
                                PRBool                aAddToColGroupAndTable,
                                nsIFrame*             aPrevCol,
                                nsIFrame**            aFirstNewFrame);

  /** empty the column frame cache */
  void ClearColCache();

  virtual PRInt32 AppendCell(nsIPresContext&   aPresContext,
                             nsTableCellFrame& aCellFrame,
                             PRInt32           aRowIndex);

  virtual void InsertCells(nsIPresContext& aPresContext,
                           nsVoidArray&    aCellFrames, 
                           PRInt32         aRowIndex, 
                           PRInt32         aColIndexBefore);

  virtual void RemoveCell(nsIPresContext&   aPresContext,
                          nsTableCellFrame* aCellFrame,
                          PRInt32           aRowIndex);

  void AppendRows(nsIPresContext&       aPresContext,
                  nsTableRowGroupFrame& aRowGroupFrame,
                  PRInt32               aRowIndex,
                  nsVoidArray&          aRowFrames);

  PRInt32 InsertRow(nsIPresContext&       aPresContext,
                    nsTableRowGroupFrame& aRowGroupFrame,
                    nsIFrame&             aFrame,
                    PRInt32               aRowIndex,
                    PRBool                aConsiderSpans);

  PRInt32 InsertRows(nsIPresContext&       aPresContext,
                     nsTableRowGroupFrame& aRowGroupFrame,
                     nsVoidArray&          aFrames,
                     PRInt32               aRowIndex,
                     PRBool                aConsiderSpans);

  virtual void RemoveRows(nsIPresContext&  aPresContext,
                          nsTableRowFrame& aFirstRowFrame,
                          PRInt32          aNumRowsToRemove,
                          PRBool           aConsiderSpans);

  void AppendRowGroups(nsIPresContext& aPresContext,
                       nsIFrame*       aFirstRowGroupFrame);

  void InsertRowGroups(nsIPresContext& aPresContext,
                       nsIFrame*       aFirstRowGroupFrame,
                       nsIFrame*       aLastRowGroupFrame);

  void InsertColGroups(nsIPresContext& aPresContext,
                       PRInt32         aColIndex,
                       nsIFrame*       aFirstFrame,
                       nsIFrame*       aLastFrame = nsnull);

  virtual void RemoveCol(nsIPresContext&       aPresContext,
                         nsTableColGroupFrame* aColGroupFrame,
                         PRInt32               aColIndex,
                         PRBool                aRemoveFromCache,
                         PRBool                aRemoveFromCellMap);

  nsTableCellFrame* GetCellInfoAt(PRInt32            aRowX, 
                                  PRInt32            aColX, 
                                  PRBool*            aOriginates = nsnull, 
                                  PRInt32*           aColSpan = nsnull);

  PRInt32 GetNumCellsOriginatingInCol(PRInt32 aColIndex) const;

  PRBool HasPctCol() const;
  void SetHasPctCol(PRBool aValue);

  PRBool HasCellSpanningPctCol() const;
  void SetHasCellSpanningPctCol(PRBool aValue);
  // is this the 3rd reflow due to a height on a table in pagination mode.
  PRBool IsThirdPassReflow() const;

protected:

  /** protected constructor. 
    * @see NewFrame
    */
  nsTableFrame();

  /** destructor, responsible for mColumnLayoutData */
  virtual ~nsTableFrame();

  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  virtual PRBool ParentDisablesSelection() const; //override default behavior

  // Sets the starting column index for aColGroupFrame and the siblings frames that
  // follow
  void SetStartingColumnIndexFor(nsTableColGroupFrame* aColGroupFrame,
                                 PRInt32 aIndex);

  // Calculate the starting column index to use for the specified col group frame
  PRInt32 CalculateStartingColumnIndexFor(nsTableColGroupFrame* aColGroupFrame);

  PRBool DescendantReflowedNotTimeout() const;
  void   SetDescendantReflowedNotTimeout(PRBool aValue);
  PRBool RequestedTimeoutReflow() const;
  void   SetRequestedTimeoutReflow(PRBool aValue);
  void   SetThirdPassReflow(PRBool aValue);

  void   InterruptNotification(nsIPresContext* aPresContext,
                               PRBool          aIsRequest);

public:
  /** first pass of ResizeReflow.  
    * lays out all table content with aMaxSize(NS_UNCONSTRAINEDSIZE,NS_UNCONSTRAINEDSIZE) and
    * a non-null aMaxElementSize so we get all the metrics we need to do column balancing.
    * Pass 1 only needs to be executed once no matter how many times the table is resized, 
    * as long as content and style don't change.  This is managed in the member variable mFirstPassIsValid.
    * The layout information for each cell is cached in mColumLayoutData.
    * Incremental layout can take advantage of aStartingFrame to pick up where a previous
    * ResizeReflowPass1 left off.
    *
    * @see nsIFrameReflow::Reflow
    */

  /** do I need to do a reflow? */
  virtual PRBool NeedsReflow(const nsHTMLReflowState& aReflowState);

  // increment or decrement the count of pending reflow commands targeted at
  // descendants. Only rebalance the table when this count goes to 0 or the
  // reflow is the last one in a batch (limited by the pres shell).
  NS_IMETHOD  ReflowCommandNotify(nsIPresShell*     aShell,
                                  nsIReflowCommand* aRC,
                                  PRBool            aCommandAdded);
  PRBool IsRowInserted() const;
  void   SetRowInserted(PRBool aValue);

protected:

  NS_METHOD ReflowChildren(nsIPresContext*      aPresContext,
                           nsTableReflowState&  aReflowState,
                           PRBool               aDoColGroups,
                           PRBool               aDirtyOnly,
                           nsReflowStatus&      aStatus,
                           PRBool*              aReflowedAtLeastOne = nsnull);
// begin incremental reflow methods
  
  /** Incremental Reflow attempts to do column balancing with the minimum number of reflow
    * commands to child elements.  This is done by processing the reflow command,
    * rebalancing column widths (if necessary), then comparing the resulting column widths
    * to the prior column widths and reflowing only those cells that require a reflow.
    * All incremental reflows go through this method.
    *
    * @see Reflow
    */
  NS_IMETHOD IncrementalReflow(nsIPresContext*          aPresContext,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus);

  /** process an incremental reflow command targeted at a child of this frame. 
    * @param aNextFrame  the next frame in the reflow target chain
    * @see nsIFrameReflow::Reflow
    */
  NS_IMETHOD IR_TargetIsChild(nsIPresContext*       aPresContext,
                              nsTableReflowState&   aReflowStatet,
                              nsReflowStatus&       aStatus,
                              nsIFrame*             aNextFrame);

  /** process an incremental reflow command targeted at this frame. 
    * @see nsIFrameReflow::Reflow
    */
  NS_IMETHOD IR_TargetIsMe(nsIPresContext*      aPresContext,
                           nsTableReflowState&  aReflowState,
                           nsReflowStatus&      aStatus);

  /** process a style chnaged notification.
    * @see nsIFrameReflow::Reflow
    * TODO: needs to be optimized for which attribute was actually changed.
    */
  NS_IMETHOD IR_StyleChanged(nsIPresContext*      aPresContext,
                             nsTableReflowState&  aReflowState,
                             nsReflowStatus&      aStatus);
  
  NS_IMETHOD AdjustSiblingsAfterReflow(nsIPresContext*     aPresContext,
                                       nsTableReflowState& aReflowState,
                                       nsIFrame*           aKidFrame,
                                       nscoord             aDeltaY);
  
  nsresult RecoverState(nsTableReflowState& aReflowState,
                        nsIFrame*           aKidFrame);

  NS_METHOD CollapseRowGroupIfNecessary(nsIPresContext* aPresContext,
                                        nsIFrame* aRowGroupFrame,
                                        const nscoord& aYTotalOffset,
                                        nscoord& aYGroupOffset, PRInt32& aRowX);

  NS_METHOD AdjustForCollapsingRows(nsIPresContext* aPresContext, 
                                    nscoord&        aHeight);

  NS_METHOD AdjustForCollapsingCols(nsIPresContext* aPresContext, 
                                    nscoord&        aWidth);
  // end incremental reflow methods


  // WIDTH AND HEIGHT CALCULATION

public:

  // calculate the computed width of aFrame including its border and padding given 
  // its reflow state.
  nscoord CalcBorderBoxWidth(const nsHTMLReflowState& aReflowState);

  // calculate the computed height of aFrame including its border and padding given 
  // its reflow state.
  nscoord CalcBorderBoxHeight(const nsHTMLReflowState& aReflowState);
  // calculate the minimum width to layout aFrame and its desired width 
  // including border and padding given its reflow state and column width information 
  void CalcMinAndPreferredWidths(nsIPresContext*          aPresContextconst,
                                 const nsHTMLReflowState& aReflowState,
                                 PRBool                   aCalcPrefWidthIfAutoWithPctCol,
                                 nscoord&                 aMinWidth,
                                 nscoord&                 aPreferredWidth);
protected:

  // calcs the width of the table according to the computed widths of each column.
  virtual PRInt32 CalcDesiredWidth(const nsHTMLReflowState& aReflowState);

  // return the desired height of this table accounting for the current
  // reflow state, and for the table attributes and parent 
  nscoord CalcDesiredHeight(nsIPresContext*          aPresContext,
                            const nsHTMLReflowState& aReflowState);
  // The following two functions are helpers for CalcDesiredHeight 
 
  void DistributeSpaceToCells(nsIPresContext*          aPresContext, 
                              const nsHTMLReflowState& aReflowState,
                              nsIFrame*                aRowGroupFrame);
  void DistributeSpaceToRows(nsIPresContext*          aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsIFrame*                aRowGroupFrame, 
                             nscoord                  aSumOfRowHeights,
                             nscoord                  aExcess,
                             nscoord&                 aExcessAllocated,
                             nscoord&                 aRowGroupYPos);

  void PlaceChild(nsIPresContext*      aPresContext,
                  nsTableReflowState&  aReflowState,
                  nsIFrame*            aKidFrame,
                  nsHTMLReflowMetrics& aDesiredSize);

  /** assign widths for each column, taking into account the table content, the effective style, 
    * the layout constraints, and the compatibility mode.  
    * @param aPresContext     the presentation context
    * @param aTableStyle      the resolved style for the table
    * @param aMaxSize         the height and width constraints
    * @param aMaxElementSize  the min size of the largest indivisible object
    */
  virtual void BalanceColumnWidths(nsIPresContext*          aPresContext, 
                                   const nsHTMLReflowState& aReflowState);


  nsIFrame* GetFirstBodyRowGroupFrame();
  PRBool MoveOverflowToChildList(nsIPresContext* aPresContext);
  void PushChildren(nsIPresContext *aPresContext,
                    nsIFrame*       aFromChild,
                    nsIFrame*       aPrevSibling);

public:
  // put the children frames in the display order (e.g. thead before tbody before tfoot)
  // and put the non row group frames at the end. Also return the number of row group frames.
  void OrderRowGroups(nsVoidArray&           aChildren,
                      PRUint32&              aNumRowGroups,
                      nsIFrame**             aFirstBody = nsnull,
                      nsTableRowGroupFrame** aHead      = nsnull,
                      nsTableRowGroupFrame** aFoot      = nsnull);

  // Returns PR_TRUE if there are any cells above the row at
  // aRowIndex and spanning into the row at aRowIndex     
  PRBool RowIsSpannedInto(PRInt32 aRowIndex);

  // Returns PR_TRUE if there is a cell originating in aRowIndex
  // which spans into the next row
  PRBool RowHasSpanningCells(PRInt32 aRowIndex);

  // Returns PR_TRUE if there are any cells to the left of the column at
  // aColIndex and spanning into the column at aColIndex     
  PRBool ColIsSpannedInto(PRInt32 aColIndex);

  // Returns PR_TRUE if there is a cell originating in aColIndex
  // which spans into the next col
  PRBool ColHasSpanningCells(PRInt32 aColIndex);

  // Allows rows to notify the table of additions or changes to a cell's width
  // The table uses this to update the appropriate column widths and to decide 
  // whether to reinitialize (and then rebalance) or rebalance the table. If the 
  // most extreme measure results (e.g. reinitialize) then PR_TRUE is returned 
  // indicating that further calls are not going to accomplish anyting.
  PRBool CellChangedWidth(const nsTableCellFrame& aCellFrame,
                          nscoord                 aPrevMinWidth,
                          nscoord                 aPrevMaxWidth,
                          PRBool                  aCellWasDestroyed = PR_FALSE);
 
protected:

  PRBool HaveReflowedColGroups() const;
  void   SetHaveReflowedColGroups(PRBool aValue);

  PRBool DidResizeReflow() const;
  void   SetResizeReflow(PRBool aValue);

  /** Support methods for DidSetStyleContext */
  void      MapBorderMarginPadding(nsIPresContext* aPresContext);
  void      MapHTMLBorderStyle(nsStyleBorder& aBorderStyle, nscoord aBorderWidth);
  PRBool    ConvertToPixelValue(nsHTMLValue& aValue, 
                                PRInt32      aDefault, 
                                PRInt32&     aResult);

public:
  PRBool NeedStrategyInit() const;
  void SetNeedStrategyInit(PRBool aValue);

  PRBool NeedStrategyBalance() const;
  void SetNeedStrategyBalance(PRBool aValue);

  /** Get the cell map for this table frame.  It is not always mCellMap.
    * Only the firstInFlow has a legit cell map
    */
  virtual nsTableCellMap* GetCellMap() const;
    
  void AdjustRowIndices(nsIPresContext* aPresContext,
                        PRInt32         aRowIndex,
                        PRInt32         aAdjustment);

  NS_IMETHOD AdjustRowIndices(nsIPresContext* aPresContext,
                              nsIFrame*       aRowGroup, 
                              PRInt32         aRowIndex,
                              PRInt32         anAdjustment);

  // Return PR_TRUE if rules=groups is set for the table content 
  PRBool HasGroupRules() const;

  // Remove cell borders which aren't bordering row and/or col groups 
  void ProcessGroupRules(nsIPresContext* aPresContext);

  nsVoidArray& GetColCache();

	/** 
	  * Return aFrame's child if aFrame is an nsScrollFrame, otherwise return aFrame
	  */
  nsTableRowGroupFrame* GetRowGroupFrame(nsIFrame* aFrame,
                                         nsIAtom*  aFrameTypeIn = nsnull);

protected:

  PRBool HadInitialReflow() const;
  void SetHadInitialReflow(PRBool aValue);

  void SetColumnDimensions(nsIPresContext* aPresContext, 
                           nscoord         aHeight,
                           const nsMargin& aReflowState);

  /** return the number of columns as specified by the input. 
    * has 2 side effects:<br>
    * calls SetStartColumnIndex on each nsTableColumn<br>
    * sets mSpecifiedColCount.<br>
    */
  virtual PRInt32 GetSpecifiedColumnCount ();

  PRInt32 CollectRows(nsIPresContext* aPresContext,
                      nsIFrame*       aFrame,
                      nsVoidArray&    aCollection);

public: /* ----- Cell Map public methods ----- */

  PRInt32 GetStartRowIndex(nsTableRowGroupFrame& aRowGroupFrame);

  /** returns the number of rows in this table.
    * if mCellMap has been created, it is asked for the number of rows.<br>
    * otherwise, the content is enumerated and the rows are counted.
    */
  virtual PRInt32 GetRowCount() const;

  /** returns the number of columns in this table after redundant columns have been removed 
    */
  virtual PRInt32 GetEffectiveColCount();
  virtual PRInt32 GetColCount();

  /** return the column frame at colIndex.
    * returns nsnull if the col frame has not yet been allocated, or if aColIndex is out of range
    */
  nsTableColFrame * GetColFrame(PRInt32 aColIndex);
  // return the last col index which isn't of type eColAnonymousCell
  PRInt32 GetIndexOfLastRealCol();

  /** return the cell frame at aRowIndex, aColIndex.
    * returns nsnull if the cell frame has not yet been allocated, 
    * or if aRowIndex or aColIndex is out of range
    */
  nsTableCellFrame * GetCellFrameAt(PRInt32 aRowIndex, PRInt32 aColIndex);

  /** return the minimum width of the table caption.  Return 0 if there is no caption. */
  nscoord GetMinCaptionWidth();

  /** returns PR_TRUE if table-layout:auto  */
  virtual PRBool IsAutoLayout();

  // compute the height of the table to be used as the basis for 
  // percentage height cells
  void ComputePercentBasisForRows(const nsHTMLReflowState& aReflowState);

  nscoord GetPercentBasisForRows();

  nscoord GetMinWidth() const;
  void    SetMinWidth(nscoord aWidth);
  
  nscoord GetDesiredWidth() const;
  void    SetDesiredWidth(nscoord aWidth);

  nscoord GetPreferredWidth() const;
  void    SetPreferredWidth(nscoord aWidth); 
  
  /*---------------- nsITableLayout methods ------------------------*/
  
  /** Get the cell and associated data for a table cell from the frame's cellmap */
  NS_IMETHOD GetCellDataAt(PRInt32 aRowIndex, PRInt32 aColIndex, 
                           nsIDOMElement* &aCell,   //out params
                           PRInt32& aStartRowIndex, PRInt32& aStartColIndex, 
                           PRInt32& aRowSpan, PRInt32& aColSpan,
                           PRInt32& aActualRowSpan, PRInt32& aActualColSpan,
                           PRBool& aIsSelected);

  /** Get the number of rows and column for a table from the frame's cellmap 
    *  Some rows may not have enough cells (the number returned is the maximum possible),
    *  which displays as a ragged-right edge table
    */
  NS_IMETHOD GetTableSize(PRInt32& aRowCount, PRInt32& aColCount);

  /*------------end of nsITableLayout methods -----------------------*/

public:
  static nsIAtom* gColGroupAtom;
  void Dump(nsIPresContext* aPresContext,
            PRBool          aDumpRows,
            PRBool          aDumpCols, 
            PRBool          aDumpCellMap);

  nsAutoVoidArray mColFrames; // XXX temporarily public 

#ifdef DEBUG
  static void DumpTableFrames(nsIPresContext* aPresContext,
                              nsIFrame*       aFrame);
#endif

protected:
  void DumpRowGroup(nsIPresContext* aPresContext, nsIFrame* aChildFrame);
  void DebugPrintCount() const; // Debugging routine

  // DATA MEMBERS

  struct TableBits {
    unsigned mHadInitialReflow:1;      // has intial reflow happened
    unsigned mHaveReflowedColGroups:1; // have the col groups gotten their initial reflow
    unsigned mNeedStrategyBalance:1;   // does the strategy needs to balance the table
    unsigned mNeedStrategyInit:1;      // does the strategy needs to be initialized and then balance the table
    unsigned mHasPctCol:1;             // does any cell or col have a pct width
    unsigned mCellSpansPctCol:1;       // does any cell span a col with a pct width (or containing a cell with a pct width)
    unsigned mDidResizeReflow:1;       // did a resize reflow happen (indicating pass 2)
    // true if a descendant was reflowed normally since the last time we reflowed.
    // We will likely need a timeout reflow (targeted either at us or below)
    unsigned mDescendantReflowedNotTimeout:1;      
    // true if we requested a timeout reflow targeted at us. This will become false if
    // we know that a descendant will be getting a timeout reflow and we cancel the one
    // targeted at us, as an optimization.
    unsigned mRequestedTimeoutReflow:1;
    unsigned mRowInserted:1;
    unsigned mThirdPassReflow:1;
    int : 21;                          // unused
  } mBits;

  nsTableCellMap*         mCellMap;            // maintains the relationships between rows, cols, and cells
  nsITableLayoutStrategy* mTableLayoutStrategy;// the layout strategy for this frame
  nsFrameList             mColGroups;          // the list of colgroup frames
  nscoord                 mPercentBasisForRows;
  nscoord                 mMinWidth;       // XXX could store as PRUint16 with pixels
  nscoord                 mDesiredWidth;   // XXX could store as PRUint16 with pixels
  nscoord                 mPreferredWidth; // XXX could store as PRUint16 with pixels
  // the number of normal incremental reflow commands targeted below this table
  PRInt16   mNumDescendantReflowsPending;
  // the number of timeout incremental reflow commands targeted below this table
  PRInt16   mNumDescendantTimeoutReflowsPending;


  // DEBUG REFLOW 
#if defined DEBUG_TABLE_REFLOW | DEBUG_TABLE_REFLOW_TIMING
public:
  static void DebugReflow(nsIFrame*            aFrame, 
                          nsHTMLReflowState&   aReflowState, 
                          nsHTMLReflowMetrics* aMetrics = nsnull,
                          nsReflowStatus       aStatus  = NS_FRAME_COMPLETE);

#ifdef DEBUG_TABLE_REFLOW_TIMING
  static void DebugReflowDone(nsIFrame* aFrame);

  enum nsMethod {eInit=0, eBalanceCols, eNonPctCols, eNonPctColspans, ePctCols};
  static void DebugTimeMethod(nsMethod           aMethod,
                              nsTableFrame&      aFrame,
                              nsHTMLReflowState& aReflowState,
                              PRBool             aStart);
  nsReflowTimer* mTimer;
#endif
#endif
};


inline PRBool nsTableFrame::IsRowGroup(PRInt32 aDisplayType) const
{
  return PRBool((NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == aDisplayType) ||
                (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == aDisplayType) ||
                (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == aDisplayType));
}

inline nscoord nsTableFrame::GetPercentBasisForRows()
{
  return mPercentBasisForRows;
}

inline void nsTableFrame::SetHadInitialReflow(PRBool aValue)
{
  mBits.mHadInitialReflow = aValue;
}

inline PRBool nsTableFrame::HadInitialReflow() const
{
  return (PRBool)mBits.mHadInitialReflow;
}

inline void nsTableFrame::SetHaveReflowedColGroups(PRBool aValue)
{
  mBits.mHaveReflowedColGroups = aValue;
}

inline PRBool nsTableFrame::HaveReflowedColGroups() const
{
  return (PRBool)mBits.mHaveReflowedColGroups;
}

inline PRBool nsTableFrame::HasPctCol() const
{
  return (PRBool)mBits.mHasPctCol;
}

inline void nsTableFrame::SetHasPctCol(PRBool aValue)
{
  mBits.mHasPctCol = (unsigned)aValue;
}

inline PRBool nsTableFrame::HasCellSpanningPctCol() const
{
  return (PRBool)mBits.mCellSpansPctCol;
}

inline void nsTableFrame::SetHasCellSpanningPctCol(PRBool aValue)
{
  mBits.mCellSpansPctCol = (unsigned)aValue;
}

inline PRBool nsTableFrame::DescendantReflowedNotTimeout() const
{
  return (PRBool)mBits.mDescendantReflowedNotTimeout;
}

inline void nsTableFrame::SetDescendantReflowedNotTimeout(PRBool aValue)
{
  mBits.mDescendantReflowedNotTimeout = (unsigned)aValue;
}

inline PRBool nsTableFrame::RequestedTimeoutReflow() const
{
  return (PRBool)mBits.mRequestedTimeoutReflow;
}

inline void nsTableFrame::SetRequestedTimeoutReflow(PRBool aValue)
{
  mBits.mRequestedTimeoutReflow = (unsigned)aValue;
}

inline PRBool nsTableFrame::IsThirdPassReflow() const
{
  return (PRBool)mBits.mThirdPassReflow;
}

inline void nsTableFrame::SetThirdPassReflow(PRBool aValue)
{
  mBits.mThirdPassReflow = (unsigned)aValue;
}

inline PRBool nsTableFrame::IsRowInserted() const
{
  return (PRBool)mBits.mRowInserted;
}

inline void nsTableFrame::SetRowInserted(PRBool aValue)
{
  mBits.mRowInserted = (unsigned)aValue;
}

inline nsFrameList& nsTableFrame::GetColGroups()
{
  return mColGroups;
}

inline nsVoidArray& nsTableFrame::GetColCache()
{
  return mColFrames;
}

inline void nsTableFrame::SetMinWidth(nscoord aWidth)
{
  mMinWidth = aWidth;
}

inline void nsTableFrame::SetDesiredWidth(nscoord aWidth)
{
  mDesiredWidth = aWidth;
}

inline void nsTableFrame::SetPreferredWidth(nscoord aWidth)
{
  mPreferredWidth = aWidth;
}


                                                            
enum nsTableIteration {
  eTableLTR = 0,
  eTableRTL = 1,
  eTableDIR = 2
};

class nsTableIterator
{
public:
  nsTableIterator(nsIPresContext*  aPresContext,
                  nsIFrame&        aSource, 
                  nsTableIteration aType);
  nsTableIterator(nsFrameList&     aSource, 
                  nsTableIteration aType);
  nsIFrame* First();
  nsIFrame* Next();
  PRBool    IsLeftToRight();
  PRInt32   Count();

protected:
  void Init(nsIFrame*        aFirstChild,
            nsTableIteration aType);
  PRBool    mLeftToRight;
  nsIFrame* mFirstListChild;
  nsIFrame* mFirstChild;
  nsIFrame* mCurrentChild;
  PRInt32   mCount;
};

#endif































