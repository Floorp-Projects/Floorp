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

  /** returns the style molecule associated with this cell */
  nsStyleMolecule* GetStyleMolecule();
  
private:  

  // All these methods are support methods for RecalcLayoutData
  nsStyleMolecule* GetStyleMoleculeAt(nsVoidArray* aList,  PRInt32 aIndex);

  nscoord GetMargin(nsStyleMolecule* aStyle,PRUint8 aEdge);

  nscoord GetBorderWidth(nsStyleMolecule* aStyle,PRUint8 aEdge);

  nscoord GetPadding(nsStyleMolecule* aStyle,PRUint8 aEdge);

  PRUint8 GetOpposingEdge(PRUint8 aEdge);

  nsStyleMolecule* CompareCellBorders(nsStyleMolecule* aStyle1,
                                      PRUint8 aEdge1,
                                      nsStyleMolecule* aStyle2,
                                      PRUint8 aEdge2);

  
  nsStyleMolecule* FindHighestPrecedentBorder(nsVoidArray* aList,
                                              PRUint8 aEdge);
          
  

  nsStyleMolecule* FindInnerBorder( nsStyleMolecule* aStyle,
                                    nsVoidArray*  aList,
                                    PRUint8 aEdge);

  nsStyleMolecule* FindOuterBorder( nsTableFrame* aTableFrame,
                                    nsIFrame* aFrame, 
                                    nsStyleMolecule* aStyle,
                                    PRUint8 aEdge);
  
  nsStyleMolecule* FindBorderStyle(nsTableFrame*    aTableFrame,
                                   nsStyleMolecule* aCellStyle,
                                   nsVoidArray*     aCellList,
                                   PRUint8          aEdge);

  void CalculateBorders(nsTableFrame* aTableFrame,
                        nsStyleMolecule*  aCellStyle,
                        nsVoidArray*  aBoundaryCells[4]);

  nscoord FindLargestMargin(nsVoidArray* aList,PRUint8 aEdge);


  void CalculateMargins(nsTableFrame* aTableFrame,
                        nsStyleMolecule*  aCellStyle,
                        nsVoidArray*  aBoundaryCells[4]);

public:
  void RecalcLayoutData(nsTableFrame* aTableFrame,
                        nsVoidArray*  aBoundaryCells[4]);


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

  nsMargin                  mMargin;
  nsStyleMolecule*          mBorderStyle[4];
};

#endif
