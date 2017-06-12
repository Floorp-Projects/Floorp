/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Web-compatible algorithms that determine column and table isizes,
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
    virtual void ComputeColumnISizes(const ReflowInput& aReflowInput) override;

private:
    // NOTE: Using prefix "BTLS" to avoid overlapping names with
    // the values of nsLayoutUtils::IntrinsicISizeType
    enum BtlsISizeType { BTLS_MIN_ISIZE,
                         BTLS_PREF_ISIZE,
                         BTLS_FINAL_ISIZE };

    // Compute intrinsic isize member variables on the columns.
    void ComputeColumnIntrinsicISizes(nsRenderingContext* aRenderingContext);

    // Distribute a colspanning cell's percent isize (if any) to its columns.
    void DistributePctISizeToColumns(float aSpanPrefPct,
                                     int32_t aFirstCol,
                                     int32_t aColCount);

    // Distribute an isize of some BltsISizeType type to a set of columns.
    //  aISize: The amount of isize to be distributed
    //  aFirstCol: The index (in the table) of the first column to be
    //             considered for receiving isize
    //  aColCount: The number of consecutive columns (starting with aFirstCol)
    //             to be considered for receiving isize
    //  aISizeType: The type of isize being distributed.  (BTLS_MIN_ISIZE and
    //              BTLS_PREF_ISIZE are intended to be used for dividing up
    //              colspan's min & pref isize.  BTLS_FINAL_ISIZE is intended
    //              to be used for distributing the table's final isize across
    //              all its columns)
    //  aSpanHasSpecifiedISize: Should be true iff:
    //                           - We're distributing a colspanning cell's
    //                             pref or min isize to its columns
    //                           - The colspanning cell has a specified isize.
    void DistributeISizeToColumns(nscoord aISize,
                                  int32_t aFirstCol,
                                  int32_t aColCount,
                                  BtlsISizeType aISizeType,
                                  bool aSpanHasSpecifiedISize);


    // Compute the min and pref isizes of the table from the isize
    // variables on the columns.
    void ComputeIntrinsicISizes(nsRenderingContext* aRenderingContext);

    nsTableFrame *mTableFrame;
    nscoord mMinISize;
    nscoord mPrefISize;
    nscoord mPrefISizePctExpand;
    nscoord mLastCalcISize;
};

#endif /* !defined(BasicTableLayoutStrategy_h_) */
