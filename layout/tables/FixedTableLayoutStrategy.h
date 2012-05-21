/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Algorithms that determine column and table widths used for CSS2's
 * 'table-layout: fixed'.
 */

#ifndef FixedTableLayoutStrategy_h_
#define FixedTableLayoutStrategy_h_

#include "nsITableLayoutStrategy.h"

class nsTableFrame;

class FixedTableLayoutStrategy : public nsITableLayoutStrategy
{
public:
    FixedTableLayoutStrategy(nsTableFrame *aTableFrame);
    virtual ~FixedTableLayoutStrategy();

    // nsITableLayoutStrategy implementation
    virtual nscoord GetMinWidth(nsRenderingContext* aRenderingContext);
    virtual nscoord GetPrefWidth(nsRenderingContext* aRenderingContext,
                                 bool aComputingSize);
    virtual void MarkIntrinsicWidthsDirty();
    virtual void ComputeColumnWidths(const nsHTMLReflowState& aReflowState);

private:
    nsTableFrame *mTableFrame;
    nscoord mMinWidth;
    nscoord mLastCalcWidth;
};

#endif /* !defined(FixedTableLayoutStrategy_h_) */
