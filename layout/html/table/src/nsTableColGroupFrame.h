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
#ifndef nsTableColGroupFrame_h__
#define nsTableColGroupFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"

class nsTableColFrame;


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
  NS_NewTableColGroupFrame(nsIFrame** aResult);

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD AppendFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext& aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  /** reflow of a column group is a trivial matter of reflowing
    * the col group's children (columns), and setting this frame
    * to 0-size.  Since tables are row-centric, column group frames
    * don't play directly in the rendering game.  They do however
    * maintain important state that effects table and cell layout.
    */
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  /** returns the number of columns represented by this group.
    * if there are col children, count them (taking into account the span of each)
    * else, check my own span attribute.
    */
  virtual PRInt32 GetColumnCount();

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

  void DeleteColFrame(nsIPresContext& aPresContext, nsTableColFrame* aColFrame);

protected:

  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  /** Hook for style post processing.  
    * Since we need to know the full column structure before the COLS attribute
    * can be interpreted, we can't just use DidSetStyleContext
    */
  NS_IMETHOD SetStyleContextForFirstPass(nsIPresContext& aPresContext);

  NS_IMETHOD InitNewFrames(nsIPresContext& aPresContext, nsIFrame* aChildList);
  NS_IMETHOD AppendNewFrames(nsIPresContext& aPresContext, nsIFrame* aChildList);


  NS_IMETHOD IncrementalReflow(nsIPresContext&          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus);

  NS_IMETHOD IR_TargetIsMe(nsIPresContext&          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus);

  NS_IMETHOD IR_StyleChanged(nsIPresContext&          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus);


  NS_IMETHOD IR_TargetIsChild(nsIPresContext&          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus,
                              nsIFrame *               aNextFrame);

  nsresult AddTableDirtyReflowCommand(nsIPresContext& aPresContext,
                                      nsIPresShell&   aPresShell,
                                      nsIFrame*       aTableFrame);

// data members

  PRInt32 mColCount;

  /** the starting column index this col group represents. Must be >= 0. */
  PRInt32 mStartColIndex;

};

inline int nsTableColGroupFrame::GetStartColumnIndex ()
{  return mStartColIndex;}
  
#endif
