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

#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#include "gfxContext.h"

#include "gfxColor.h"
#include "gfxMatrix.h"
#include "gfxASurface.h"
#include "gfxPattern.h"


THEBES_IMPL_REFCOUNTING(gfxContext)

gfxContext::gfxContext(gfxASurface *surface) :
    mSurface(surface)
{
    mCairo = cairo_create(surface->CairoSurface());
}
gfxContext::~gfxContext()
{
    cairo_destroy(mCairo);
}

gfxASurface *gfxContext::CurrentSurface()
{
    return mSurface;
}

void gfxContext::Save()
{
    cairo_save(mCairo);
}

void gfxContext::Restore()
{
    cairo_restore(mCairo);
}

// drawing
void gfxContext::NewPath()
{
    cairo_new_path(mCairo);
}
void gfxContext::ClosePath()
{
    cairo_close_path(mCairo);
}

void gfxContext::Stroke()
{
    cairo_stroke_preserve(mCairo);
}
void gfxContext::Fill()
{
    cairo_fill_preserve(mCairo);
}

void gfxContext::MoveTo(gfxPoint pt)
{
    cairo_move_to(mCairo, pt.x, pt.y);
}
void gfxContext::LineTo(gfxPoint pt)
{
    cairo_line_to(mCairo, pt.x, pt.y);
}

void gfxContext::CurveTo(gfxPoint pt1, gfxPoint pt2, gfxPoint pt3)
{
    cairo_curve_to(mCairo, pt1.x, pt1.y, pt2.x, pt2.y, pt3.x, pt3.y);
}

void gfxContext::Arc(gfxPoint center, gfxFloat radius,
                     gfxFloat angle1, gfxFloat angle2)
{
    cairo_arc(mCairo, center.x, center.y, radius, angle1, angle2);
}

void gfxContext::Line(gfxPoint start, gfxPoint end)
{
    MoveTo(start);
    LineTo(end);
}

// XXX snapToPixels is only valid when snapping for filled
// rectangles and for even-width stroked rectangles.
// For odd-width stroked rectangles, we need to offset x/y by
// 0.5...
void gfxContext::Rectangle(gfxRect rect, PRBool snapToPixels)
{
    if (snapToPixels) {
        gfxRect snappedRect(rect);

        if (UserToDevicePixelSnapped(snappedRect)) {
            cairo_matrix_t mat;
            cairo_get_matrix(mCairo, &mat);
            cairo_identity_matrix(mCairo);
            Rectangle(snappedRect);
            cairo_set_matrix(mCairo, &mat);

            return;
        }
    }

    cairo_rectangle(mCairo, rect.pos.x, rect.pos.y, rect.size.width, rect.size.height);
}

void gfxContext::Ellipse(gfxPoint center, gfxSize dimensions)
{
    // circle?
    if (dimensions.width == dimensions.height) {
        double radius = dimensions.width / 2.0;

        cairo_arc(mCairo, center.x, center.y, radius, 0, 2.0 * M_PI);
    } else {
        double x = center.x;
        double y = center.y;
        double w = dimensions.width;
        double h = dimensions.height;

        cairo_new_path(mCairo);
        cairo_move_to(mCairo, x + w/2.0, y);

        cairo_rel_curve_to(mCairo,
                           0, 0,
                           w / 2.0, 0,
                           w / 2.0, h / 2.0);
        cairo_rel_curve_to(mCairo,
                           0, 0,
                           0, h / 2.0,
                           - w / 2.0, h / 2.0);
        cairo_rel_curve_to(mCairo,
                           0, 0,
                           - w / 2.0, 0,
                           - w / 2.0, - h / 2.0);
        cairo_rel_curve_to(mCairo,
                           0, 0,
                           0, - h / 2.0,
                           w / 2.0, - h / 2.0);
    }
}

void gfxContext::Polygon(const gfxPoint *points, PRUint32 numPoints)
{
    if (numPoints == 0)
        return;

    cairo_move_to(mCairo, points[0].x, points[0].y);
    for (PRUint32 i = 1; i < numPoints; ++i) {
        cairo_line_to(mCairo, points[i].x, points[i].y);
    }
}

void gfxContext::DrawSurface(gfxASurface *surface, gfxSize size)
{
    cairo_save(mCairo);
    cairo_set_source_surface(mCairo, surface->CairoSurface(), 0, 0);
    cairo_new_path(mCairo);

    // pixel-snap this
    Rectangle(gfxRect(gfxPoint(0.0, 0.0), size), PR_TRUE);

    cairo_fill(mCairo);
    cairo_restore(mCairo);
}

