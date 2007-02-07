/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TableBackgroundPainter interface.
 *
 * The Initial Developer of the Original Code is
 * Elika J. Etemad ("fantasai") <fantasai@inkedblade.net>.
 * Portions created by the Initial Developer are Copyright (C) 2004
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsTablePainter_h__
#define nsTablePainter_h__

#include "celldata.h"

// flags for Paint, PaintChild, PaintChildren are currently only used by tables.
//Table-based paint call; not a direct call as with views
#define NS_PAINT_FLAG_TABLE_BG_PAINT      0x00000001
//Cells should paint their backgrounds only, no children
#define NS_PAINT_FLAG_TABLE_CELL_BG_PASS  0x00000002

#include "nsIFrame.h"
class nsTableFrame;
class nsTableRowGroupFrame;
class nsTableRowFrame;
class nsTableCellFrame;

class TableBackgroundPainter
{
  /*
   * Helper class for painting table backgrounds
   *
   */

  public:

    enum Origin { eOrigin_Table, eOrigin_TableRowGroup, eOrigin_TableRow };

    /** Public constructor
      * @param aTableFrame       - the table's table frame
      * @param aOrigin           - what type of table frame is creating this instance
      * @param aPresContext      - the presentation context
      * @param aRenderingContext - the rendering context
      * @param aDirtyRect        - the area that needs to be painted
      */
    TableBackgroundPainter(nsTableFrame*        aTableFrame,
                           Origin               aOrigin,
                           nsPresContext*       aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect);

    /** Destructor */
    ~TableBackgroundPainter();

    /* ~*~ The Border Collapse Painting Issue ~*~

       In border-collapse, the *table* paints the cells' borders,
       so we need to make sure the backgrounds get painted first
       (underneath) by doing a cell-background-only painting pass.
    */

    /* ~*~ Using nsTablePainter Background Painting ~*~

       A call to PaintTable will normally paint all of the table's
       elements (except the cells in non-BC). Elements with views
       however, will be skipped and must create their own painter
       to call the appropriate paint function in their ::Paint
       method (e.g. painter.PaintRow in nsTableRow::Paint)
    */

    /** Paint background for the table frame and its children down through cells
      * (Cells themselves will only be painted in border collapse)
      * Table must do a flagged TABLE_BG_PAINT ::Paint call on its
      * children afterwards
      * @param aTableFrame - the table frame
      * @param aDeflate    - deflation needed to bring table's mRect
      *                      to the outer grid lines in border-collapse
      */
    nsresult PaintTable(nsTableFrame* aTableFrame, nsMargin* aDeflate);

    /** Paint background for the row group and its children down through cells
      * (Cells themselves will only be painted in border collapse)
      * Standards mode only
      * Table Row Group must do a flagged TABLE_BG_PAINT ::Paint call on its
      * children afterwards
      * @param aFrame - the table row group frame
      */
    nsresult PaintRowGroup(nsTableRowGroupFrame* aFrame)
    { return PaintRowGroup(aFrame, PR_FALSE); }

    /** Paint background for the row and its children down through cells
      * (Cells themselves will only be painted in border collapse)
      * Standards mode only
      * Table Row must do a flagged TABLE_BG_PAINT ::Paint call on its
      * children afterwards
      * @param aFrame - the table row frame
      */
    nsresult PaintRow(nsTableRowFrame* aFrame)
    { return PaintRow(aFrame, PR_FALSE); }

  private:

    /** Paint table frame's background
      * @param aTableFrame     - the table frame
      * @param aFirstRowGroup  - the first (in layout order) row group
      *                          may be null
      * @param aLastRowGroup   - the last (in layout order) row group
      *                          may be null
      * @param aDeflate        - adjustment to frame's rect (used for quirks BC)
      *                          may be null
      */
    nsresult PaintTableFrame(nsTableFrame*         aTableFrame,
                             nsTableRowGroupFrame* aFirstRowGroup,
                             nsTableRowGroupFrame* aLastRowGroup,
                             nsMargin*             aDeflate = nsnull);

    /* aPassThrough params indicate whether to paint the element or to just
     * pass through and paint underlying layers only
     * See Public versions for function descriptions
     */
    nsresult PaintRowGroup(nsTableRowGroupFrame* aFrame,
                           PRBool                aPassThrough);
    nsresult PaintRow(nsTableRowFrame* aFrame,
                      PRBool           aPassThrough);

    /** Paint table background layers for this cell space
      * Also paints cell's own background in border-collapse mode
      * @param aFrame      - the cell
      * @param aPassSelf   - pass this cell; i.e. paint only underlying layers
      */
    nsresult PaintCell(nsTableCellFrame* aFrame,
                       PRBool            aPassSelf);

    /** Translate mRenderingContext, mDirtyRect, and mCols' column and
      * colgroup coords
      * @param aDX - origin's x-coord change
      * @param aDY - origin's y-coord change
      */
    void TranslateContext(nscoord aDX,
                          nscoord aDY);

    struct TableBackgroundData;
    friend struct TableBackgroundData;
    struct TableBackgroundData {
      nsIFrame*                 mFrame;
      /** mRect is the rect of mFrame in the current coordinate system */
      nsRect                    mRect;
      const nsStyleBackground*  mBackground;
      const nsStyleBorder*      mBorder;

      /** Data is valid & frame is visible */
      PRBool IsVisible() const { return mBackground != nsnull; }

      /** Constructor */
      TableBackgroundData();
      /** Destructor */
      ~TableBackgroundData();
      /** Destroys synthesized data. MUST be called before destructor
       *  @param aPresContext - the pres context
       */
      void Destroy(nsPresContext* aPresContext);


      /** Clear background data */
      void Clear();

      /** Calculate and set all data values to represent aFrame */
      void SetFull(nsIFrame* aFrame);

      /** Set frame data (mFrame, mRect) but leave style data empty */
      void SetFrame(nsIFrame* aFrame);

      /** Calculate the style data for mFrame */
      void SetData();

      /** True if need to set border-collapse border; must call SetFull beforehand */
      PRBool ShouldSetBCBorder();

      /** Set border-collapse border with aBorderWidth as widths */
      nsresult SetBCBorder(nsMargin&               aBorderWidth,
                           TableBackgroundPainter* aPainter);

      private:
      nsStyleBorder* mSynthBorder;
    };

    struct ColData;
    friend struct ColData;
    struct ColData {
      TableBackgroundData  mCol;
      TableBackgroundData* mColGroup; //link to col's parent colgroup's data (owned by painter)
      ColData() {
        mColGroup = nsnull;
      }
    };

    nsPresContext*      mPresContext;
    nsIRenderingContext& mRenderingContext;
    nsRect               mDirtyRect;
#ifdef DEBUG
    nsCompatibility      mCompatMode;
#endif
    PRBool               mIsBorderCollapse;
    Origin               mOrigin; //user's table frame type

    ColData*             mCols;  //array of columns' ColData
    PRUint32             mNumCols;
    TableBackgroundData  mRowGroup; //current row group
    TableBackgroundData  mRow;      //current row
    nsRect               mCellRect; //current cell's rect


    nsStyleBorder        mZeroBorder;  //cached zero-width border
    nsStylePadding       mZeroPadding; //cached zero-width padding

};

#endif
