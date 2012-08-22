/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTablePainter_h__
#define nsTablePainter_h__

#include "celldata.h"

// flags for Paint, PaintChild, PaintChildren are currently only used by tables.
//Table-based paint call; not a direct call as with views
#define NS_PAINT_FLAG_TABLE_BG_PAINT      0x00000001
//Cells should paint their backgrounds only, no children
#define NS_PAINT_FLAG_TABLE_CELL_BG_PASS  0x00000002

class nsIFrame;
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
      * @param aDirtyRect        - the area that needs to be painted,
      * relative to aRenderingContext
      * @param aPt               - offset of the table frame relative to
      * aRenderingContext
      * @param aBGPaintFlags - Flags of the nsCSSRendering::PAINTBG_* variety
      */
    TableBackgroundPainter(nsTableFrame*        aTableFrame,
                           Origin               aOrigin,
                           nsPresContext*       aPresContext,
                           nsRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           const nsPoint&       aPt,
                           uint32_t             aBGPaintFlags);

    /** Destructor */
    ~TableBackgroundPainter();

    /* ~*~ The Border Collapse Painting Issue ~*~

       In border-collapse, the *table* paints the cells' borders,
       so we need to make sure the backgrounds get painted first
       (underneath) by doing a cell-background-only painting pass.
    */

    /* ~*~ Using nsTablePainter Background Painting ~*~

       A call to PaintTable will normally paint all of the table's
       elements (except for the table background, if aPaintTableBackground
       is false).
       Elements with views however, will be skipped and must create their
       own painter to call the appropriate paint function in their ::Paint
       method (e.g. painter.PaintRow in nsTableRow::Paint)
    */

    /** Paint background for the table frame (if requested) and its children
      * down through cells.
      * (Cells themselves will only be painted in border collapse)
      * Table must do a flagged TABLE_BG_PAINT ::Paint call on its
      * children afterwards
      * @param aTableFrame - the table frame
      * @param aDeflate    - deflation needed to bring table's mRect
      *                      to the outer grid lines in border-collapse
      * @param aPaintTableBackground - if true, the table background
      * is included, otherwise it isn't
      */
    nsresult PaintTable(nsTableFrame* aTableFrame, const nsMargin& aDeflate,
                        bool aPaintTableBackground);

    /** Paint background for the row group and its children down through cells
      * (Cells themselves will only be painted in border collapse)
      * Standards mode only
      * Table Row Group must do a flagged TABLE_BG_PAINT ::Paint call on its
      * children afterwards
      * @param aFrame - the table row group frame
      */
    nsresult PaintRowGroup(nsTableRowGroupFrame* aFrame)
    { return PaintRowGroup(aFrame, false); }

    /** Paint background for the row and its children down through cells
      * (Cells themselves will only be painted in border collapse)
      * Standards mode only
      * Table Row must do a flagged TABLE_BG_PAINT ::Paint call on its
      * children afterwards
      * @param aFrame - the table row frame
      */
    nsresult PaintRow(nsTableRowFrame* aFrame)
    { return PaintRow(aFrame, false); }

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
                             const nsMargin&       aDeflate);

    /* aPassThrough params indicate whether to paint the element or to just
     * pass through and paint underlying layers only
     * See Public versions for function descriptions
     */
    nsresult PaintRowGroup(nsTableRowGroupFrame* aFrame,
                           bool                  aPassThrough);
    nsresult PaintRow(nsTableRowFrame* aFrame,
                      bool             aPassThrough);

    /** Paint table background layers for this cell space
      * Also paints cell's own background in border-collapse mode
      * @param aFrame      - the cell
      * @param aPassSelf   - pass this cell; i.e. paint only underlying layers
      */
    nsresult PaintCell(nsTableCellFrame* aFrame,
                       bool              aPassSelf);

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
      bool                      mVisible;
      const nsStyleBorder*      mBorder;

      /** Data is valid & frame is visible */
      bool IsVisible() const { return mVisible; }

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
      bool ShouldSetBCBorder();

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
        mColGroup = nullptr;
      }
    };

    nsPresContext*      mPresContext;
    nsRenderingContext& mRenderingContext;
    nsPoint              mRenderPt;
    nsRect               mDirtyRect;
#ifdef DEBUG
    nsCompatibility      mCompatMode;
#endif
    bool                 mIsBorderCollapse;
    Origin               mOrigin; //user's table frame type

    ColData*             mCols;  //array of columns' ColData
    uint32_t             mNumCols;
    TableBackgroundData  mRowGroup; //current row group
    TableBackgroundData  mRow;      //current row
    nsRect               mCellRect; //current cell's rect

    nsStyleBorder        mZeroBorder;  //cached zero-width border
    uint32_t             mBGPaintFlags;
};

#endif