// transform stuff
void gfxContext::Translate(gfxPoint pt)
{
    cairo_translate(mCairo, pt.x, pt.y);
}
void gfxContext::Scale(gfxFloat x, gfxFloat y)
{
    cairo_scale(mCairo, x, y);
}
void gfxContext::Rotate(gfxFloat angle)
{
    cairo_rotate(mCairo, angle);
}
void gfxContext::Multiply(const gfxMatrix& matrix)
{
    cairo_matrix_t mat = matrix.ToCairoMatrix();
    cairo_transform(mCairo, &mat);
}

void gfxContext::SetMatrix(const gfxMatrix& matrix)
{
    cairo_matrix_t mat = matrix.ToCairoMatrix();
    cairo_set_matrix(mCairo, &mat);
}

void gfxContext::IdentityMatrix()
{
    cairo_identity_matrix(mCairo);
}

gfxMatrix gfxContext::CurrentMatrix() const
{
    cairo_matrix_t mat;
    cairo_get_matrix(mCairo, &mat);
    return gfxMatrix(mat);
}

gfxPoint gfxContext::DeviceToUser(gfxPoint point) const
{
    gfxPoint ret = point;
    cairo_device_to_user(mCairo, &ret.x, &ret.y);
    return ret;
}

gfxSize gfxContext::DeviceToUser(gfxSize size) const
{
    gfxSize ret = size;
    cairo_device_to_user_distance(mCairo, &ret.width, &ret.height);
    return ret;
}

gfxRect gfxContext::DeviceToUser(gfxRect rect) const
{
    gfxRect ret = rect;
    cairo_device_to_user(mCairo, &ret.pos.x, &ret.pos.y);
    cairo_device_to_user_distance(mCairo, &ret.size.width, &ret.size.height);
    return ret;
}

gfxPoint gfxContext::UserToDevice(gfxPoint point) const
{
    gfxPoint ret = point;
    cairo_user_to_device(mCairo, &ret.x, &ret.y);
    return ret;
}

gfxSize gfxContext::UserToDevice(gfxSize size) const
{
    gfxSize ret = size;
    cairo_user_to_device_distance(mCairo, &ret.width, &ret.height);
    return ret;
}

gfxRect gfxContext::UserToDevice(gfxRect rect) const
{
    gfxRect ret = rect;
    cairo_user_to_device(mCairo, &ret.pos.x, &ret.pos.y);
    cairo_user_to_device_distance(mCairo, &ret.size.width, &ret.size.height);
    return ret;
}

PRBool gfxContext::UserToDevicePixelSnapped(gfxRect& rect) const
{
    // if we're not at 1.0 scale, don't snap
    cairo_matrix_t mat;
    cairo_get_matrix(mCairo, &mat);
    if (mat.xx != 1.0 || mat.yy != 1.0)
        return PR_FALSE;

    gfxPoint p1 = UserToDevice(rect.pos);
    gfxPoint p2 = UserToDevice(rect.pos + rect.size);

    gfxPoint p3 = UserToDevice(rect.pos + gfxSize(rect.size.width, 0.0));
    gfxPoint p4 = UserToDevice(rect.pos + gfxSize(0.0, rect.size.height));

    // rectangle is no longer axis-aligned after transforming, so we can't snap
    if (p1.x != p4.x ||
        p2.x != p3.x ||
        p1.y != p3.y ||
        p2.y != p4.y)
        return PR_FALSE;

    p1.round();
    p2.round();

    gfxPoint pd = p2 - p1;

    rect.pos = p1;
    rect.size = gfxSize(pd.x, pd.y);

    return PR_TRUE;
}

void gfxContext::PixelSnappedRectangleAndSetPattern(const gfxRect& rect,
                                                    gfxPattern *pattern)
{
    gfxRect r(rect);

    // Bob attempts to pixel-snap the rectangle, and returns true if
    // the snapping succeeds.  If it does, we need to set up an
    // identity matrix, because the rectangle given back is in device
    // coordinates.
    //
    // We then have to call a translate to dr.pos afterwards, to make
    // sure the image lines up in the right place with our pixel
    // snapped rectangle.
    //
    // If snapping wasn't successful, we just translate to where the
    // pattern would normally start (in app coordinates) and do the
    // same thing.

    gfxMatrix mat = CurrentMatrix();
    if (UserToDevicePixelSnapped(r)) {
        IdentityMatrix();
    }

    Translate(r.pos);
    r.pos.x = r.pos.y = 0;
    Rectangle(r);
    SetPattern(pattern);

    SetMatrix(mat);
}

