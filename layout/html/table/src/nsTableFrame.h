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
#ifndef nsTableFrame_h__
#define nsTableFrame_h__

#include "nscore.h"
#include "nsContainerFrame.h"

class nsCellLayoutData;
class nsTableCell;
class nsVoidArray;
class nsTableCellFrame;
class CellData;
struct nsStyleMolecule;
struct InnerTableReflowState;

/**
  */
class nsTableFrame : public nsContainerFrame
{
public:

  friend class nsTableOuterFrame;

  static nsresult NewFrame(nsIFrame** aInstancePtrResult,
                           nsIContent* aContent,
                           PRInt32     aIndexInParent,
                           nsIFrame*   aParent);

  virtual void Paint(nsIPresContext& aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect);

  virtual ReflowStatus ResizeReflow(nsIPresContext* aPresContext,
                                    nsReflowMetrics& aDesiredSize,
                                    const nsSize& aMaxSize,
                                    nsSize* aMaxElementSize);

  virtual ReflowStatus  IncrementalReflow(nsIPresContext* aPresContext,
                                          nsReflowMetrics& aDesiredSize,
                                          const nsSize&    aMaxSize,
                                          nsReflowCommand& aReflowCommand);

  /**
   * @see nsContainerFrame
   */
  virtual nsIFrame* CreateContinuingFrame(nsIPresContext* aPresContext,
                                          nsIFrame*       aParent);

  /** resize myself and my children according to the arcane rules of cell height magic. 
    * By default, the height of a cell is the max (height of cells in its row)
    * In the case of a cell with rowspan>1 (lets call this C),
    *   if the sum of the height of the rows spanned (excluding C in the calculation)
    *   is greater than or equal to the height of C, 
    *     the cells in the rows are sized normally and 
    *     the height of C is set to the sum of the heights (taking into account borders, padding, and margins)
    *   else
    *     the height of each row is expanded by a percentage of the difference between
    *     the row's desired height and the height of C
    */
  virtual void ShrinkWrapChildren(nsIPresContext* aPresContext, 
                                  nsReflowMetrics& aDesiredSize,
                                  nsSize* aMaxElementSize);
  
  /** return the column layout data for this inner table frame.
    * if this is a continuing frame, return the first-in-flow's column layout data.
    */
  virtual nsVoidArray *GetColumnLayoutData();

  /** Associate aData with the cell at (aRow,aCol)
    * @return PR_TRUE if the data was successfully associated with a Cell
    *         PR_FALSE if there was an error, such as aRow or aCol being invalid
    */
  virtual PRBool SetCellLayoutData(nsCellLayoutData * aData, nsTableCell *aCell);

  /** Get the layout data associated with the cell at (aRow,aCol)
    * @return PR_NULL if there was an error, such as aRow or aCol being invalid
    *         otherwise, the data is returned.
    */
  virtual nsCellLayoutData * GetCellLayoutData(nsTableCell *aCell);

  /**
    */
          PRBool IsProportionalWidth(nsStyleMolecule* aMol);


          
  /**
    * DEBUG METHOD
    *
    */
  
  void    ListColumnLayoutData(FILE* out = stdout, PRInt32 aIndent = 0) const;

  
  /**
    */
          PRInt32 GetColumnWidth(PRInt32 aColIndex);

  /**
    */
          void SetColumnWidth(PRInt32 aColIndex, PRInt32 aWidth);


          
  /**
    * Calculate Layout Information
    *
    */
  nsCellLayoutData* FindCellLayoutData(nsTableCell* aCell);
  void    AppendLayoutData(nsVoidArray* aList, nsTableCell* aTableCell);
  void    ResetColumnLayoutData();
  void    ResetCellLayoutData( nsTableCell* aCell, 
                               nsTableCell* aAbove,
                               nsTableCell* aBelow,
                               nsTableCell* aLeft,
                               nsTableCell* aRight);



protected:

  nsTableFrame(nsIContent* aContent,
                PRInt32 aIndexInParent,
                nsIFrame* aParentFrame);

  virtual ~nsTableFrame();

  /**
    */
  virtual void DeleteColumnLayoutData();

  /**
    */
  virtual nsIFrame::ReflowStatus ResizeReflowPass1(nsIPresContext*  aPresContext,
                                                   nsReflowMetrics& aDesiredSize,
                                                   const nsSize&    aMaxSize,
                                                   nsSize*          aMaxElementSize,
                                                   nsStyleMolecule* aTableStyle);

  /**
    * aMinCaptionWidth - the max of all the minimum caption widths.  0 if no captions.
    * aMaxCaptionWidth - the max of all the desired caption widths.  0 if no captions.
    */
  virtual nsIFrame::ReflowStatus ResizeReflowPass2(nsIPresContext*  aPresContext,
                                                   nsReflowMetrics& aDesiredSize,
                                                   const nsSize&    aMaxSize,
                                                   nsSize*          aMaxElementSize,
                                                   nsStyleMolecule* aTableStyle,
                                                   PRInt32 aMinCaptionWidth,
                                                   PRInt32 mMaxCaptionWidth);

