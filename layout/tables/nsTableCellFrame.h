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

#include "nscore.h"
#include "nsHTMLContainerFrame.h"
#include "nsTableRowFrame.h"  // need to actually include this here to inline GetRowIndex

struct nsStyleSpacing;
class nsTableFrame;
class nsHTMLValue;

/**
 * nsTableCellFrame
 * data structure to maintain information about a single table cell's frame
 *
 * @author  sclark
 */
class nsTableCellFrame : public nsHTMLContainerFrame
{
public:

  // default constructor supplied by the compiler

  void InitCellFrame(PRInt32 aColIndex);

  /** instantiate a new instance of nsTableCellFrame.
    * @param aResult    the new object is returned in this out-param
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableCellFrame(nsIFrame*& aResult);

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD Reflow(nsIPresContext&      aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  /**
   * @see nsContainerFrame
   */
  NS_IMETHOD CreateContinuingFrame(nsIPresContext&  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  virtual void VerticallyAlignChild();

  /** return the mapped cell's row span.  Always >= 1. */
  virtual PRInt32 GetRowSpan();

  // there is no set row index because row index depends on the cell's parent row only

  /** return the mapped cell's row index (starting at 0 for the first row) */
  virtual PRInt32 GetRowIndex();

  /** return the mapped cell's col span.  Always >= 1. */
  virtual PRInt32 GetColSpan();
  
  /** return the mapped cell's column index (starting at 0 for the first column) */
  virtual PRInt32 GetColIndex();

  /** return the available width given to this frame during its last reflow */
  virtual nscoord GetPriorAvailWidth();
  
  /** set the available width given to this frame during its last reflow */
  virtual void SetPriorAvailWidth(nscoord aPriorAvailWidth);

  /** return the desired size returned by this frame during its last reflow */
  virtual nsSize GetDesiredSize();

  /** set the desired size returned by this frame during its last reflow */
  virtual void SetDesiredSize(const nsHTMLReflowMetrics & aDesiredSize);

  /** return the desired size returned by this frame during its last reflow */
  virtual nsSize GetPass1DesiredSize();

  /** set the desired size returned by this frame during its last reflow */
  virtual void SetPass1DesiredSize(const nsHTMLReflowMetrics & aDesiredSize);

  /** return the MaxElement size returned by this frame during its last reflow 
    * not counting reflows where MaxElementSize is not requested.  
    * That is, the cell frame will always remember the last non-null MaxElementSize
    */
  virtual nsSize GetPass1MaxElementSize();

  /** set the MaxElement size returned by this frame during its last reflow.
    * should never be called with a null MaxElementSize
    */
  virtual void SetPass1MaxElementSize(const nsSize & aMaxElementSize);

  void RecalcLayoutData(nsTableFrame* aTableFrame,
                        nsVoidArray*  aBoundaryCells[4]);

  
  NS_IMETHOD GetMargin(nsMargin& aMargin);

  PRBool GetContentEmpty();
  void SetContentEmpty(PRBool aContentEmpty);

protected:
  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

private:  

  // All these methods are support methods for RecalcLayoutData
  nsIFrame* GetFrameAt(nsVoidArray* aList,  PRInt32 aIndex);

  nscoord GetMargin(nsIFrame* aFrame, PRUint8 aEdge) const;

  nscoord GetBorderWidth(nsIFrame* aFrame, PRUint8 aEdge) const;

  nscoord GetPadding(nsIFrame* aFrame, PRUint8 aEdge) const;

  PRUint8 GetOpposingEdge(PRUint8 aEdge);

  void CalculateBorders(nsTableFrame* aTableFrame,
                        nsVoidArray*  aBoundaryCells[4]);

  nscoord FindLargestMargin(nsVoidArray* aList,PRUint8 aEdge);


  void CalculateMargins(nsTableFrame* aTableFrame,
                        nsVoidArray*  aBoundaryCells[4]);

  NS_IMETHOD  List(FILE* out = stdout, PRInt32 aIndent = 0, nsIListFilter *aFilter = nsnull) const;

protected:

  // Subclass hook for style post processing
  NS_IMETHOD DidSetStyleContext(nsIPresContext* aPresContext);

  void      MapBorderMarginPadding(nsIPresContext* aPresContext);

  void      MapHTMLBorderStyle(nsIPresContext* aPresContext,
                               nsStyleSpacing& aSpacingStyle, 
                               nscoord aBorderWidth,
                               nsTableFrame *aTableFrame);

  PRBool    ConvertToPixelValue(nsHTMLValue& aValue, PRInt32 aDefault, PRInt32& aResult);

  NS_IMETHOD IR_StyleChanged(nsIPresContext&          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus);

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
  nsIFrame*    mBorderFrame[4];  // the frame whose border is used
  PRBool       mIsContentEmpty;  // PR_TRUE if the cell's contents take up no space
  //XXX: mIsContentEmpty should get yanked in favor of using free a bit on the frame base class
  //     the FrameState slot (mState; GetFrameState/SetFrameState)

};

inline void nsTableCellFrame::InitCellFrame(PRInt32 aColIndex)
{
  NS_PRECONDITION(0<=aColIndex, "bad col index arg");
  mColIndex = aColIndex;
}


inline PRInt32 nsTableCellFrame::GetRowIndex()
{
  nsTableRowFrame * row;
  GetContentParent((nsIFrame *&)row);
  if (nsnull!=row)
    return row->GetRowIndex();
  else
    return 0;
}

inline PRInt32 nsTableCellFrame::GetColIndex()
{  return mColIndex;}

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

inline nsSize nsTableCellFrame::GetPass1DesiredSize()
{ return mPass1DesiredSize; }

inline void nsTableCellFrame::SetPass1DesiredSize(const nsHTMLReflowMetrics & aDesiredSize)
{ 
  mPass1DesiredSize.width = aDesiredSize.width;
  mPass1DesiredSize.height = aDesiredSize.height;
}

inline nsSize nsTableCellFrame::GetPass1MaxElementSize()
{ return mPass1MaxElementSize; }

inline void nsTableCellFrame::SetPass1MaxElementSize(const nsSize & aMaxElementSize)
{ 
  mPass1MaxElementSize.width = aMaxElementSize.width;
  mPass1MaxElementSize.height = aMaxElementSize.height;
}

inline void nsTableCellFrame::CalculateBorders(nsTableFrame* aTableFrame,
                                               nsVoidArray*  aBoundaryCells[4])
{ 

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



