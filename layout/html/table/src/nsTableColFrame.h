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
#ifndef nsTableColFrame_h__
#define nsTableColFrame_h__

#include "nscore.h"
#include "nsContainerFrame.h"

class nsVoidArray;

class nsTableColFrame : public nsFrame {
public:

  enum {eWIDTH_SOURCE_NONE          =0,   // no cell has contributed to the width style
        eWIDTH_SOURCE_CELL          =1,   // a cell specified a width
        eWIDTH_SOURCE_CELL_WITH_SPAN=2    // a cell implicitly specified a width via colspan
  };

  void InitColFrame(PRInt32 aColIndex);

  /** instantiate a new instance of nsTableColFrame.
    * @param aResult    the new object is returned in this out-param
    * @param aContent   the table object to map
    * @param aParent    the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableColFrame(nsIFrame*& aResult);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  /** return the index of the column the col represents.  always >= 0 */
  virtual PRInt32 GetColumnIndex ();

  /** return the number of the columns the col represents.  always >= 0 */
  virtual PRInt32 GetSpan ();

  /** set the index of the column this content object represents.  must be >= 0 */
  virtual void SetColumnIndex (int aColIndex);

  /** convenience method, calls into cellmap */
  nsVoidArray * GetCells();

  nscoord GetMaxColWidth();
  void SetMaxColWidth(nscoord aMaxColWidth);

  nscoord GetMinColWidth();
  void SetMinColWidth(nscoord aMinColWidth);

  nscoord GetEffectiveMaxColWidth();
  void SetEffectiveMaxColWidth(nscoord aMaxColWidth);

  nscoord GetEffectiveMinColWidth();
  void SetEffectiveMinColWidth(nscoord aMinColWidth);

  nscoord GetAdjustedMinColWidth();
  void SetAdjustedMinColWidth(nscoord aMinColWidth);

  PRInt32 GetWidthSource();
  void SetWidthSource(PRInt32 aMinColWidth);

  nscoord GetColWidthForComputation();

  /** convenience method, calls into cellmap */
  PRInt32 Count() const;

protected:

  nsTableColFrame();

  /** the starting index of the column (starting at 0) that this col object represents */
  PRInt32  mColIndex;


  nscoord mMaxColWidth;
  nscoord mMinColWidth;

  nscoord mMaxEffectiveColWidth;
  nscoord mMinEffectiveColWidth;

  nscoord mMinAdjustedColWidth;

  PRInt32 mWidthSource;

};


inline void nsTableColFrame::InitColFrame(PRInt32 aColIndex)
{
  NS_ASSERTION(0<=aColIndex, "bad col index param");
  mColIndex = aColIndex;
}

inline PRInt32 nsTableColFrame::GetColumnIndex()
{ return mColIndex; }

inline void nsTableColFrame::SetColumnIndex (int aColIndex)
{  mColIndex = aColIndex;}

inline nscoord nsTableColFrame::GetMaxColWidth()
{ return mMaxColWidth; }

inline void nsTableColFrame::SetMaxColWidth(nscoord aMaxColWidth)
{ mMaxColWidth = aMaxColWidth; }

inline nscoord nsTableColFrame::GetMinColWidth()
{ return mMinColWidth; }

inline void nsTableColFrame::SetMinColWidth(nscoord aMinColWidth)
{ mMinColWidth = aMinColWidth; }

inline nscoord nsTableColFrame::GetEffectiveMaxColWidth()
{ return mMaxEffectiveColWidth; }

inline void nsTableColFrame::SetEffectiveMaxColWidth(nscoord aMaxColWidth)
{ mMaxEffectiveColWidth = aMaxColWidth; }

inline nscoord nsTableColFrame::GetEffectiveMinColWidth()
{ return mMinEffectiveColWidth; }

inline void nsTableColFrame::SetEffectiveMinColWidth(nscoord aMinEffectiveColWidth)
{ mMinEffectiveColWidth = aMinEffectiveColWidth; }

inline nscoord nsTableColFrame::GetAdjustedMinColWidth()
{ return mMinAdjustedColWidth; }

inline void nsTableColFrame::SetAdjustedMinColWidth(nscoord aMinAdjustedColWidth)
{ mMinAdjustedColWidth = aMinAdjustedColWidth; }

inline PRInt32 nsTableColFrame::GetWidthSource()
{ return mWidthSource; }

inline void nsTableColFrame::SetWidthSource(PRInt32 aWidthSource)
{ mWidthSource = aWidthSource; }
#endif

