/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTableColGroupFrame_h__
#define nsTableColGroupFrame_h__

#include "mozilla/Attributes.h"
#include "nscore.h"
#include "nsContainerFrame.h"
#include "nsTableColFrame.h"

class nsTableFrame;
class nsTableColFrame;

enum nsTableColGroupType {
  eColGroupContent            = 0, // there is real col group content associated   
  eColGroupAnonymousCol       = 1, // the result of a col
  eColGroupAnonymousCell      = 2  // the result of a cell alone
};

/**
 * nsTableColGroupFrame
 * data structure to maintain information about a single table cell's frame
 *
 * @author  sclark
 */
class nsTableColGroupFrame : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  // default constructor supplied by the compiler

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsIFrame* NS_NewTableColGroupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  /** Initialize the colgroup frame with a set of children.
    * @see nsIFrame::SetInitialChildList
    */
  virtual nsresult SetInitialChildList(ChildListID     aListID,
                                       nsFrameList&    aChildList) MOZ_OVERRIDE;

  /**
   * ColGroups never paint anything, nor receive events.
   */
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE {}

  /** A colgroup can be caused by three things:
    * 1)	An element with table-column-group display
    * 2)	An element with a table-column display without a
	   *    table-column-group parent
    * 3)	Cells that are not in a column (and hence get an anonymous
	   *    column and colgroup).
    * @return colgroup type
    */
  nsTableColGroupType GetColType() const;

  /** Set the colgroup type based on the creation cause
    * @param aType - the reason why this colgroup is needed
    */
  void SetColType(nsTableColGroupType aType);
  
  /** Real in this context are colgroups that come from an element
    * with table-column-group display or wrap around columns that
    * come from an element with table-column display. Colgroups
    * that are the result of wrapping cells in an anonymous
    * column and colgroup are not considered real here.
    * @param aTableFrame - the table parent of the colgroups
    * @return the last real colgroup
    */
  static nsTableColGroupFrame* GetLastRealColGroup(nsTableFrame* aTableFrame);

  /** @see nsIFrame::DidSetStyleContext */
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) MOZ_OVERRIDE;

  /** @see nsIFrame::AppendFrames, InsertFrames, RemoveFrame
    */
  virtual nsresult AppendFrames(ChildListID     aListID,
                                nsFrameList&    aFrameList) MOZ_OVERRIDE;
  virtual nsresult InsertFrames(ChildListID     aListID,
                                nsIFrame*       aPrevFrame,
                                nsFrameList&    aFrameList) MOZ_OVERRIDE;
  virtual nsresult RemoveFrame(ChildListID     aListID,
                               nsIFrame*       aOldFrame) MOZ_OVERRIDE;

  /** remove the column aChild from the column group, if requested renumber
    * the subsequent columns in this column group and all following column
    * groups. see also ResetColIndices for this
    * @param aChild       - the column frame that needs to be removed
    * @param aResetSubsequentColIndices - if true the columns that follow
    *                                     after aChild will be reenumerated
    */
  void RemoveChild(nsTableColFrame& aChild,
                   bool             aResetSubsequentColIndices);

  /** reflow of a column group is a trivial matter of reflowing
    * the col group's children (columns), and setting this frame
    * to 0-size.  Since tables are row-centric, column group frames
    * don't play directly in the rendering game.  They do however
    * maintain important state that effects table and cell layout.
    */
  virtual nsresult Reflow(nsPresContext*          aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::tableColGroupFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  /** Add column frames to the table storages: colframe cache and cellmap
    * this doesn't change the mFrames of the colgroup frame.
    * @param aFirstColIndex - the index at which aFirstFrame should be inserted
    *                         into the colframe cache.
    * @param aResetSubsequentColIndices - the indices of the col frames
    *                                     after the insertion might need
    *                                     an update
    * @param aCols - an iterator that can be used to iterate over the col
    *                frames to be added.  Once this is done, the frames on the
    *                sbling chain of its .get() at that point will still need
    *                their col indices updated.
    * @result            - if there is no table frame or the table frame is not
    *                      the first in flow it will return an error
    */
  nsresult AddColsToTable(int32_t                   aFirstColIndex,
                          bool                      aResetSubsequentColIndices,
                          const nsFrameList::Slice& aCols);

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
  void Dump(int32_t aIndent);
#endif

  /** returns the number of columns represented by this group.
    * if there are col children, count them (taking into account the span of each)
    * else, check my own span attribute.
    */
  virtual int32_t GetColCount() const;

  /** first column on the child list */
  nsTableColFrame * GetFirstColumn();
  /** next sibling to aChildFrame that is a column frame, first column frame
    * in the column group if aChildFrame is null
    */
  nsTableColFrame * GetNextColumn(nsIFrame *aChildFrame);

  /** @return - the position of the first column in this colgroup in the table
    * colframe cache.
    */
  int32_t GetStartColumnIndex();
  
  /** set the position of the first column in this colgroup in the table
    * colframe cache.
    */
  void SetStartColumnIndex(int32_t aIndex);

  /** helper method to get the span attribute for this colgroup */
  int32_t GetSpan();

  /** provide access to the mFrames list
    */
  nsFrameList& GetWritableChildList();

  /** set the column index for all frames starting at aStartColFrame, it
    * will also reset the column indices in all subsequent colgroups
    * @param aFirstColGroup - start the reset operation inside this colgroup
    * @param aFirstColIndex - first column that is reset should get this index
    * @param aStartColFrame - if specified the reset starts with this column
    *                         inside the colgroup; if not specified, the reset
    *                         starts with the first column
    */
  static void ResetColIndices(nsIFrame*       aFirstColGroup,
                              int32_t         aFirstColIndex,
                              nsIFrame*       aStartColFrame = nullptr);

  /**
   * Gets inner border widths before collapsing with cell borders
   * Caller must get left border from previous column
   * GetContinuousBCBorderWidth will not overwrite aBorder.left
   * see nsTablePainter about continuous borders
   */
  void GetContinuousBCBorderWidth(nsMargin& aBorder);
  /**
   * Set full border widths before collapsing with cell borders
   * @param aForSide - side to set; only accepts top and bottom
   */
  void SetContinuousBCBorderWidth(uint8_t     aForSide,
                                  BCPixelSize aPixelValue);

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsContainerFrame::IsFrameOfType(aFlags & ~(nsIFrame::eTablePart));
  }
  
  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0) MOZ_OVERRIDE;
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0) MOZ_OVERRIDE;
  virtual void InvalidateFrameForRemoval() MOZ_OVERRIDE { InvalidateFrameSubtree(); }

