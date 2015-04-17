/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Web-compatible algorithms that determine column and table widths,
 * used for CSS2's 'table-layout: auto'.
 */

#ifndef BasicTableLayoutStrategy_h_
#define BasicTableLayoutStrategy_h_

#include "mozilla/Attributes.h"
#include "nsITableLayoutStrategy.h"

class nsTableFrame;

class BasicTableLayoutStrategy : public nsITableLayoutStrategy
{
public:
    explicit BasicTableLayoutStrategy(nsTableFrame *aTableFrame);
    virtual ~BasicTableLayoutStrategy();

    // nsITableLayoutStrategy implementation
    virtual nscoord GetMinISize(nsRenderingContext* aRenderingContext) override;
    virtual nscoord GetPrefISize(nsRenderingContext* aRenderingContext,
                                 bool aComputingSize) override;
    virtual void MarkIntrinsicISizesDirty() override;
    virtual void ComputeColumnWidths(const nsHTMLReflowState& aReflowState) override;

private:
    // NOTE: Using prefix "BTLS" to avoid overlapping names with
    // the values of nsLayoutUtils::IntrinsicISizeType
    enum BtlsWidthType { BTLS_MIN_WIDTH,
                         BTLS_PREF_WIDTH,
                         BTLS_FINAL_WIDTH };

    // Compute intrinsic width member variables on the columns.
    void ComputeColumnIntrinsicISizes(nsRenderingContext* aRenderingContext);

    // Distribute a colspanning cell's percent width (if any) to its columns.
    void DistributePctWidthToColumns(float aSpanPrefPct,
                                     int32_t aFirstCol,
                                     int32_t aColCount);

    // Distribute a width of some BltsWidthType type to a set of columns.
    //  aWidth: The amount of width to be distributed
    //  aFirstCol: The index (in the table) of the first column to be
    //             considered for receiving width
    //  aColCount: The number of consecutive columns (starting with aFirstCol)
    //             to be considered for receiving width
    //  aWidthType: The type of width being distributed.  (BTLS_MIN_WIDTH and
    //              BTLS_PREF_WIDTH are intended to be used for dividing up
    //              colspan's min & pref width.  BTLS_FINAL_WIDTH is intended
    //              to be used for distributing the table's final width across
    //              all its columns)
    //  aSpanHasSpecifiedWidth: Should be true iff:
    //                           - We're distributing a colspanning cell's
    //                             pref or min width to its columns
    //                           - The colspanning cell has a specified width.
    void DistributeWidthToColumns(nscoord aWidth,
                                  int32_t aFirstCol,
                                  int32_t aColCount,
                                  BtlsWidthType aWidthType,
                                  bool aSpanHasSpecifiedWidth);


    // Compute the min and pref widths of the table from the width
    // variables on the columns.
    void ComputeIntrinsicISizes(nsRenderingContext* aRenderingContext);

    nsTableFrame *mTableFrame;
    nscoord mMinWidth;
    nscoord mPrefWidth;
    nscoord mPrefWidthPctExpand;
    nscoord mLastCalcWidth;
};

#endif /* !defined(BasicTableLayoutStrategy_h_) */
