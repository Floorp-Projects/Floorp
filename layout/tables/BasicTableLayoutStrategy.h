/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
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
 * The Original Code is Mozilla's table layout code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
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

/*
 * Web-compatible algorithms that determine column and table widths,
 * used for CSS2's 'table-layout: auto'.
 */

#ifndef BasicTableLayoutStrategy_h_
#define BasicTableLayoutStrategy_h_

#include "nsITableLayoutStrategy.h"

class nsTableFrame;

class BasicTableLayoutStrategy : public nsITableLayoutStrategy
{
public:
    BasicTableLayoutStrategy(nsTableFrame *aTableFrame);
    virtual ~BasicTableLayoutStrategy();

    // nsITableLayoutStrategy implementation
    virtual nscoord GetMinWidth(nsRenderingContext* aRenderingContext);
    virtual nscoord GetPrefWidth(nsRenderingContext* aRenderingContext,
                                 bool aComputingSize);
    virtual void MarkIntrinsicWidthsDirty();
    virtual void ComputeColumnWidths(const nsHTMLReflowState& aReflowState);

private:
    // NOTE: Using prefix "BTLS" to avoid overlapping names with 
    // the values of nsLayoutUtils::IntrinsicWidthType
    enum BtlsWidthType { BTLS_MIN_WIDTH, 
                         BTLS_PREF_WIDTH, 
                         BTLS_FINAL_WIDTH };

    // Compute intrinsic width member variables on the columns.
    void ComputeColumnIntrinsicWidths(nsRenderingContext* aRenderingContext);

    // Distribute a colspanning cell's percent width (if any) to its columns.
    void DistributePctWidthToColumns(float aSpanPrefPct,
                                     PRInt32 aFirstCol,
                                     PRInt32 aColCount);

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
    //  aSpanHasSpecifiedWidth: Should be PR_TRUE iff:
    //                           - We're distributing a colspanning cell's
    //                             pref or min width to its columns
    //                           - The colspanning cell has a specified width.
    void DistributeWidthToColumns(nscoord aWidth, 
                                  PRInt32 aFirstCol, 
                                  PRInt32 aColCount,
                                  BtlsWidthType aWidthType,
                                  bool aSpanHasSpecifiedWidth);
 

    // Compute the min and pref widths of the table from the width
    // variables on the columns.
    void ComputeIntrinsicWidths(nsRenderingContext* aRenderingContext);

    nsTableFrame *mTableFrame;
    nscoord mMinWidth;
    nscoord mPrefWidth;
    nscoord mPrefWidthPctExpand;
    nscoord mLastCalcWidth;
};

#endif /* !defined(BasicTableLayoutStrategy_h_) */