protected:
  nsTableColGroupFrame(nsStyleContext* aContext);

  void InsertColsReflow(int32_t                   aColIndex,
                        const nsFrameList::Slice& aCols);

  virtual int GetLogicalSkipSides(const nsHTMLReflowState* aReflowState = nullptr) const MOZ_OVERRIDE;

  // data members
  int32_t mColCount;
  // the starting column index this col group represents. Must be >= 0. 
  int32_t mStartColIndex;

  // border width in pixels
  BCPixelSize mTopContBorderWidth;
  BCPixelSize mBottomContBorderWidth;
};

inline nsTableColGroupFrame::nsTableColGroupFrame(nsStyleContext *aContext)
: nsContainerFrame(aContext), mColCount(0), mStartColIndex(0)
{ 
  SetColType(eColGroupContent);
}
  
inline int32_t nsTableColGroupFrame::GetStartColumnIndex()
{  
  return mStartColIndex;
}

inline void nsTableColGroupFrame::SetStartColumnIndex (int32_t aIndex)
{
  mStartColIndex = aIndex;
}

inline int32_t nsTableColGroupFrame::GetColCount() const
{  
  return mColCount;
}

inline nsFrameList& nsTableColGroupFrame::GetWritableChildList()
{  
  return mFrames;
}

#endif

