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

#ifndef FixedTableLayoutStrategy_h__
#define FixedTableLayoutStrategy_h__

#include "nscore.h"
#include "BasicTableLayoutStrategy.h"
#include "nsCoord.h"

class nsVoidArray;
class nsTableFrame;
struct nsStylePosition;


/* ---------- FixedTableLayoutStrategy ---------- */

/** Implementation of HTML4 "table-layout=fixed" table layout.
  * The input to this class is the resolved styles for both the
  * table columns and the cells in row0.
  * The output from this class is to set the column widths in
  * mTableFrame.
  */
class FixedTableLayoutStrategy : public BasicTableLayoutStrategy
{
public:

  /** Public constructor.
    * @paran aFrame           the table frame for which this delegate will do layout
    * @param aNumCols         the total number of columns in the table    
    */
  FixedTableLayoutStrategy(nsTableFrame *aFrame);

  /** destructor */
  virtual ~FixedTableLayoutStrategy();

  /** Called during resize reflow to determine the new column widths
    * @param aTableStyle - the resolved style for mTableFrame
	  * @param aReflowState - the reflow state for mTableFrame
	  * @param aMaxWidth - the computed max width for columns to fit into
	  */
  virtual PRBool  BalanceColumnWidths(nsIPresContext*          aPresContext,
                                      const nsHTMLReflowState& aReflowState);

  virtual nscoord CalcPctAdjTableWidth(const nsHTMLReflowState& aReflowState,
                                       nscoord                  aAvailWidth,
                                       float                    aPixelToTwips) {return 0;};
#ifdef DEBUG
  void  SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const {
    *aResult = sizeof(*this);
  }
#endif

protected:
   /* assign the width of all columns
    * if there is a colframe with a width attribute, use it as the column width.
    * otherwise if there is a cell in the first row and it has a width attribute, use it.
    *  if this cell includes a colspan, width is divided equally among spanned columns
    * otherwise the cell get a proportion of the remaining space. 
    *  as determined by the table width attribute.  If no table width attribute, it gets 0 width
    * Computes the minimum and maximum table widths.
    *
    * Set column width information in each column frame and in the table frame.
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    *
    */
  virtual PRBool AssignNonPctColumnWidths(nsIPresContext*          aPresContext,
                                          nscoord                  aComputedWidth,
                                          const nsHTMLReflowState& aReflowState,
                                          float                    aPixelToTwips);


};


#endif

