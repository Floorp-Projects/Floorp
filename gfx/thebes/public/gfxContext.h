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

#ifndef GFXCONTEXT_H
#define GFXCONTEXT_H

#include <cairo.h>

#include "gfxColor.h"
#include "gfxPattern.h"
#include "gfxPoint.h"
#include "gfxRect.h"

class gfxASurface;
class gfxMatrix;
class gfxRegion;
class gfxFilter;
class gfxTextRun;

class gfxContext {
public:
    gfxContext();
    ~gfxContext();

    // <insert refcount stuff here>

    void SetSurface(gfxASurface* surface);
    gfxASurface* CurrentSurface();

    // state
    void Save();
    void Restore();

    // drawing
    void NewPath();
    void ClosePath();

    void Stroke();
    void Fill();
    
    void MoveTo(double x, double y);
    void LineTo(double x, double y);
    void CurveTo(double x1, double y1,
                 double x2, double y2,
                 double x3, double y3);

    void Arc(double xc, double yc,
             double radius,
             double angle1, double angle2);

    void Rectangle(double x, double y, double width, double height);
    void Polygon(const gfxPoint points[], unsigned long numPoints);

    void DrawSurface(gfxASurface *surface, double width, double height);

    // transform stuff
    void Translate(double x, double y);
    void Scale(double x, double y);
    void Rotate(double angle);

    void SetMatrix(const gfxMatrix& matrix);
    gfxMatrix CurrentMatrix() const;


    // properties
    void SetColor(const gfxRGBA& c);
    gfxRGBA CurrentColor() const;

    void SetDash(double* dashes, int ndash, double offset);
    //void getDash() const;

    void SetLineWidth(double width);
    double CurrentLineWidth() const;

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

    void SetMiterLimit(double limit);
    double CurrentMiterLimit() const;


    // clipping
    void Clip(const gfxRect& rect); // will clip to a rect
    void Clip(const gfxRegion& region); // will clip to a region
    void Clip(); // will clip the last path you've drawn
    void ResetClip();

    // patterns
    void SetPattern(gfxPattern& pattern);

    // fonts?
    void DrawString(gfxTextRun& text, int pos, int len);

    // filters
    // Start rendering under the filter. We guarantee not to draw outside 'maxArea'.
    void PushFilter(gfxFilter& filter, gfxRect& maxArea);

    // Completed rendering under the filter, composite what we rendered back to the
    // underlying surface using the filter.
    void PopFilter();

private:
    cairo_t *mCairo;
};

#endif /* GFXCONTEXT_H */
