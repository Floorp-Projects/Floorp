/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsITableLayoutStrategy_h__
#define nsITableLayoutStrategy_h__

#include "nscore.h"
#include "nsSize.h"

class nsIPresContext;
class nsIStyleContext;
struct nsHTMLReflowState;
struct nsMargin;
class nsTableCellFrame;
class nsStyleCoord;
class nsISizeOfHandler;

class nsITableLayoutStrategy
{
public:
  virtual ~nsITableLayoutStrategy() {};

  /** call once every time any table thing changes (content, structure, or style) 
    * @param aMaxElementSize  [OUT] if not null, the max element size is computed and returned in this param
    * @param aComputedWidth   the computed size of the table
    */
  virtual PRBool Initialize(nsIPresContext*          aPresContext,
                            nsSize*                  aMaxElementSize, 
                            nscoord                  aComputedWidth,
                            const nsHTMLReflowState& aBorderPadding)=0;

  /** compute the max-element-size for the table
    * @param aMaxElementSize  [OUT] width field set to the min legal width of the table
    */
  virtual void SetMaxElementSize(nsSize*         aMaxElementSize,
                                 const nsMargin& aPadding)=0;

  /** assign widths for each column, taking into account the table content, the effective style, 
    * the layout constraints, and the compatibility mode.  Sets mColumnWidths as a side effect.
    * @param aTableStyle      the resolved style for the table
    * @param aReflowState     the reflow state for the calling table frame
    * @param aMaxWidth        the width constraint

    */
  virtual PRBool BalanceColumnWidths(nsIPresContext*          aPresContext,
                                     nsIStyleContext*         aTableStyle,
                                     const nsHTMLReflowState& aReflowState,
                                     nscoord                  aMaxWidth)=0;

  /** return the computed max "natural" size of the table. 
    * this is the sum of the desired size of the content taking into account table
    * attributes, but NOT factoring in the available size the table is laying out into.
    * the actual table width in a given situation will depend on the available size
    * provided by the parent (especially for percent-width tables.)
    */
  virtual nscoord GetTableMaxWidth(const nsHTMLReflowState& aReflowState) const = 0;

  /** return the computed minimum possible size of the table. 
    * this is the sum of the minimum sizes of the content taking into account table
    * attributes, but NOT factoring in the available size the table is laying out into.
    * the actual table width in a given situation will depend on the available size
    * provided by the parent (especially for percent-width tables.)
    */
  virtual nscoord GetTableMinWidth() const = 0;

  /** return the value of the COLS attribute, used for balancing column widths */
  virtual nscoord GetCOLSAttribute() const = 0;

  // see nsTableFrame::ColumnsCanBeInvalidatedBy
  virtual PRBool ColumnsCanBeInvalidatedBy(nsStyleCoord*           aPrevStyleWidth,
                                           const nsTableCellFrame& aCellFrame) const = 0;

  // see nsTableFrame::ColumnsCanBeInvalidatedBy
  virtual PRBool ColumnsCanBeInvalidatedBy(const nsTableCellFrame& aCellFrame,
                                           PRBool                  aConsiderMinWidth = PR_FALSE) const = 0;

  // see nsTableFrame::ColumnsCanBeInvalidatedBy
  virtual PRBool ColumnsAreValidFor(const nsTableCellFrame& aCellFrame,
                                    nscoord                 aPrevCellMin,
                                    nscoord                 aPrevCellDes) const = 0;

#ifdef DEBUG
  virtual void SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const = 0;
#endif
};

#endif

