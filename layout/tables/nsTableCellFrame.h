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
#ifndef nsTableCellFrame_h__
#define nsTableCellFrame_h__

#include "nsITableCellLayout.h"
#include "nscore.h"
#include "nsHTMLContainerFrame.h"
#include "nsTableRowFrame.h"  // need to actually include this here to inline GetRowIndex
#include "nsIStyleContext.h"

struct nsStyleSpacing;
class nsTableFrame;
class nsHTMLValue;

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
class nsTableCellFrame : public nsHTMLContainerFrame, public nsITableCellLayout
{
public:

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // default constructor supplied by the compiler

  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  // table cells contain an area frame which does most of the work, and
  // so these functions should never be called. They assert and return
  // NS_ERROR_NOT_IMPLEMENTED
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

  void InitCellFrame(PRInt32 aColIndex);

  void SetBorderEdge(PRUint8 aSide, 
                     PRInt32 aRowIndex, 
                     PRInt32 aColIndex, 
                     nsBorderEdge *border,
                     nscoord aOddAmountToAdd);

  void SetBorderEdgeLength(PRUint8 aSide, 
                           PRInt32 aIndex, 
                           nscoord aLength);


  /** instantiate a new instance of nsTableCellFrame.
    * @param aResult    the new object is returned in this out-param
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableCellFrame(nsIFrame** aResult);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD SetSelected(nsIDOMRange *aRange,PRBool aSelected, nsSpread aSpread);

  NS_IMETHOD Reflow(nsIPresContext&      aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableCellFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  virtual void VerticallyAlignChild();

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
  virtual nsresult SetColIndex(PRInt32 aColIndex);

  /** return the available width given to this frame during its last reflow */
  virtual nscoord GetPriorAvailWidth();
  
  /** set the available width given to this frame during its last reflow */
  virtual void SetPriorAvailWidth(nscoord aPriorAvailWidth);

  /** return the desired size returned by this frame during its last reflow */
  virtual nsSize GetDesiredSize();

  /** set the desired size returned by this frame during its last reflow */
  virtual void SetDesiredSize(const nsHTMLReflowMetrics & aDesiredSize);

  /** return the desired size returned by this frame during its last reflow */
  virtual nsSize GetPass1DesiredSize() const;

  /** set the desired size returned by this frame during its last reflow */
  virtual void SetPass1DesiredSize(const nsHTMLReflowMetrics & aDesiredSize);

  /** return the MaxElement size returned by this frame during its last reflow 
    * not counting reflows where MaxElementSize is not requested.  
    * That is, the cell frame will always remember the last non-null MaxElementSize
    */
  virtual nsSize GetPass1MaxElementSize() const;

  /** set the MaxElement size returned by this frame during its last reflow.
    * should never be called with a null MaxElementSize
    */
  virtual void SetPass1MaxElementSize(const nsSize & aMaxElementSize);

  void RecalcLayoutData(nsMargin& aMargin);
  
  NS_IMETHOD GetMargin(nsMargin& aMargin);

  PRBool GetContentEmpty();
  void SetContentEmpty(PRBool aContentEmpty);

  // The collapse offset is (0,0) except for cells originating in a row/col which is collapsed
  void    SetCollapseOffsetX(nscoord aXOffset);
  void    SetCollapseOffsetY(nscoord aYOffset);
  nsPoint GetCollapseOffset();

protected:
  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  virtual PRBool ParentDisablesSelection() const; //override default behavior

private:  

  // All these methods are support methods for RecalcLayoutData
  nsIFrame* GetFrameAt(nsVoidArray* aList,  PRInt32 aIndex);

  nscoord GetMargin(nsIFrame* aFrame, PRUint8 aEdge) const;

  //XXX: aTableFrame can be removed as soon as border-collapse inherits correctly
  void GetCellBorder(nsMargin &aBorder, nsTableFrame *aTableFrame);

  PRUint8 GetOpposingEdge(PRUint8 aEdge);

protected:

  // Subclass hook for style post processing
  NS_IMETHOD DidSetStyleContext(nsIPresContext* aPresContext);

  void      MapBorderMarginPadding(nsIPresContext* aPresContext);

  void      MapHTMLBorderStyle(nsIPresContext* aPresContext,
                               nsStyleSpacing& aSpacingStyle, 
                               nscoord         aBorderWidth,
                               nsTableFrame*   aTableFrame);

  void      MapVAlignAttribute(nsIPresContext* aPresContext, nsTableFrame *aTableFrame);
  void      MapHAlignAttribute(nsIPresContext* aPresContext, nsTableFrame *aTableFrame);

  PRBool    ConvertToPixelValue(nsHTMLValue& aValue, PRInt32 aDefault, PRInt32& aResult);

protected:

  /** the starting column for this cell */
  int          mColIndex;

  /** the available width we were given in our previous reflow */
  nscoord      mPriorAvailWidth;

  /** these are the last computed desired and max element sizes */
  nsSize       mDesiredSize;
  nsSize       mMaxElementSize;

  /** these are the Pass 1 unconstrained desired and max element sizes */
  nsSize       mPass1DesiredSize;
  nsSize       mPass1MaxElementSize;

  nsresult     mCalculated;
  nsMargin     mMargin;
  PRBool       mIsContentEmpty;  // PR_TRUE if the cell's contents take up no space
  //XXX: mIsContentEmpty should get yanked in favor of using free a bit on the frame base class
  //     the FrameState slot (mState; GetFrameState/SetFrameState)

  nsPoint      mCollapseOffset;

public:
  nsBorderEdges mBorderEdges;       // one list of border segments for each side of the table frame
                                    // used only for the collapsing border model

};

inline nsresult nsTableCellFrame::GetRowIndex(PRInt32 &aRowIndex) const
{
  nsTableCellFrame* cell = (nsTableCellFrame*)GetFirstInFlow();
  nsresult result;
  nsTableRowFrame * row;
  cell->GetParent((nsIFrame **)&row);
  if (nsnull!=row)
  {
    aRowIndex = row->GetRowIndex();
    result = NS_OK;
  }
  else
  {
    aRowIndex = 0;
    result = NS_ERROR_NOT_INITIALIZED;
  }
  return result;
}

inline nsresult nsTableCellFrame::GetColIndex(PRInt32 &aColIndex) const
{  
  aColIndex = mColIndex;
  return  NS_OK;
}

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

inline nsSize nsTableCellFrame::GetPass1DesiredSize() const
{ return mPass1DesiredSize; }

inline void nsTableCellFrame::SetPass1DesiredSize(const nsHTMLReflowMetrics & aDesiredSize)
{ 
  mPass1DesiredSize.width = aDesiredSize.width;
  mPass1DesiredSize.height = aDesiredSize.height;
}

inline nsSize nsTableCellFrame::GetPass1MaxElementSize() const
{ return mPass1MaxElementSize; }

inline void nsTableCellFrame::SetPass1MaxElementSize(const nsSize & aMaxElementSize)
{ 
  mPass1MaxElementSize.width = aMaxElementSize.width;
  mPass1MaxElementSize.height = aMaxElementSize.height;
}

inline NS_METHOD nsTableCellFrame::GetMargin(nsMargin& aMargin)
{
  if (mCalculated == NS_OK)
  {
    aMargin = mMargin;
    return NS_OK;
  }
  return NS_ERROR_NOT_INITIALIZED;
}

inline PRBool nsTableCellFrame::GetContentEmpty()
{
  return mIsContentEmpty;
}

inline void nsTableCellFrame::SetContentEmpty(PRBool aContentEmpty)
{
  mIsContentEmpty = aContentEmpty;
}

#endif