  nscoord GetTopMarginFor(nsIPresContext* aCX,
                          InnerTableReflowState& aState,
                          nsStyleMolecule* aKidMol);

  void PlaceChild(nsIPresContext*    aPresContext,
                  InnerTableReflowState& aState,
                  nsIFrame*          aKidFrame,
                  const nsRect&      aKidRect,
                  nsStyleMolecule*   aKidMol,
                  nsSize*            aMaxElementSize,
                  nsSize&            aKidMaxElementSize);

  PRBool        ReflowMappedChildren(nsIPresContext*        aPresContext,
                                     InnerTableReflowState& aState,
                                     nsSize*                aMaxElementSize);

  PRBool        PullUpChildren(nsIPresContext*        aPresContext,
                               InnerTableReflowState& aState,
                               nsSize*                aMaxElementSize);

  ReflowStatus  ReflowUnmappedChildren(nsIPresContext*        aPresContext,
                                       InnerTableReflowState& aState,
                                       nsSize*                aMaxElementSize);


  /** 
    */
  virtual void BalanceColumnWidths(nsIPresContext*  aPresContext, 
                                   nsStyleMolecule* aTableStyle,
                                   const nsSize&    aMaxSize, 
                                   nsSize*          aMaxElementSize);

  /**
    */
  virtual PRBool AssignFixedColumnWidths(nsIPresContext* aPresContext, 
                                         PRInt32   aMaxWidth, 
                                         PRInt32   aNumCols, 
                                         nsStyleMolecule* aTableStyleMol,
                                         PRInt32 & aTotalFixedWidth,
                                         PRInt32 & aMinTableWidth, 
                                         PRInt32 & aMaxTableWidth);
  /**
   */
  virtual PRBool BalanceProportionalColumnsForSpecifiedWidthTable(nsIPresContext*  aPresContext,
                                                                  nsStyleMolecule* aTableStyleMol,
                                                                  PRInt32 aAvailWidth,
                                                                  PRInt32 aMaxWidth,
                                                                  PRInt32 aMinTableWidth, 
                                                                  PRInt32 aMaxTableWidth);

  /**
   */
  virtual PRBool BalanceProportionalColumnsForAutoWidthTable(nsIPresContext*  aPresContext,
                                                             nsStyleMolecule* aTableStyleMol,
                                                             PRInt32 aAvailWidth,
                                                             PRInt32 aMaxWidth,
                                                             PRInt32 aMinTableWidth, 
                                                             PRInt32 aMaxTableWidth);

  virtual PRBool SetColumnsToMinWidth(nsIPresContext* aPresContext);

  virtual PRBool BalanceColumnsTableFits(nsIPresContext*  aPresContext, 
                                         nsStyleMolecule* aTableStyleMol, 
                                         PRInt32          aAvailWidth);

  virtual PRBool BalanceColumnsHTML4Constrained(nsIPresContext*  aPresContext,
                                                nsStyleMolecule* aTableStyleMol, 
                                                PRInt32 aAvailWidth,
                                                PRInt32 aMaxWidth,
                                                PRInt32 aMinTableWidth, 
                                                PRInt32 aMaxTableWidth);

  /**
  */
  virtual void SetTableWidth(nsIPresContext*  aPresContext, 
                             nsStyleMolecule* aTableStyle);

  /**
    */
  virtual void VerticallyAlignChildren(nsIPresContext* aPresContext,
                                        nscoord* aAscents,
                                        nscoord aMaxAscent,
                                        nscoord aMaxHeight);

  /** support routine returns PR_TRUE if the table should be sized "automatically" 
    * according to its content.  
    * In NAV4, this is the default (no WIDTH, no COLS).
    */
  virtual PRBool TableIsAutoWidth();

  /** support routine returns PR_TRUE if the table columns should be sized "automatically" 
    * according to its content.  
    * In NAV4, this is when there is a COLS attribute on the table.
    */
  virtual PRBool AutoColumnWidths(nsStyleMolecule* aTableStyleMol);

  /** given the new parent size, do I really need to do a reflow? */
  virtual PRBool NeedsReflow(const nsSize& aMaxSize);

  virtual PRInt32 GetReflowPass() const;

  virtual void SetReflowPass(PRInt32 aReflowPass);

  virtual PRBool IsFirstPassValid() const;

private:
  void nsTableFrame::DebugPrintCount() const; // Debugging routine


  /**  table reflow is a multi-pass operation.  Use these constants to keep track of
    *  which pass is currently being executed.
    */
  enum {kPASS_UNDEFINED=0, kPASS_FIRST=1, kPASS_SECOND=2, kPASS_THIRD=3};

  nsVoidArray *mColumnLayoutData;   // array of array of cellLayoutData's
  PRInt32     *mColumnWidths;       // widths of each column
  PRBool       mFirstPassValid;     // PR_TRUE if first pass data is still legit
  PRInt32      mPass;               // which Reflow pass are we currently in?
  PRBool       mIsInvariantWidth;   // PR_TRUE if table width cannot change

};

#endif
