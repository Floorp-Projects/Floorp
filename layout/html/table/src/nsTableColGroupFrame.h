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
#ifndef nsTableColGroupFrame_h__
#define nsTableColGroupFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"
#include "nsTableColFrame.h"

class nsTableColFrame;
class nsTableFrame;


enum nsTableColGroupType {
  eColGroupContent            = 0, // there is real col group content associated   
  eColGroupAnonymousCol       = 1, // the result of a col
  eColGroupAnonymousCell      = 2 // the result of a cell alone
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

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  nsTableColGroupType GetType() const;

  void SetType(nsTableColGroupType aType);

  static PRBool GetLastRealColGroup(nsTableFrame* aTableFrame, 
                                    nsIFrame**    aLastColGroup);

  static nsTableColGroupFrame* FindParentForAppendedCol(nsTableFrame*  aTableFrame, 
                                                        nsTableColType aColType);

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

  void RemoveChild(nsIPresContext&  aPresContext, 
                   nsTableColFrame& aLastChild,
                   PRBool           aResetColIndices);  
  
  void RemoveChildrenAtEnd(nsIPresContext& aPresContext,
                           PRInt32         aNumChildrenToRemove);

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);

  /** reflow of a column group is a trivial matter of reflowing
    * the col group's children (columns), and setting this frame
    * to 0-size.  Since tables are row-centric, column group frames
    * don't play directly in the rendering game.  They do however
    * maintain important state that effects table and cell layout.
    */
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableColGroupFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  
  NS_IMETHOD AddColsToTable(nsIPresContext&  aPresContext,
                            PRInt32          aFirstColIndex,
                            PRBool           aResetSubsequentColIndices,
                            nsIFrame*        aFirstFrame,
                            nsIFrame*        aLastFrame = nsnull);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

  /** returns the number of columns represented by this group.
    * if there are col children, count them (taking into account the span of each)
    * else, check my own span attribute.
    */
  virtual PRInt32 GetColCount() const;

  virtual nsTableColFrame * GetFirstColumn();

  virtual nsTableColFrame * GetNextColumn(nsIFrame *aChildFrame);

  virtual nsTableColFrame * GetColumnAt(PRInt32 aColIndex);

  virtual PRInt32 GetStartColumnIndex();
  
  /** sets mStartColIndex to aIndex.
    * @return the col count
    * has the side effect of setting all child COL indexes
    */
  virtual PRInt32 SetStartColumnIndex(PRInt32 aIndex);

  /** helper method to get the span attribute for this colgroup */
  PRInt32 GetSpan();

  /** helper method returns PR_TRUE if this colgroup exists without any
    * colgroup or col content in the table backing it.
    */
  //PRBool IsManufactured();

  void DeleteColFrame(nsIPresContext* aPresContext, nsTableColFrame* aColFrame);

  static nsTableColGroupFrame* GetColGroupFrameContaining(nsIPresContext*  aPresContext,
                                                          nsFrameList&     aColGroupList,
                                                          nsTableColFrame& aColFrame);
  nsFrameList& GetChildList();

  static void ResetColIndices(nsIPresContext* aPresContext,
                              nsIFrame*       aFirstColGroup,
                              PRInt32         aFirstColIndex,
                              nsIFrame*       aStartColFrame = nsnull);
protected:
  nsTableColGroupFrame();

  void InsertColsReflow(nsIPresContext& aPresContext,
                        nsIPresShell&   aPresShell,
                        PRInt32         aColIndex,
                        nsIFrame*       aFirstFrame,
                        nsIFrame*       aLastFrame = nsnull);

  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  NS_IMETHOD IncrementalReflow(nsIPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus);

  NS_IMETHOD IR_TargetIsMe(nsIPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus);

  NS_IMETHOD IR_StyleChanged(nsIPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus);


  NS_IMETHOD IR_TargetIsChild(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus,
                              nsIFrame *               aNextFrame);

// data members

  PRInt32 mColCount;

  /** the starting column index this col group represents. Must be >= 0. */
  PRInt32 mStartColIndex;

  struct ColGroupBits {
    unsigned int mType:4;       
    unsigned int mUnused:28;                         
  } mBits;
};

inline nsTableColGroupFrame::nsTableColGroupFrame()
: mColCount(0), mStartColIndex(0)
{ 
  mBits.mType = 0;
}
  
inline PRInt32 nsTableColGroupFrame::GetStartColumnIndex()
{  
  return mStartColIndex;
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

