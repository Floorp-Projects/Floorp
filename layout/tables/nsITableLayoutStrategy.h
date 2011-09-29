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

    /** Implement nsIFrame::GetMinWidth for the table */
    virtual nscoord GetMinWidth(nsRenderingContext* aRenderingContext) = 0;

    /** Implement nsIFrame::GetPrefWidth for the table */
    virtual nscoord GetPrefWidth(nsRenderingContext* aRenderingContext,
                                 bool aComputingSize) = 0;

    /** Implement nsIFrame::MarkIntrinsicWidthsDirty for the table */
    virtual void MarkIntrinsicWidthsDirty() = 0;

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
    nsITableLayoutStrategy(Type aType) : mType(aType) {}
private:
    Type mType;
};

#endif /* !defined(nsITableLayoutStrategy_h_) */
