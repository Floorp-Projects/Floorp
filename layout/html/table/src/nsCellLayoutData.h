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
#ifndef nsCellLayoutData_h__
#define nsCellLayoutData_h__

#include "nscore.h"
#include "nsSize.h"
#include "nsIFrame.h"
#include "nsTableCellFrame.h"

class nsColLayoutData;
class nsTableFrame;
class nsIStyleContext;
struct nsStyleSpacing;


/** Simple data class that represents in-process reflow information about a cell.
  * Each cell is represented by a nsTableCellFrame together with the results 
  * of the "first pass" layout results (where "first pass" means the cell was told
  * to lay itself out with unrestricted height and width.)
  * There is one nsCellLayoutData object per nsTableFrame for each cell.
  * 
  * TODO: All methods will be inline.
  */
class nsCellLayoutData
{
public:

  /** public constructor.  Does not allocate any of its own data.
    * @param aCellFrame      the frame representing the cell
    * @param aDesiredSize    the max size of the cell if given infinite height and width
    * @param aMaxElementSize the min size of the largest indivisible element within the cell
    */
  nsCellLayoutData(nsTableCellFrame *aCellFrame,
                   nsReflowMetrics * aDesiredSize, nsSize * aMaxElementSize);

  /** destructor, not responsible for destroying any of the stored data */
  virtual ~nsCellLayoutData();

  /** return the frame mapping the cell */
  nsTableCellFrame * GetCellFrame();

  /** set the frame mapping the cell */
  void SetCellFrame(nsTableCellFrame * aCellFrame);

  /** return the max size of the cell if given infinite height and width */
  nsReflowMetrics * GetDesiredSize();

  /** set the max size of the cell if given infinite height and width.
    * called after pass 1 of table reflow is complete.
    */
  void SetDesiredSize(nsReflowMetrics * aDesiredSize);

  /** get the min size of the largest indivisible element within the cell */
  nsSize * GetMaxElementSize();

  /** set the min size of the largest indivisible element within the cell.
    * called after pass 1 of table reflow is complete.
    */
  void SetMaxElementSize(nsSize * aMaxElementSize);

  /** debug method outputs data about this cell to FILE *out */
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  /** returns the style context associated with this cell */
  nsIStyleContext* GetStyleContext();
  
private:  

  // All these methods are support methods for RecalcLayoutData
  nsIFrame* GetFrameAt(nsVoidArray* aList,  PRInt32 aIndex);

  nscoord GetMargin(nsIFrame* aFrame, PRUint8 aEdge) const;

  nscoord GetBorderWidth(nsIFrame* aFrame, PRUint8 aEdge) const;

  nscoord GetPadding(nsIFrame* aFrame, PRUint8 aEdge) const;

  PRUint8 GetOpposingEdge(PRUint8 aEdge);

  nsIFrame* CompareCellBorders(nsIFrame* aFrame1,
                               PRUint8 aEdge1,
                               nsIFrame* aFrame2,
                               PRUint8 aEdge2);

  
  nsIFrame* FindHighestPrecedentBorder(nsVoidArray* aList,
                                       PRUint8 aEdge);
          
  

  nsIFrame* FindInnerBorder( nsVoidArray*  aList,
                             PRUint8 aEdge);

  nsIFrame* FindOuterBorder( nsTableFrame* aTableFrame,
                             PRUint8 aEdge);
  
  nsIFrame* FindBorderFrame(nsTableFrame*    aTableFrame,
                            nsVoidArray*     aCellList,
                            PRUint8          aEdge);

  void CalculateBorders(nsTableFrame* aTableFrame,
                        nsVoidArray*  aBoundaryCells[4]);

  nscoord FindLargestMargin(nsVoidArray* aList,PRUint8 aEdge);


  void CalculateMargins(nsTableFrame* aTableFrame,
                        nsVoidArray*  aBoundaryCells[4]);

public:
  void RecalcLayoutData(nsTableFrame* aTableFrame,
                        nsVoidArray*  aBoundaryCells[4]);

  
  NS_IMETHOD GetMargin(nsMargin& aMargin);
  


private:
#if 0 // these come on line when we do character-based cell content alignment
  /** the kinds of width information stored */
  enum {WIDTH_NORMAL=0, WIDTH_LEFT_ALIGNED=1, WIDTH_RIGHT_ALIGNED=2};

  /** the width information for a cell */
  PRInt32 mWidth[3];
#endif

  nsColLayoutData           *mColLayoutData;
  nsTableCellFrame          *mCellFrame;
  nsReflowMetrics           mDesiredSize;
  nsSize                    mMaxElementSize;

  nsresult                  mCalculated;
  nsMargin                  mMargin;
  nsIFrame*                 mBorderFrame[4];  // the frame whose border is used
};

inline nsTableCellFrame * nsCellLayoutData::GetCellFrame()
{ return mCellFrame; }

inline void nsCellLayoutData::SetCellFrame(nsTableCellFrame * aCellFrame)
{ mCellFrame = aCellFrame; }

inline nsReflowMetrics * nsCellLayoutData::GetDesiredSize()
{ return &mDesiredSize; }

inline void nsCellLayoutData::SetDesiredSize(nsReflowMetrics * aDesiredSize)
{ 
  if (nsnull!=aDesiredSize)
    mDesiredSize = *aDesiredSize;
}

inline nsSize * nsCellLayoutData::GetMaxElementSize()
{ return &mMaxElementSize; }

inline void nsCellLayoutData::SetMaxElementSize(nsSize * aMaxElementSize)
{ 
  if (nsnull!=aMaxElementSize)
    mMaxElementSize = *aMaxElementSize;
}

inline void nsCellLayoutData::CalculateBorders(nsTableFrame*     aTableFrame,
                                        nsVoidArray*      aBoundaryCells[4])
{ 

  for (PRInt32 edge = 0; edge < 4; edge++)
    mBorderFrame[edge] = FindBorderFrame(aTableFrame, aBoundaryCells[edge], edge);
}

inline NS_METHOD nsCellLayoutData::GetMargin(nsMargin& aMargin)
{
  if (mCalculated == NS_OK)
  {
    aMargin = mMargin;
    return NS_OK;
  }
  return NS_ERROR_NOT_INITIALIZED;
}

#endif
