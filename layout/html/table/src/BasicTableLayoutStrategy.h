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

#ifndef BasicTableLayoutStrategy_h__
#define BasicTableLayoutStrategy_h__

#include "nscore.h"
#include "nsITableLayoutStrategy.h"

class nsTableFrame;

/* ----------- SpanInfo ---------- */

struct SpanInfo
{
  PRInt32 span;
  PRInt32 cellMinWidth;
  PRInt32 cellDesiredWidth;

  SpanInfo(PRInt32 aSpan, PRInt32 aMinWidth, PRInt32 aDesiredWidth)
  {
    span = aSpan;
    cellMinWidth = aMinWidth;
    cellDesiredWidth = aDesiredWidth;
  };

  ~SpanInfo() {};
};


/* ---------- BasicTableLayoutStrategy ---------- */

class BasicTableLayoutStrategy : public nsITableLayoutStrategy
{
public:

  BasicTableLayoutStrategy(nsTableFrame *aFrame);

  ~BasicTableLayoutStrategy();

  virtual PRBool BalanceColumnWidths(nsIPresContext* aPresContext,
                                     PRInt32 aMaxWidth, 
                                     PRInt32 aNumCols,
                                     PRInt32 &aTotalFixedWidth,
                                     PRInt32 &aMinTableWidth,
                                     PRInt32 &aMaxTableWidth,
                                     nsSize* aMaxElementSize);
  /** assign widths for each column that has fixed width.  
    * Computes the minimum and maximum table widths. 
    * Sets mColumnWidths as a side effect.
    *
    * @param aPresContext     the presentation context
    * @param aMaxWidth        the maximum width of the table
    * @param aNumCols         the total number of columns in the table
    * @param aTableStyle      the resolved style for the table
    * @param aTotalFixedWidth out param, the sum of the fixed width columns
    * @param aMinTableWidth   out param, the min possible table width
    * @param aMaxTableWidth   out param, the max table width
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    *
    * TODO: should be renamed to "AssignKnownWidthInformation
    */
  virtual PRBool AssignFixedColumnWidths(nsIPresContext* aPresContext, 
                                         PRInt32   aMaxWidth, 
                                         PRInt32   aNumCols, 
                                         PRInt32 & aTotalFixedWidth,
                                         PRInt32 & aMinTableWidth, 
                                         PRInt32 & aMaxTableWidth);

  /** assign widths for each column that has proportional width inside a table that 
    * has a fixed width.
    * Sets mColumnWidths as a side effect.
    *
    * @param aPresContext     the presentation context
    * @param aTableStyle      the resolved style for the table
    * @param aAvailWidth      the remaining amount of horizontal space available
    * @param aMaxWidth        the total amount of horizontal space available
    * @param aMinTableWidth   the min possible table width
    * @param aMaxTableWidth   the max table width
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    *
    * TODO: rename this method to reflect that it is a Nav4 compatibility method
    */
  virtual PRBool BalanceProportionalColumnsForSpecifiedWidthTable(nsIPresContext*  aPresContext,
                                                                  PRInt32 aAvailWidth,
                                                                  PRInt32 aMaxWidth,
                                                                  PRInt32 aMinTableWidth, 
                                                                  PRInt32 aMaxTableWidth);

  /** assign widths for each column that has proportional width inside a table that 
    * has auto width (width set by the content and available space.)
    * Sets mColumnWidths as a side effect.
    *
    * @param aPresContext     the presentation context
    * @param aTableStyle      the resolved style for the table
    * @param aAvailWidth      the remaining amount of horizontal space available
    * @param aMaxWidth        the total amount of horizontal space available
    * @param aMinTableWidth   the min possible table width
    * @param aMaxTableWidth   the max table width
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    *
    * TODO: rename this method to reflect that it is a Nav4 compatibility method
    */
  virtual PRBool BalanceProportionalColumnsForAutoWidthTable(nsIPresContext*  aPresContext,
                                                             PRInt32 aAvailWidth,
                                                             PRInt32 aMaxWidth,
                                                             PRInt32 aMinTableWidth, 
                                                             PRInt32 aMaxTableWidth);

  /** assign the minimum allowed width for each column that has proportional width.
    * Typically called when the min table width doesn't fit in the available space.
    * Sets mColumnWidths as a side effect.
    *
    * @param aPresContext     the presentation context
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    */
  virtual PRBool SetColumnsToMinWidth(nsIPresContext* aPresContext);

  /** assign the maximum allowed width for each column that has proportional width.
    * Typically called when the desired max table width fits in the available space.
    * Sets mColumnWidths as a side effect.
    *
    * @param aPresContext     the presentation context
    * @param aTableStyle      the resolved style for the table
    * @param aAvailWidth      the remaining amount of horizontal space available
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    */
  virtual PRBool BalanceColumnsTableFits(nsIPresContext*  aPresContext, 
                                         PRInt32          aAvailWidth);

  /** assign widths for each column that has proportional width inside a table that 
    * has auto width (width set by the content and available space) according to the
    * HTML 4 specification.
    * Sets mColumnWidths as a side effect.
    *
    * @param aPresContext     the presentation context
    * @param aTableStyle      the resolved style for the table
    * @param aAvailWidth      the remaining amount of horizontal space available
    * @param aMaxWidth        the total amount of horizontal space available
    * @param aMinTableWidth   the min possible table width
    * @param aMaxTableWidth   the max table width
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    *
    * TODO: rename this method to reflect that it is a Nav4 compatibility method
    */
  virtual PRBool BalanceColumnsConstrained(nsIPresContext*  aPresContext,
                                           PRInt32 aAvailWidth,
                                           PRInt32 aMaxWidth,
                                           PRInt32 aMinTableWidth, 
                                           PRInt32 aMaxTableWidth);


protected:
  nsTableFrame * mTableFrame;

};

#endif

