/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * interface for the set of algorithms that determine column and table
 * widths
 */

#ifndef nsITableLayoutStrategy_h_
#define nsITableLayoutStrategy_h_

#include "nscore.h"
#include "nsCoord.h"

class nsRenderingContext;
struct nsHTMLReflowState;

class nsITableLayoutStrategy
{
public:
    virtual ~nsITableLayoutStrategy() {}

    /** Implement nsIFrame::GetMinISize for the table */
    virtual nscoord GetMinISize(nsRenderingContext* aRenderingContext) = 0;

    /** Implement nsIFrame::GetPrefISize for the table */
    virtual nscoord GetPrefISize(nsRenderingContext* aRenderingContext,
                                 bool aComputingSize) = 0;

    /** Implement nsIFrame::MarkIntrinsicISizesDirty for the table */
    virtual void MarkIntrinsicISizesDirty() = 0;

    /**
     * Compute final column widths based on the intrinsic width data and
     * the available width.
     */
    virtual void ComputeColumnWidths(const nsHTMLReflowState& aReflowState) = 0;

    /**
     * Return the type of table layout strategy, without the cost of
     * a virtual function call
     */
    enum Type { Auto, Fixed };
    Type GetType() const { return mType; }

protected:
    explicit nsITableLayoutStrategy(Type aType) : mType(aType) {}
private:
    Type mType;
};

#endif /* !defined(nsITableLayoutStrategy_h_) */
