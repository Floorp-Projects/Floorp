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
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "gfxASurface.h"
#include "gfxColor.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "gfxTypes.h"
#include "gfxMatrix.h"
#include "gfxPattern.h"

class gfxRegion;
class gfxFilter;
class gfxTextRun;

class gfxContext {
    THEBES_DECL_REFCOUNTING

public:
    gfxContext(gfxASurface *surface);
    ~gfxContext();

    // this does not addref
    gfxASurface* CurrentSurface();
    cairo_t* GetCairo() { return mCairo; }

    /**
     ** State
     **/
    // XXX document exactly what bits are saved
    void Save();
    void Restore();

    /**
     ** Paths & Drawing
     **/

    // these do not consume the current path
    void Stroke();
    void Fill();

    void NewPath();
    void ClosePath();

    void MoveTo(gfxPoint pt);
    void LineTo(gfxPoint pt);
    void CurveTo(gfxPoint pt1, gfxPoint pt2, gfxPoint pt3);

    // clockwise arc
    void Arc(gfxPoint center, gfxFloat radius,
             gfxFloat angle1, gfxFloat angle2);

    // counterclockwise arc
    void NegativeArc(gfxPoint center, gfxFloat radius,
                     gfxFloat angle1, gfxFloat angle2);

    // path helpers
    void Line(gfxPoint start, gfxPoint end); // XXX snapToPixels option?
    void Rectangle(gfxRect rect, PRBool snapToPixels = PR_FALSE);
    void Ellipse(gfxPoint center, gfxSize dimensions);
    void Polygon(const gfxPoint *points, PRUint32 numPoints);

    /**
     ** Text
     **/

    // Add the text outline to the current path
    void AddStringToPath(gfxTextRun& text, int pos, int len);

    // Draw a substring of the text run at the current point
    void DrawString(gfxTextRun& text, int pos, int len);

    /**
     ** Transformation Matrix manipulation
     **/

    void Translate(gfxPoint pt);
    void Scale(gfxFloat x, gfxFloat y);
    void Rotate(gfxFloat angle); // radians

    // post-multiplies 'other' onto the current CTM
    void Multiply(const gfxMatrix& other);

    void SetMatrix(const gfxMatrix& matrix);
    void IdentityMatrix();
    gfxMatrix CurrentMatrix() const;

    gfxPoint DeviceToUser(gfxPoint point) const;
    gfxSize DeviceToUser(gfxSize size) const;
    gfxPoint UserToDevice(gfxPoint point) const;
    gfxSize UserToDevice(gfxSize size) const;

    /**
     ** Painting sources
     **/

    void SetColor(const gfxRGBA& c);
    void SetPattern(gfxPattern *pattern);
    void SetSource(gfxASurface *surface) {
        SetSource(surface, gfxPoint(0, 0));
    }
    void SetSource(gfxASurface* surface, gfxPoint origin);

    /**
     ** Painting
     **/
    void Paint(gfxFloat alpha = 1.0);

    /**
     ** Shortcuts
     **/

    // Creates a new path with a rectangle from 0,0 to size.w,size.h
    // and calls cairo_fill
    void DrawSurface(gfxASurface *surface, gfxSize size);

    /**
     ** Line Properties
     **/

    typedef enum {
        gfxLineSolid,
        gfxLineDashed,
        gfxLineDotted
    } gfxLineType;

    void SetDash(gfxLineType ltype);
    void SetDash(gfxFloat *dashes, int ndash, gfxFloat offset);
    //void getDash() const;

    void SetLineWidth(gfxFloat width);
    gfxFloat CurrentLineWidth() const;

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

    /**
     ** Operators and Rendering control
     **/

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

    /**
     ** Clipping
     **/

    void Clip(); // will clip the last path you've drawn
    void ResetClip(); // will remove any clip set
    // Helper functions that will create a rect path and call Clip().
    // Any current path will be destroyed by these functions!
    //
    void Clip(gfxRect rect); // will clip to a rect
    void Clip(const gfxRegion& region); // will clip to a region

    /**
     ** Filters/Group Rendering
     ** XXX these aren't really "filters" and should be renamed properly.
     **/
    enum FilterHints {
        // Future drawing will completely cover the specified maxArea
        FILTER_OPAQUE_DRAW
    };

    // Start rendering under the filter. We guarantee not to draw outside 'maxArea'.
    void PushFilter(gfxFilter& filter, FilterHints hints, gfxRect& maxArea);

    // Completed rendering under the filter, composite what we rendered back to the
    // underlying surface using the filter.
    void PopFilter();

private:
    cairo_t *mCairo;
    nsRefPtr<gfxASurface> mSurface;
};

#endif /* GFX_CONTEXT_H */
