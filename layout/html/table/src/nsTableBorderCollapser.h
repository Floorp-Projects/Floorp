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
#ifndef nsTableBorderCollapser_h__
#define nsTableBorderCollapser_h__
#if 0
#include "nsIStyleContext.h"
class nsTableFrame;

/**
  * Class for handling CSS border-style:collapse
  */
class nsTableBorderCollapser 
{
public:
  nsTableBorderCollapser(nsTableFrame& aTableFrame);

  ~nsTableBorderCollapser();

  /** notification that top and bottom borders have been computed */ 
  void DidComputeHorizontalBorders(nsIPresContext* aPresContext,
                                   PRInt32         aStartRowIndex,
                                   PRInt32         aEndRowIndex);

  /** compute the left and right collapsed borders between aStartRowIndex and aEndRowIndex, inclusive */
  void ComputeVerticalBorders(nsIPresContext* aPresContext,
                              PRInt32         aStartRowIndex, 
                              PRInt32          aEndRowIndex);

  /** compute the top and bottom collapsed borders between aStartRowIndex and aEndRowIndex, inclusive */
  void ComputeHorizontalBorders(nsIPresContext* aPresContext,
                                PRInt32         aStartRowIndex, 
                                PRInt32         aEndRowIndex);

  /** compute the left borders for the table objects intersecting at (aRowIndex, aColIndex) */
  void ComputeLeftBorderForEdgeAt(nsIPresContext* aPresContext,
                                  PRInt32         aRowIndex, 
                                  PRInt32         aColIndex);

  /** compute the right border for the table cell at (aRowIndex, aColIndex)
    * and the appropriate border for that cell's right neighbor 
    * (the left border for a neighboring cell, or the right table edge) 
    */
  void ComputeRightBorderForEdgeAt(nsIPresContext* aPresContext,
                                   PRInt32         aRowIndex, 
                                   PRInt32         aColIndex);

  /** compute the top borders for the table objects intersecting at (aRowIndex, aColIndex) */
  void ComputeTopBorderForEdgeAt(nsIPresContext* aPresContext,
                                 PRInt32         aRowIndex, 
                                 PRInt32         aColIndex);

  /** compute the bottom border for the table cell at (aRowIndex, aColIndex)
    * and the appropriate border for that cell's bottom neighbor 
    * (the top border for a neighboring cell, or the bottom table edge) 
    */
  void ComputeBottomBorderForEdgeAt(nsIPresContext* aPresContext,
                                    PRInt32         aRowIndex, 
                                    PRInt32         aColIndex);
  
  /** at the time we initially compute collapsing borders, we don't yet have the 
    * column widths.  So we set them as a post-process of the column balancing algorithm.
    */
  void SetHorizontalEdgeLengths();

  /** @return the identifier representing the edge opposite from aEdge (left-right, top-bottom) */
  PRUint8 GetOpposingEdge(PRUint8 aEdge);

  /** @return the computed width for aSide of aBorder */
  nscoord GetWidthForSide(const nsMargin& aBorder, 
	                      PRUint8         aSide);

  /** returns BORDER_PRECEDENT_LOWER if aStyle1 is lower precedent that aStyle2
    *         BORDER_PRECEDENT_HIGHER if aStyle1 is higher precedent that aStyle2
    *         BORDER_PRECEDENT_EQUAL if aStyle1 and aStyle2 have the same precedence
    *         (note, this is not necessarily the same as saying aStyle1==aStyle2)
    * according to the CSS-2 collapsing borders for tables precedent rules.
    */
  PRUint8 CompareBorderStyles(PRUint8 aStyle1, 
	                            PRUint8 aStyle2);

  /** helper to set the length of an edge for aSide border of this table frame */
  void SetBorderEdgeLength(PRUint8 aSide, 
                           PRInt32 aIndex, 
                           nscoord aLength);

  /** Compute the style, width, and color of an edge in a collapsed-border table.
    * This method is the CSS2 border conflict resolution algorithm
    * The spec says to resolve conflicts in this order:<br>
    * 1. any border with the style HIDDEN wins<br>
    * 2. the widest border with a style that is not NONE wins<br>
    * 3. the border styles are ranked in this order, highest to lowest precedence:<br>
    *       double, solid, dashed, dotted, ridge, outset, groove, inset<br>
    * 4. borders that are of equal width and style (differ only in color) have this precedence:<br>
    *       cell, row, rowgroup, col, colgroup, table<br>
    * 5. if all border styles are NONE, then that's the computed border style.<br>
    * This method assumes that the styles were added to aStyles in the reverse precedence order
    * of their frame type, so that styles that come later in the list win over style 
    * earlier in the list if the tie-breaker gets down to #4.
    * This method sets the out-param aBorder with the resolved border attributes
    *
    * @param aSide   the side that is being compared
    * @param aStyles the resolved styles of the table objects intersecting at aSide
    *                styles must be added to this list in reverse precedence order
    * @param aBorder [OUT] the border edge that we're computing.  Results of the computation
    *                      are stored in aBorder:  style, color, and width.
    * @param aFlipLastSide an indication of what the bordering object is:  another cell, or the table itself.
    */
  void ComputeBorderSegment(PRUint8       aSide, 
                            nsVoidArray*  aStyles, 
                            nsBorderEdge& aBorder,
                            PRBool        aFlipLastSide);

  void GetBorder(nsMargin& aBorder);

  void GetBorderAt(PRInt32   aRowIndex,
                   PRInt32   aColIndex,
                   nsMargin& aBorder);

  void GetMaxBorder(PRInt32  aStartRowIndex,
                    PRInt32  aEndRowIndex,
                    PRInt32  aStartColIndex,
                    PRInt32  aEndColIndex,
                    nsMargin aBorder);

  nsBorderEdges* GetEdges();

protected:
  nsBorderEdges mBorderEdges;
  nsTableFrame& mTableFrame;
};

inline nsBorderEdges* nsTableBorderCollapser::GetEdges() 
{
  return &mBorderEdges;
}

#endif
#endif






