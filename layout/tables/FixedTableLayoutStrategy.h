/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Algorithms that determine column and table isizes used for CSS2's
 * 'table-layout: fixed'.
 */

#ifndef FixedTableLayoutStrategy_h_
#define FixedTableLayoutStrategy_h_

#include "mozilla/Attributes.h"
#include "nsITableLayoutStrategy.h"

class nsTableFrame;

class FixedTableLayoutStrategy : public nsITableLayoutStrategy
{
public:
    explicit FixedTableLayoutStrategy(nsTableFrame *aTableFrame);
    virtual ~FixedTableLayoutStrategy();

    // nsITableLayoutStrategy implementation
    virtual nscoord GetMinISize(nsRenderingContext* aRenderingContext) override;
    virtual nscoord GetPrefISize(nsRenderingContext* aRenderingContext,
                                 bool aComputingSize) override;
    virtual void MarkIntrinsicISizesDirty() override;
    virtual void ComputeColumnISizes(const nsHTMLReflowState& aReflowState) override;

private:
    nsTableFrame *mTableFrame;
    nscoord mMinWidth;
    nscoord mLastCalcWidth;
};

#endif /* !defined(FixedTableLayoutStrategy_h_) */
