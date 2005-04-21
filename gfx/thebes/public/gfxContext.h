/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include <cairo.h>

#include "gfxColor.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "gfxTypes.h"

class gfxASurface;
class gfxMatrix;
class gfxRegion;
class gfxFilter;
class gfxTextRun;
class gfxPattern;

class gfxContext {
public:
    gfxContext();
    ~gfxContext();

    // <insert refcount stuff here>
    // XXX document ownership
    // XXX can you call SetSurface more than once?
    // What happens to the current drawing state if you do that? or filters?
    // XXX need a fast way to save and restore the current translation as
    // we traverse the frame hierarchy. adding and subtracting an offset
    // can create inconsistencies due to FP phenomena.
    void SetSurface(gfxASurface* surface);
    gfxASurface* CurrentSurface();

    // state
    void Save();
    void Restore();

    // drawing
    // XXX are these the only things that affect the current path?
    void NewPath();
    void ClosePath();

    void Stroke();
    void Fill();
    
    void MoveTo(gfxPoint pt);
    void LineTo(gfxPoint pt);
    void CurveTo(gfxPoint pt1, gfxPoint pt2, gfxPoint pt3);

    void Arc(gfxPoint center, gfxFloat radius,
             gfxFloat angle1, gfxFloat angle2);

    void Rectangle(gfxRect rect);
    void Polygon(const gfxPoint points[], unsigned long numPoints);

    // Add the text outline to the current path
    void AddStringToPath(gfxTextRun& text, int pos, int len);

    // XXX These next two don't affect the current path?
    // XXX document 'size'
    void DrawSurface(gfxASurface *surface, gfxSize size);

    // Draw a substring of the text run at the current point
    void DrawString(gfxTextRun& text, int pos, int len);

    // transform stuff
    void Translate(gfxPoint pt);
    void Scale(gfxFloat x, gfxFloat y);
    void Rotate(gfxFloat angle);

    void SetMatrix(const gfxMatrix& matrix);
    gfxMatrix CurrentMatrix() const;

    // properties
    void SetColor(const gfxRGBA& c);
    gfxRGBA CurrentColor() const;

    void SetDash(gfxFloat* dashes, int ndash, gfxFloat offset);
    //void getDash() const;

    void SetLineWidth(gfxFloat width);
    gfxFloat CurrentLineWidth() const;

    // define enum for operators (clear, src, dst, etc)
    enum GraphicsOperator {
        OPERATOR_CLEAR,
        OPERATOR_SRC,
        OPERATOR_DST,
        OPERATOR_OVER,
        OPERATOR_OVER_REVERSE,
        OPERATOR_IN,
        OPERATOR_IN_REVERSE,
        OPERATOR_OUT,
        OPERATOR_OUT_REVERSE,
        OPERATOR_ATOP,
        OPERATOR_ATOP_REVERSE,
        OPERATOR_XOR,
        OPERATOR_ADD,
        OPERATOR_SATURATE
    };
    void SetOperator(GraphicsOperator op);
    GraphicsOperator CurrentOperator() const;

    /**
     * MODE_ALIASED means that only pixels whose centers are in the drawn area
     * should be modified, and they should be modified to take the value drawn
     * at the pixel center.
     */
    enum AntialiasMode {
        MODE_ALIASED,
        MODE_COVERAGE
    };
    void SetAntialiasMode(AntialiasMode mode);
    AntialiasMode CurrentAntialiasMode();

    enum GraphicsLineCap {
        LINE_CAP_BUTT,
        LINE_CAP_ROUND,
        LINE_CAP_SQUARE
    };
    void SetLineCap(GraphicsLineCap cap);
    GraphicsLineCap CurrentLineCap() const;

    enum GraphicsLineJoin {
        LINE_JOIN_MITER,
        LINE_JOIN_ROUND,
        LINE_JOIN_BEVEL
    };
    void SetLineJoin(GraphicsLineJoin join);
    GraphicsLineJoin CurrentLineJoin() const;

    void SetMiterLimit(gfxFloat limit);
    gfxFloat CurrentMiterLimit() const;


    // clipping
    void Clip(const gfxRect& rect); // will clip to a rect
    void Clip(const gfxRegion& region); // will clip to a region
    void Clip(); // will clip the last path you've drawn
    void ResetClip();

    // patterns
    void SetPattern(gfxPattern& pattern);

    // filters
    // Start rendering under the filter. We guarantee not to draw outside 'maxArea'.
    enum FilterHints {
        // Future drawing will completely cover the specified maxArea
        FILTER_OPAQUE_DRAW
    };
    void PushFilter(gfxFilter& filter, FilterHints hints, gfxRect& maxArea);

    // Completed rendering under the filter, composite what we rendered back to the
    // underlying surface using the filter.
    void PopFilter();

private:
    cairo_t *mCairo;
};

#endif /* GFX_CONTEXT_H */
