/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTablePainter_h__
#define nsTablePainter_h__

#include "imgIContainer.h"

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

  typedef mozilla::image::DrawResult DrawResult;

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
      * @returns DrawResult::SUCCESS if all painting was successful. If some
      *          painting failed or an improved result could be achieved by sync
      *          decoding images, returns another value.
      */
    DrawResult PaintTable(nsTableFrame* aTableFrame, const nsMargin& aDeflate,
                          bool aPaintTableBackground);

    /** Paint background for the row group and its children down through cells
      * (Cells themselves will only be painted in border collapse)
      * Standards mode only
      * Table Row Group must do a flagged TABLE_BG_PAINT ::Paint call on its
      * children afterwards
      * @param aFrame - the table row group frame
      * @returns DrawResult::SUCCESS if all painting was successful. If some
      *          painting failed or an improved result could be achieved by sync
      *          decoding images, returns another value.
      */
    DrawResult PaintRowGroup(nsTableRowGroupFrame* aFrame);

    /** Paint background for the row and its children down through cells
      * (Cells themselves will only be painted in border collapse)
      * Standards mode only
      * Table Row must do a flagged TABLE_BG_PAINT ::Paint call on its
      * children afterwards
      * @param aFrame - the table row frame
      * @returns DrawResult::SUCCESS if all painting was successful. If some
      *          painting failed or an improved result could be achieved by sync
      *          decoding images, returns another value.
      */
    DrawResult PaintRow(nsTableRowFrame* aFrame);

  private:
    struct TableBackgroundData;

    /** Paint table frame's background
      * @param aTableFrame     - the table frame
      * @param aFirstRowGroup  - the first (in layout order) row group
      *                          may be null
      * @param aLastRowGroup   - the last (in layout order) row group
      *                          may be null
      * @param aDeflate        - adjustment to frame's rect (used for quirks BC)
      *                          may be null
      */
    DrawResult PaintTableFrame(nsTableFrame*         aTableFrame,
                               nsTableRowGroupFrame* aFirstRowGroup,
                               nsTableRowGroupFrame* aLastRowGroup,
                               const nsMargin&       aDeflate);

    /* aPassThrough params indicate whether to paint the element or to just
     * pass through and paint underlying layers only.
     * aRowGroupBGData is not a const reference because the function modifies
     * its copy. Same for aRowBGData in PaintRow.
     * See Public versions for function descriptions
     */
    DrawResult PaintRowGroup(nsTableRowGroupFrame* aFrame,
                             TableBackgroundData   aRowGroupBGData,
                             bool                  aPassThrough);

    DrawResult PaintRow(nsTableRowFrame* aFrame,
                        const TableBackgroundData& aRowGroupBGData,
                        TableBackgroundData aRowBGData,
                        bool             aPassThrough);

    /** Paint table background layers for this cell space
      * Also paints cell's own background in border-collapse mode
      * @param aCell           - the cell
      * @param aRowGroupBGData - background drawing info for the row group
      * @param aRowBGData      - background drawing info for the row
      * @param aCellBGRect     - background rect for the cell
      * @param aRowBGRect      - background rect for the row
      * @param aRowGroupBGRect - background rect for the row group
      * @param aColBGRect      - background rect for the column and column group
      * @param aPassSelf       - pass this cell; i.e. paint only underlying layers
      */
    DrawResult PaintCell(nsTableCellFrame* aCell,
                         const TableBackgroundData& aRowGroupBGData,
                         const TableBackgroundData& aRowBGData,
                         nsRect&           aCellBGRect,
                         nsRect&           aRowBGRect,
                         nsRect&           aRowGroupBGRect,
                         nsRect&           aColBGRect,
                         bool              aPassSelf);

    /** Compute table background layer positions for this cell space
      * @param aCell              - the cell
      * @param aRowGroupBGData    - background drawing info for the row group
      * @param aRowBGData         - background drawing info for the row
      * @param aCellBGRectOut     - outparam: background rect for the cell
      * @param aRowBGRectOut      - outparam: background rect for the row
      * @param aRowGroupBGRectOut - outparam: background rect for the row group
      * @param aColBGRectOut      - outparam: background rect for the column
                                    and column group
      */
    void ComputeCellBackgrounds(nsTableCellFrame* aCell,
                                const TableBackgroundData& aRowGroupBGData,
                                const TableBackgroundData& aRowBGData,
                                nsRect&           aCellBGRect,
                                nsRect&           aRowBGRect,
                                nsRect&           aRowGroupBGRect,
                                nsRect&           aColBGRect);

    /** Translate mRenderingContext, mDirtyRect, and mCols' column and
      * colgroup coords
      * @param aDX - origin's x-coord change
      * @param aDY - origin's y-coord change
      */
    void TranslateContext(nscoord aDX,
                          nscoord aDY);

    struct TableBackgroundData {
    public:
      /**
       * Construct an empty TableBackgroundData instance, which is invisible.
       */
      TableBackgroundData();

      /**
       * Construct a TableBackgroundData instance for a frame. Visibility will
       * be derived from the frame and can be overridden using MakeInvisible().
       */
      explicit TableBackgroundData(nsIFrame* aFrame);

      /** Destructor */
      ~TableBackgroundData() {}

      /** Data is valid & frame is visible */
      bool IsVisible() const { return mVisible; }

      /** Override visibility of the frame, force it to be invisible */
      void MakeInvisible() { mVisible = false; }

      /** True if need to set border-collapse border; must call SetFull beforehand */
      bool ShouldSetBCBorder() const;

      /** Set border-collapse border with aBorderWidth as widths */
      void SetBCBorder(const nsMargin& aBorderWidth);

      /**
       * @param  aZeroBorder An nsStyleBorder instance that has been initialized
       *                     for the right nsPresContext, with all border widths
       *                     set to zero and border styles set to solid.
       * @return             The nsStyleBorder that should be used for rendering
       *                     this background.
       */
      nsStyleBorder StyleBorder(const nsStyleBorder& aZeroBorder) const;

      nsIFrame* const mFrame;

      /** mRect is the rect of mFrame in the current coordinate system */
      nsRect mRect;

    private:
      nsMargin mSynthBorderWidths;
      bool mVisible;
      bool mUsesSynthBorder;
    };

    struct ColData {
      ColData(nsIFrame* aFrame, TableBackgroundData& aColGroupBGData);
      TableBackgroundData mCol;
      TableBackgroundData& mColGroup; // reference to col's parent colgroup's data, owned by TablePainter in mColGroups
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

    nsTArray<TableBackgroundData> mColGroups;
    nsTArray<ColData>    mCols;
    size_t               mNumCols;

    nsStyleBorder        mZeroBorder;  //cached zero-width border
    uint32_t             mBGPaintFlags;
};

#endif