void gfxContext::SetAntialiasMode(AntialiasMode mode)
{
    // XXX implement me
}

gfxContext::AntialiasMode gfxContext::CurrentAntialiasMode()
{
    return MODE_COVERAGE;
}

void gfxContext::SetDash(gfxLineType ltype)
{
    static double dash[] = {5.0, 5.0};
    static double dot[] = {1.0, 1.0};

    switch (ltype) {
        case gfxLineDashed:
            SetDash(dash, 2, 0.0);
            break;
        case gfxLineDotted:
            SetDash(dot, 2, 0.0);
            break;
        case gfxLineSolid:
        default:
            SetDash(nsnull, 0, 0.0);
            break;
    }
}

void gfxContext::SetDash(gfxFloat *dashes, int ndash, gfxFloat offset)
{
    cairo_set_dash(mCairo, dashes, ndash, offset);
}
//void getDash() const;

void gfxContext::SetLineWidth(gfxFloat width)
{
    cairo_set_line_width(mCairo, width);
}
gfxFloat gfxContext::CurrentLineWidth() const
{
    return cairo_get_line_width(mCairo);
}

void gfxContext::SetOperator(GraphicsOperator op)
{
    cairo_set_operator(mCairo, (cairo_operator_t)op);
}
gfxContext::GraphicsOperator gfxContext::CurrentOperator() const
{
    return (GraphicsOperator)cairo_get_operator(mCairo);
}

void gfxContext::SetLineCap(GraphicsLineCap cap)
{
    cairo_set_line_cap(mCairo, (cairo_line_cap_t)cap);
}
gfxContext::GraphicsLineCap gfxContext::CurrentLineCap() const
{
    return (GraphicsLineCap)cairo_get_line_cap(mCairo);
}

void gfxContext::SetLineJoin(GraphicsLineJoin join)
{
    cairo_set_line_join(mCairo, (cairo_line_join_t)join);
}
gfxContext::GraphicsLineJoin gfxContext::CurrentLineJoin() const
{
    return (GraphicsLineJoin)cairo_get_line_join(mCairo);
}


void gfxContext::SetMiterLimit(gfxFloat limit)
{
    cairo_set_miter_limit(mCairo, limit);
}
gfxFloat gfxContext::CurrentMiterLimit() const
{
    return cairo_get_miter_limit(mCairo);
}


// clipping
void gfxContext::Clip(gfxRect rect)
{
    cairo_new_path(mCairo);
    cairo_rectangle(mCairo, rect.pos.x, rect.pos.y, rect.size.width, rect.size.height);
    cairo_clip(mCairo);
}
void gfxContext::Clip(const gfxRegion& region)
{
}

void gfxContext::Clip()
{
    cairo_clip_preserve(mCairo);
}

void gfxContext::ResetClip()
{
    cairo_reset_clip(mCairo);
}


// rendering sources

void gfxContext::SetColor(const gfxRGBA& c)
{
    cairo_set_source_rgba(mCairo, c.r, c.g, c.b, c.a);
}

void gfxContext::SetPattern(gfxPattern *pattern)
{
    cairo_set_source(mCairo, pattern->CairoPattern());
}

void gfxContext::SetSource(gfxASurface *surface, gfxPoint offset)
{
    cairo_set_source_surface(mCairo, surface->CairoSurface(), offset.x, offset.y);
}

// masking

void gfxContext::Mask(gfxPattern *pattern)
{
    cairo_mask(mCairo, pattern->CairoPattern());
}

void gfxContext::Mask(gfxASurface *surface, gfxPoint offset)
{
    cairo_mask_surface(mCairo, surface->CairoSurface(), offset.x, offset.y);
}

// fonts?
void gfxContext::DrawString(gfxTextRun& text, int pos, int len)
{

}

void gfxContext::Paint(gfxFloat alpha)
{
    cairo_paint_with_alpha(mCairo, alpha);
}

// filters
void gfxContext::PushFilter(gfxFilter& filter, FilterHints hints, gfxRect& maxArea)
{

}

void gfxContext::PopFilter()
{

}
