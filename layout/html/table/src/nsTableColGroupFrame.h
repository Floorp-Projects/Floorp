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
#ifndef nsTableColGroupFrame_h__
#define nsTableColGroupFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"
#include "nsTableColFrame.h"
#include "nsTablePainter.h"

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
class nsTableColGroupFrame : public nsHTMLContainerFrame
{
public:

  // default constructor supplied by the compiler

  /** instantiate a new instance of nsTableColGroupFrame.
    * @param aResult    the new object is returned in this out-param
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableColGroupFrame(nsIPresShell* aPresShell, nsIFrame** aResult);

  /** sets defaults for the colgroup.
    * @see nsIFrame::Init
    */
  NS_IMETHOD Init(nsPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);

  /** Initialize the colgroup frame with a set of children.
    * @see nsIFrame::SetInitialChildList
    */
  NS_IMETHOD SetInitialChildList(nsPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

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
    * @param aLastColgroup - the last real colgroup
    * @return  is false if there is a non real colgroup at the end
    */
  static PRBool GetLastRealColGroup(nsTableFrame* aTableFrame, 
                                    nsIFrame**    aLastColGroup);

  /** @see nsIFrame::AppendFrames, InsertFrames, RemoveFrame
    */
  NS_IMETHOD AppendFrames(nsPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  /** remove the column aChild from the column group, if requested renumber
    * the subsequent columns in this column group and all following column
    * groups. see also ResetColIndices for this
    * @param aPresContext - the presentation context
    * @param aChild       - the column frame that needs to be removed
    * @param aResetSubsequentColIndices - if true the columns that follow
    *                                     after aChild will be reenumerated
    */
  void RemoveChild(nsPresContext&  aPresContext,
                   nsTableColFrame& aChild,
                   PRBool           aResetSubsequentColIndices);

  /** @see nsIFrame::Paint
    * all the table painting is done in nsTablePainter.cpp
    */
  NS_IMETHOD Paint(nsPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

  // column groups don't paint their own background -- the cells do
  virtual PRBool CanPaintBackground() { return PR_FALSE; }

  /** @see nsIFrame::GetFrameForPoint
    */
  NS_IMETHOD GetFrameForPoint(nsPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);

  /** reflow of a column group is a trivial matter of reflowing
    * the col group's children (columns), and setting this frame
    * to 0-size.  Since tables are row-centric, column group frames
    * don't play directly in the rendering game.  They do however
    * maintain important state that effects table and cell layout.
    */
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableColGroupFrame
   */
  virtual nsIAtom* GetType() const;

  /** Add column frames to the table storages: colframe cache and cellmap
    * this doesn't change the mFrames of the colgroup frame.
    * @param aPresContext - the presentation context
    * @param aFirstColIndex - the index at which aFirstFrame should be inserted
    *                         into the colframe cache.
    * @param aResetSubsequentColIndices - the indices of the col frames
    *                                     after the insertion might need
    *                                     an update
    * @param aFirstFrame - first frame that needs to be added to the table,
    *                      the frame should have a correctly set sibling
    * @param aLastFrame  - last frame that needs to be added. It can be either
    *                      null or should be in the sibling chain of
    *                      aFirstFrame
    * @result            - if there is no table frame or the table frame is not
    *                      the first in flow it will return an error
    */
  nsresult AddColsToTable(nsPresContext&  aPresContext,
                          PRInt32          aFirstColIndex,
                          PRBool           aResetSubsequentColIndices,
                          nsIFrame*        aFirstFrame,
                          nsIFrame*        aLastFrame = nsnull);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
  void Dump(PRInt32 aIndent);
#endif

  /** returns the number of columns represented by this group.
    * if there are col children, count them (taking into account the span of each)
    * else, check my own span attribute.
    */
  virtual PRInt32 GetColCount() const;

  /** first column on the child list */
  nsTableColFrame * GetFirstColumn();
  /** next sibling to aChildFrame that is a column frame, first column frame
    * in the column group if aChildFrame is null
    */
  nsTableColFrame * GetNextColumn(nsIFrame *aChildFrame);

  /** @return - the position of the first column in this colgroup in the table
    * colframe cache.
    */
  PRInt32 GetStartColumnIndex();
  
  /** set the position of the first column in this colgroup in the table
    * colframe cache.
    */
  void SetStartColumnIndex(PRInt32 aIndex);

  /** helper method to get the span attribute for this colgroup */
  PRInt32 GetSpan();

  /** provide access to the mFrames list
    */
  nsFrameList& GetChildList();

  /** set the column index for all frames starting at aStartColFrame, it
    * will also reset the column indices in all subsequent colgroups
    * @param aFirstColGroup - start the reset operation inside this colgroup
    * @param aFirstColIndex - first column that is reset should get this index
    * @param aStartColFrame - if specified the reset starts with this column
    *                         inside the colgroup; if not specified, the reset
    *                         starts with the first column
    */
  static void ResetColIndices(nsIFrame*       aFirstColGroup,
                              PRInt32         aFirstColIndex,
                              nsIFrame*       aStartColFrame = nsnull);

  /**
   * Gets inner border widths before collapsing with cell borders
   * Caller must get left border from previous column
   * GetContinuousBCBorderWidth will not overwrite aBorder.left
   * see nsTablePainter about continuous borders
   */
  void GetContinuousBCBorderWidth(float     aPixelsToTwips,
                                  nsMargin& aBorder);
  /**
   * Set full border widths before collapsing with cell borders
   * @param aForSide - side to set; only accepts top and bottom
   */
  void SetContinuousBCBorderWidth(PRUint8     aForSide,
                                  BCPixelSize aPixelValue);
protected:
  nsTableColGroupFrame();

  void InsertColsReflow(nsPresContext& aPresContext,
                        nsIPresShell&   aPresShell,
                        PRInt32         aColIndex,
                        nsIFrame*       aFirstFrame,
                        nsIFrame*       aLastFrame = nsnull);

  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  NS_IMETHOD IncrementalReflow(nsPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus);

  NS_IMETHOD IR_TargetIsMe(nsPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus);

  NS_IMETHOD IR_StyleChanged(nsPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus);


  NS_IMETHOD IR_TargetIsChild(nsPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus,
                              nsIFrame *               aNextFrame);

  // data members
  PRInt32 mColCount;
  // the starting column index this col group represents. Must be >= 0. 
  PRInt32 mStartColIndex;

  // border width in pixels
  BCPixelSize mTopContBorderWidth;
  BCPixelSize mBottomContBorderWidth;
};

inline nsTableColGroupFrame::nsTableColGroupFrame()
: mColCount(0), mStartColIndex(0)
{ 
  SetColType(eColGroupContent);
}
  
inline PRInt32 nsTableColGroupFrame::GetStartColumnIndex()
{  
  return mStartColIndex;
}

inline void nsTableColGroupFrame::SetStartColumnIndex (PRInt32 aIndex)
{
  mStartColIndex = aIndex;
}

inline PRInt32 nsTableColGroupFrame::GetColCount() const
{  
  return mColCount;
}

inline nsFrameList& nsTableColGroupFrame::GetChildList()
{  
  return mFrames;
}

#endif

