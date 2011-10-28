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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "cairo.h"

#include "gfxContext.h"

#include "gfxColor.h"
#include "gfxMatrix.h"
#include "gfxASurface.h"
#include "gfxPattern.h"
#include "gfxPlatform.h"
#include "gfxTeeSurface.h"

gfxContext::gfxContext(gfxASurface *surface) :
    mSurface(surface)
{
    MOZ_COUNT_CTOR(gfxContext);

    mCairo = cairo_create(surface->CairoSurface());
    mFlags = surface->GetDefaultContextFlags();
    if (mSurface->GetRotateForLandscape()) {
        // Rotate page 90 degrees to draw landscape page on portrait paper
        gfxIntSize size = mSurface->GetSize();
        Translate(gfxPoint(0, size.width));
        gfxMatrix matrix(0, -1,
                         1,  0,
                         0,  0);
        Multiply(matrix);
    }
}
gfxContext::~gfxContext()
{
    cairo_destroy(mCairo);

    MOZ_COUNT_DTOR(gfxContext);
}

gfxASurface *
gfxContext::OriginalSurface()
{
    return mSurface;
}

already_AddRefed<gfxASurface>
gfxContext::CurrentSurface(gfxFloat *dx, gfxFloat *dy)
{
    cairo_surface_t *s = cairo_get_group_target(mCairo);
    if (s == mSurface->CairoSurface()) {
        if (dx && dy)
            cairo_surface_get_device_offset(s, dx, dy);
        gfxASurface *ret = mSurface;
        NS_ADDREF(ret);
        return ret;
    }

    if (dx && dy)
        cairo_surface_get_device_offset(s, dx, dy);
    return gfxASurface::Wrap(s);
}

void
gfxContext::Save()
{
    cairo_save(mCairo);
}

void
gfxContext::Restore()
{
    cairo_restore(mCairo);
}

// drawing
void
gfxContext::NewPath()
{
    cairo_new_path(mCairo);
}

void
gfxContext::ClosePath()
{
    cairo_close_path(mCairo);
}

already_AddRefed<gfxPath> gfxContext::CopyPath() const
{
    nsRefPtr<gfxPath> path = new gfxPath(cairo_copy_path(mCairo));
    return path.forget();
}

void gfxContext::AppendPath(gfxPath* path)
{
    if (path->mPath->status == CAIRO_STATUS_SUCCESS && path->mPath->num_data != 0)
        cairo_append_path(mCairo, path->mPath);
}

gfxPoint
gfxContext::CurrentPoint() const
{
    double x, y;
    cairo_get_current_point(mCairo, &x, &y);
    return gfxPoint(x, y);
}

void
gfxContext::Stroke()
{
    cairo_stroke_preserve(mCairo);
}

void
gfxContext::Fill()
{
    cairo_fill_preserve(mCairo);
}

void
gfxContext::FillWithOpacity(gfxFloat aOpacity)
{
  // This method exists in the hope that one day cairo gets a direct
  // API for this, and then we would change this method to use that
  // API instead.
  if (aOpacity != 1.0) {
    gfxContextAutoSaveRestore saveRestore(this);
    Clip();
    Paint(aOpacity);
  } else {
    Fill();
  }
}

void
gfxContext::MoveTo(const gfxPoint& pt)
{
    cairo_move_to(mCairo, pt.x, pt.y);
}

void
gfxContext::NewSubPath()
{
    cairo_new_sub_path(mCairo);
}

void
gfxContext::LineTo(const gfxPoint& pt)
{
    cairo_line_to(mCairo, pt.x, pt.y);
}

void
gfxContext::CurveTo(const gfxPoint& pt1, const gfxPoint& pt2, const gfxPoint& pt3)
{
    cairo_curve_to(mCairo, pt1.x, pt1.y, pt2.x, pt2.y, pt3.x, pt3.y);
}

void
gfxContext::QuadraticCurveTo(const gfxPoint& pt1, const gfxPoint& pt2)
{
    double cx, cy;
    cairo_get_current_point(mCairo, &cx, &cy);
    cairo_curve_to(mCairo,
                   (cx + pt1.x * 2.0) / 3.0,
                   (cy + pt1.y * 2.0) / 3.0,
                   (pt1.x * 2.0 + pt2.x) / 3.0,
                   (pt1.y * 2.0 + pt2.y) / 3.0,
                   pt2.x,
                   pt2.y);
}

void
gfxContext::Arc(const gfxPoint& center, gfxFloat radius,
                gfxFloat angle1, gfxFloat angle2)
{
    cairo_arc(mCairo, center.x, center.y, radius, angle1, angle2);
}

void
gfxContext::NegativeArc(const gfxPoint& center, gfxFloat radius,
                        gfxFloat angle1, gfxFloat angle2)
{
    cairo_arc_negative(mCairo, center.x, center.y, radius, angle1, angle2);
}

void
gfxContext::Line(const gfxPoint& start, const gfxPoint& end)
{
    MoveTo(start);
    LineTo(end);
}

// XXX snapToPixels is only valid when snapping for filled
// rectangles and for even-width stroked rectangles.
// For odd-width stroked rectangles, we need to offset x/y by
// 0.5...
void
gfxContext::Rectangle(const gfxRect& rect, bool snapToPixels)
{
    if (snapToPixels) {
        gfxRect snappedRect(rect);

        if (UserToDevicePixelSnapped(snappedRect, true))
        {
            cairo_matrix_t mat;
            cairo_get_matrix(mCairo, &mat);
            cairo_identity_matrix(mCairo);
            Rectangle(snappedRect);
            cairo_set_matrix(mCairo, &mat);

            return;
        }
    }

    cairo_rectangle(mCairo, rect.X(), rect.Y(), rect.Width(), rect.Height());
}

void
gfxContext::Ellipse(const gfxPoint& center, const gfxSize& dimensions)
{
    gfxSize halfDim = dimensions / 2.0;
    gfxRect r(center - gfxPoint(halfDim.width, halfDim.height), dimensions);
    gfxCornerSizes c(halfDim, halfDim, halfDim, halfDim);

    RoundedRectangle (r, c);
}

void
gfxContext::Polygon(const gfxPoint *points, PRUint32 numPoints)
{
    if (numPoints == 0)
        return;

    cairo_move_to(mCairo, points[0].x, points[0].y);
    for (PRUint32 i = 1; i < numPoints; ++i) {
        cairo_line_to(mCairo, points[i].x, points[i].y);
    }
}

void
gfxContext::DrawSurface(gfxASurface *surface, const gfxSize& size)
{
    cairo_save(mCairo);
    cairo_set_source_surface(mCairo, surface->CairoSurface(), 0, 0);
    cairo_new_path(mCairo);

    // pixel-snap this
    Rectangle(gfxRect(gfxPoint(0.0, 0.0), size), true);

    cairo_fill(mCairo);
    cairo_restore(mCairo);
}

// transform stuff
void
gfxContext::Translate(const gfxPoint& pt)
{
    cairo_translate(mCairo, pt.x, pt.y);
}

void
gfxContext::Scale(gfxFloat x, gfxFloat y)
{
    cairo_scale(mCairo, x, y);
}

void
gfxContext::Rotate(gfxFloat angle)
{
    cairo_rotate(mCairo, angle);
}

void
gfxContext::Multiply(const gfxMatrix& matrix)
{
    const cairo_matrix_t& mat = reinterpret_cast<const cairo_matrix_t&>(matrix);
    cairo_transform(mCairo, &mat);
}

void
gfxContext::SetMatrix(const gfxMatrix& matrix)
{
    const cairo_matrix_t& mat = reinterpret_cast<const cairo_matrix_t&>(matrix);
    cairo_set_matrix(mCairo, &mat);
}

void
gfxContext::IdentityMatrix()
{
    cairo_identity_matrix(mCairo);
}

gfxMatrix
gfxContext::CurrentMatrix() const
{
    cairo_matrix_t mat;
    cairo_get_matrix(mCairo, &mat);
    return gfxMatrix(*reinterpret_cast<gfxMatrix*>(&mat));
}

void
gfxContext::NudgeCurrentMatrixToIntegers()
{
    cairo_matrix_t mat;
    cairo_get_matrix(mCairo, &mat);
    gfxMatrix(*reinterpret_cast<gfxMatrix*>(&mat)).NudgeToIntegers();
    cairo_set_matrix(mCairo, &mat);
}

gfxPoint
gfxContext::DeviceToUser(const gfxPoint& point) const
{
    gfxPoint ret = point;
    cairo_device_to_user(mCairo, &ret.x, &ret.y);
    return ret;
}

gfxSize
gfxContext::DeviceToUser(const gfxSize& size) const
{
    gfxSize ret = size;
    cairo_device_to_user_distance(mCairo, &ret.width, &ret.height);
    return ret;
}

gfxRect
gfxContext::DeviceToUser(const gfxRect& rect) const
{
    gfxRect ret = rect;
    cairo_device_to_user(mCairo, &ret.x, &ret.y);
    cairo_device_to_user_distance(mCairo, &ret.width, &ret.height);
    return ret;
}

gfxPoint
gfxContext::UserToDevice(const gfxPoint& point) const
{
    gfxPoint ret = point;
    cairo_user_to_device(mCairo, &ret.x, &ret.y);
    return ret;
}

gfxSize
gfxContext::UserToDevice(const gfxSize& size) const
{
    gfxSize ret = size;
    cairo_user_to_device_distance(mCairo, &ret.width, &ret.height);
    return ret;
}

gfxRect
gfxContext::UserToDevice(const gfxRect& rect) const
{
    double xmin = rect.X(), ymin = rect.Y(), xmax = rect.XMost(), ymax = rect.YMost();

    double x[3], y[3];
    x[0] = xmin;  y[0] = ymax;
    x[1] = xmax;  y[1] = ymax;
    x[2] = xmax;  y[2] = ymin;

    cairo_user_to_device(mCairo, &xmin, &ymin);
    xmax = xmin;
    ymax = ymin;
    for (int i = 0; i < 3; i++) {
        cairo_user_to_device(mCairo, &x[i], &y[i]);
        xmin = NS_MIN(xmin, x[i]);
        xmax = NS_MAX(xmax, x[i]);
        ymin = NS_MIN(ymin, y[i]);
        ymax = NS_MAX(ymax, y[i]);
    }

    return gfxRect(xmin, ymin, xmax - xmin, ymax - ymin);
}

bool
gfxContext::UserToDevicePixelSnapped(gfxRect& rect, bool ignoreScale) const
{
    if (GetFlags() & FLAG_DISABLE_SNAPPING)
        return false;

    // if we're not at 1.0 scale, don't snap, unless we're
    // ignoring the scale.  If we're not -just- a scale,
    // never snap.
    const gfxFloat epsilon = 0.0000001;
#define WITHIN_E(a,b) (fabs((a)-(b)) < epsilon)
    cairo_matrix_t mat;
    cairo_get_matrix(mCairo, &mat);
    if (!ignoreScale &&
        (!WITHIN_E(mat.xx,1.0) || !WITHIN_E(mat.yy,1.0) ||
         !WITHIN_E(mat.xy,0.0) || !WITHIN_E(mat.yx,0.0)))
        return false;
#undef WITHIN_E

    gfxPoint p1 = UserToDevice(rect.TopLeft());
    gfxPoint p2 = UserToDevice(rect.TopRight());
    gfxPoint p3 = UserToDevice(rect.BottomRight());

    // Check that the rectangle is axis-aligned. For an axis-aligned rectangle,
    // two opposite corners define the entire rectangle. So check if
    // the axis-aligned rectangle with opposite corners p1 and p3
    // define an axis-aligned rectangle whose other corners are p2 and p4.
    // We actually only need to check one of p2 and p4, since an affine
    // transform maps parallelograms to parallelograms.
    if (p2 == gfxPoint(p1.x, p3.y) || p2 == gfxPoint(p3.x, p1.y)) {
        p1.Round();
        p3.Round();

        rect.MoveTo(gfxPoint(NS_MIN(p1.x, p3.x), NS_MIN(p1.y, p3.y)));
        rect.SizeTo(gfxSize(NS_MAX(p1.x, p3.x) - rect.X(),
                            NS_MAX(p1.y, p3.y) - rect.Y()));
        return true;
    }

    return false;
}

bool
gfxContext::UserToDevicePixelSnapped(gfxPoint& pt, bool ignoreScale) const
{
    if (GetFlags() & FLAG_DISABLE_SNAPPING)
        return false;

    // if we're not at 1.0 scale, don't snap, unless we're
    // ignoring the scale.  If we're not -just- a scale,
    // never snap.
    cairo_matrix_t mat;
    cairo_get_matrix(mCairo, &mat);
    if ((!ignoreScale && (mat.xx != 1.0 || mat.yy != 1.0)) ||
        (mat.xy != 0.0 || mat.yx != 0.0))
        return false;

    pt = UserToDevice(pt);
    pt.Round();
    return true;
}

void
gfxContext::PixelSnappedRectangleAndSetPattern(const gfxRect& rect,
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

    Translate(r.TopLeft());
    r.MoveTo(gfxPoint(0, 0));
    Rectangle(r);
    SetPattern(pattern);

    SetMatrix(mat);
}

void
gfxContext::SetAntialiasMode(AntialiasMode mode)
{
    if (mode == MODE_ALIASED) {
        cairo_set_antialias(mCairo, CAIRO_ANTIALIAS_NONE);
    } else if (mode == MODE_COVERAGE) {
        cairo_set_antialias(mCairo, CAIRO_ANTIALIAS_DEFAULT);
    }
}

gfxContext::AntialiasMode
gfxContext::CurrentAntialiasMode() const
{
    cairo_antialias_t aa = cairo_get_antialias(mCairo);
    if (aa == CAIRO_ANTIALIAS_NONE)
        return MODE_ALIASED;
    return MODE_COVERAGE;
}

void
gfxContext::SetDash(gfxLineType ltype)
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

void
gfxContext::SetDash(gfxFloat *dashes, int ndash, gfxFloat offset)
{
    cairo_set_dash(mCairo, dashes, ndash, offset);
}

bool
gfxContext::CurrentDash(FallibleTArray<gfxFloat>& dashes, gfxFloat* offset) const
{
    int count = cairo_get_dash_count(mCairo);
    if (count <= 0 || !dashes.SetLength(count)) {
        return false;
    }
    cairo_get_dash(mCairo, dashes.Elements(), offset);
    return true;
}

gfxFloat
gfxContext::CurrentDashOffset() const
{
    if (cairo_get_dash_count(mCairo) <= 0) {
        return 0.0;
    }
    gfxFloat offset;
    cairo_get_dash(mCairo, NULL, &offset);
    return offset;
}

void
gfxContext::SetLineWidth(gfxFloat width)
{
    cairo_set_line_width(mCairo, width);
}

gfxFloat
gfxContext::CurrentLineWidth() const
{
    return cairo_get_line_width(mCairo);
}

void
gfxContext::SetOperator(GraphicsOperator op)
{
    if (mFlags & FLAG_SIMPLIFY_OPERATORS) {
        if (op != OPERATOR_SOURCE &&
            op != OPERATOR_CLEAR &&
            op != OPERATOR_OVER)
            op = OPERATOR_OVER;
    }

    cairo_set_operator(mCairo, (cairo_operator_t)op);
}

gfxContext::GraphicsOperator
gfxContext::CurrentOperator() const
{
    return (GraphicsOperator)cairo_get_operator(mCairo);
}

void
gfxContext::SetLineCap(GraphicsLineCap cap)
{
    cairo_set_line_cap(mCairo, (cairo_line_cap_t)cap);
}

gfxContext::GraphicsLineCap
gfxContext::CurrentLineCap() const
{
    return (GraphicsLineCap)cairo_get_line_cap(mCairo);
}

void
gfxContext::SetLineJoin(GraphicsLineJoin join)
{
    cairo_set_line_join(mCairo, (cairo_line_join_t)join);
}

gfxContext::GraphicsLineJoin
gfxContext::CurrentLineJoin() const
{
    return (GraphicsLineJoin)cairo_get_line_join(mCairo);
}

void
gfxContext::SetMiterLimit(gfxFloat limit)
{
    cairo_set_miter_limit(mCairo, limit);
}

gfxFloat
gfxContext::CurrentMiterLimit() const
{
    return cairo_get_miter_limit(mCairo);
}

void
gfxContext::SetFillRule(FillRule rule)
{
    cairo_set_fill_rule(mCairo, (cairo_fill_rule_t)rule);
}

gfxContext::FillRule
gfxContext::CurrentFillRule() const
{
    return (FillRule)cairo_get_fill_rule(mCairo);
}

// clipping
void
gfxContext::Clip(const gfxRect& rect)
{
    cairo_new_path(mCairo);
    cairo_rectangle(mCairo, rect.X(), rect.Y(), rect.Width(), rect.Height());
    cairo_clip(mCairo);
}

void
gfxContext::Clip()
{
    cairo_clip_preserve(mCairo);
}

void
gfxContext::ResetClip()
{
    cairo_reset_clip(mCairo);
}

void
gfxContext::UpdateSurfaceClip()
{
    NewPath();
    // we paint an empty rectangle to ensure the clip is propagated to
    // the destination surface
    SetDeviceColor(gfxRGBA(0,0,0,0));
    Rectangle(gfxRect(0,1,1,0));
    Fill();
}

gfxRect
gfxContext::GetClipExtents()
{
    double xmin, ymin, xmax, ymax;
    cairo_clip_extents(mCairo, &xmin, &ymin, &xmax, &ymax);
    return gfxRect(xmin, ymin, xmax - xmin, ymax - ymin);
}

bool
gfxContext::ClipContainsRect(const gfxRect& aRect)
{
    cairo_rectangle_list_t *clip =
        cairo_copy_clip_rectangle_list(mCairo);

    bool result = false;

    if (clip->status == CAIRO_STATUS_SUCCESS) {
        for (int i = 0; i < clip->num_rectangles; i++) {
            gfxRect rect(clip->rectangles[i].x, clip->rectangles[i].y,
                         clip->rectangles[i].width, clip->rectangles[i].height);
            if (rect.Contains(aRect)) {
                result = true;
                break;
            }
        }
    }

   cairo_rectangle_list_destroy(clip);
   return result;
}

// rendering sources

void
gfxContext::SetColor(const gfxRGBA& c)
{
    if (gfxPlatform::GetCMSMode() == eCMSMode_All) {

        gfxRGBA cms;
        gfxPlatform::TransformPixel(c, cms, gfxPlatform::GetCMSRGBTransform());

        // Use the original alpha to avoid unnecessary float->byte->float
        // conversion errors
        cairo_set_source_rgba(mCairo, cms.r, cms.g, cms.b, c.a);
    }
    else
        cairo_set_source_rgba(mCairo, c.r, c.g, c.b, c.a);
}

void
gfxContext::SetDeviceColor(const gfxRGBA& c)
{
    cairo_set_source_rgba(mCairo, c.r, c.g, c.b, c.a);
}

bool
gfxContext::GetDeviceColor(gfxRGBA& c)
{
    return cairo_pattern_get_rgba(cairo_get_source(mCairo),
                                  &c.r,
                                  &c.g,
                                  &c.b,
                                  &c.a) == CAIRO_STATUS_SUCCESS;
}

void
gfxContext::SetSource(gfxASurface *surface, const gfxPoint& offset)
{
    NS_ASSERTION(surface->GetAllowUseAsSource(), "Surface not allowed to be used as source!");
    cairo_set_source_surface(mCairo, surface->CairoSurface(), offset.x, offset.y);
}

void
gfxContext::SetPattern(gfxPattern *pattern)
{
    cairo_set_source(mCairo, pattern->CairoPattern());
}

already_AddRefed<gfxPattern>
gfxContext::GetPattern()
{
    cairo_pattern_t *pat = cairo_get_source(mCairo);
    NS_ASSERTION(pat, "I was told this couldn't be null");

    gfxPattern *wrapper = nsnull;
    if (pat)
        wrapper = new gfxPattern(pat);
    else
        wrapper = new gfxPattern(gfxRGBA(0,0,0,0));

    NS_IF_ADDREF(wrapper);
    return wrapper;
}


// masking

void
gfxContext::Mask(gfxPattern *pattern)
{
    cairo_mask(mCairo, pattern->CairoPattern());
}

void
gfxContext::Mask(gfxASurface *surface, const gfxPoint& offset)
{
    cairo_mask_surface(mCairo, surface->CairoSurface(), offset.x, offset.y);
}

void
gfxContext::Paint(gfxFloat alpha)
{
    cairo_paint_with_alpha(mCairo, alpha);
}

// groups

void
gfxContext::PushGroup(gfxASurface::gfxContentType content)
{
    cairo_push_group_with_content(mCairo, (cairo_content_t) content);
}

static gfxRect
GetRoundOutDeviceClipExtents(gfxContext* aCtx)
{
    gfxContextMatrixAutoSaveRestore save(aCtx);
    aCtx->IdentityMatrix();
    gfxRect r = aCtx->GetClipExtents();
    r.RoundOut();
    return r;
}

/**
 * Copy the contents of aSrc to aDest, translated by aTranslation.
 */
static void
CopySurface(gfxASurface* aSrc, gfxASurface* aDest, const gfxPoint& aTranslation)
{
  cairo_t *cr = cairo_create(aDest->CairoSurface());
  cairo_set_source_surface(cr, aSrc->CairoSurface(), aTranslation.x, aTranslation.y);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);
  cairo_destroy(cr);
}

void
gfxContext::PushGroupAndCopyBackground(gfxASurface::gfxContentType content)
{
    if (content == gfxASurface::CONTENT_COLOR_ALPHA &&
        !(GetFlags() & FLAG_DISABLE_COPY_BACKGROUND)) {
        nsRefPtr<gfxASurface> s = CurrentSurface();
        if ((s->GetAllowUseAsSource() || s->GetType() == gfxASurface::SurfaceTypeTee) &&
            (s->GetContentType() == gfxASurface::CONTENT_COLOR ||
             s->GetOpaqueRect().Contains(GetRoundOutDeviceClipExtents(this)))) {
            cairo_push_group_with_content(mCairo, CAIRO_CONTENT_COLOR);
            nsRefPtr<gfxASurface> d = CurrentSurface();

            if (d->GetType() == gfxASurface::SurfaceTypeTee) {
                NS_ASSERTION(s->GetType() == gfxASurface::SurfaceTypeTee, "Mismatched types");
                nsAutoTArray<nsRefPtr<gfxASurface>,2> ss;
                nsAutoTArray<nsRefPtr<gfxASurface>,2> ds;
                static_cast<gfxTeeSurface*>(s.get())->GetSurfaces(&ss);
                static_cast<gfxTeeSurface*>(d.get())->GetSurfaces(&ds);
                NS_ASSERTION(ss.Length() == ds.Length(), "Mismatched lengths");
                gfxPoint translation = d->GetDeviceOffset() - s->GetDeviceOffset();
                for (PRUint32 i = 0; i < ss.Length(); ++i) {
                    CopySurface(ss[i], ds[i], translation);
                }
            } else {
                CopySurface(s, d, gfxPoint(0, 0));
            }
            d->SetOpaqueRect(s->GetOpaqueRect());
            return;
        }
    }
    cairo_push_group_with_content(mCairo, (cairo_content_t) content);
}

already_AddRefed<gfxPattern>
gfxContext::PopGroup()
{
    cairo_pattern_t *pat = cairo_pop_group(mCairo);
    gfxPattern *wrapper = new gfxPattern(pat);
    cairo_pattern_destroy(pat);
    NS_IF_ADDREF(wrapper);
    return wrapper;
}

void
gfxContext::PopGroupToSource()
{
    cairo_pop_group_to_source(mCairo);
}

bool
gfxContext::PointInFill(const gfxPoint& pt)
{
    return cairo_in_fill(mCairo, pt.x, pt.y);
}

bool
gfxContext::PointInStroke(const gfxPoint& pt)
{
    return cairo_in_stroke(mCairo, pt.x, pt.y);
}

gfxRect
gfxContext::GetUserPathExtent()
{
    double xmin, ymin, xmax, ymax;
    cairo_path_extents(mCairo, &xmin, &ymin, &xmax, &ymax);
    return gfxRect(xmin, ymin, xmax - xmin, ymax - ymin);
}

gfxRect
gfxContext::GetUserFillExtent()
{
    double xmin, ymin, xmax, ymax;
    cairo_fill_extents(mCairo, &xmin, &ymin, &xmax, &ymax);
    return gfxRect(xmin, ymin, xmax - xmin, ymax - ymin);
}

gfxRect
gfxContext::GetUserStrokeExtent()
{
    double xmin, ymin, xmax, ymax;
    cairo_stroke_extents(mCairo, &xmin, &ymin, &xmax, &ymax);
    return gfxRect(xmin, ymin, xmax - xmin, ymax - ymin);
}

already_AddRefed<gfxFlattenedPath>
gfxContext::GetFlattenedPath()
{
    gfxFlattenedPath *path =
        new gfxFlattenedPath(cairo_copy_path_flat(mCairo));
    NS_IF_ADDREF(path);
    return path;
}

bool
gfxContext::HasError()
{
     return cairo_status(mCairo) != CAIRO_STATUS_SUCCESS;
}

void
gfxContext::RoundedRectangle(const gfxRect& rect,
                             const gfxCornerSizes& corners,
                             bool draw_clockwise)
{
    //
    // For CW drawing, this looks like:
    //
    //  ...******0**      1    C
    //              ****
    //                  ***    2
    //                     **
    //                       *
    //                        *
    //                         3
    //                         *
    //                         *
    //
    // Where 0, 1, 2, 3 are the control points of the Bezier curve for
    // the corner, and C is the actual corner point.
    //
    // At the start of the loop, the current point is assumed to be
    // the point adjacent to the top left corner on the top
    // horizontal.  Note that corner indices start at the top left and
    // continue clockwise, whereas in our loop i = 0 refers to the top
    // right corner.
    //
    // When going CCW, the control points are swapped, and the first
    // corner that's drawn is the top left (along with the top segment).
    //
    // There is considerable latitude in how one chooses the four
    // control points for a Bezier curve approximation to an ellipse.
    // For the overall path to be continuous and show no corner at the
    // endpoints of the arc, points 0 and 3 must be at the ends of the
    // straight segments of the rectangle; points 0, 1, and C must be
    // collinear; and points 3, 2, and C must also be collinear.  This
    // leaves only two free parameters: the ratio of the line segments
    // 01 and 0C, and the ratio of the line segments 32 and 3C.  See
    // the following papers for extensive discussion of how to choose
    // these ratios:
    //
    //   Dokken, Tor, et al. "Good approximation of circles by
    //      curvature-continuous Bezier curves."  Computer-Aided
    //      Geometric Design 7(1990) 33--41.
    //   Goldapp, Michael. "Approximation of circular arcs by cubic
    //      polynomials." Computer-Aided Geometric Design 8(1991) 227--238.
    //   Maisonobe, Luc. "Drawing an elliptical arc using polylines,
    //      quadratic, or cubic Bezier curves."
    //      http://www.spaceroots.org/documents/ellipse/elliptical-arc.pdf
    //
    // We follow the approach in section 2 of Goldapp (least-error,
    // Hermite-type approximation) and make both ratios equal to
    //
    //          2   2 + n - sqrt(2n + 28)
    //  alpha = - * ---------------------
    //          3           n - 4
    //
    // where n = 3( cbrt(sqrt(2)+1) - cbrt(sqrt(2)-1) ).
    //
    // This is the result of Goldapp's equation (10b) when the angle
    // swept out by the arc is pi/2, and the parameter "a-bar" is the
    // expression given immediately below equation (21).
    //
    // Using this value, the maximum radial error for a circle, as a
    // fraction of the radius, is on the order of 0.2 x 10^-3.
    // Neither Dokken nor Goldapp discusses error for a general
    // ellipse; Maisonobe does, but his choice of control points
    // follows different constraints, and Goldapp's expression for
    // 'alpha' gives much smaller radial error, even for very flat
    // ellipses, than Maisonobe's equivalent.
    //
    // For the various corners and for each axis, the sign of this
    // constant changes, or it might be 0 -- it's multiplied by the
    // appropriate multiplier from the list before using.
    const gfxFloat alpha = 0.55191497064665766025;

    typedef struct { gfxFloat a, b; } twoFloats;

    twoFloats cwCornerMults[4] = { { -1,  0 },
                                   {  0, -1 },
                                   { +1,  0 },
                                   {  0, +1 } };
    twoFloats ccwCornerMults[4] = { { +1,  0 },
                                    {  0, -1 },
                                    { -1,  0 },
                                    {  0, +1 } };

    twoFloats *cornerMults = draw_clockwise ? cwCornerMults : ccwCornerMults;

    gfxPoint pc, p0, p1, p2, p3;

    if (draw_clockwise)
        cairo_move_to(mCairo, rect.X() + corners[NS_CORNER_TOP_LEFT].width, rect.Y());
    else
        cairo_move_to(mCairo, rect.X() + rect.Width() - corners[NS_CORNER_TOP_RIGHT].width, rect.Y());

    NS_FOR_CSS_CORNERS(i) {
        // the corner index -- either 1 2 3 0 (cw) or 0 3 2 1 (ccw)
        mozilla::css::Corner c = mozilla::css::Corner(draw_clockwise ? ((i+1) % 4) : ((4-i) % 4));

        // i+2 and i+3 respectively.  These are used to index into the corner
        // multiplier table, and were deduced by calculating out the long form
        // of each corner and finding a pattern in the signs and values.
        int i2 = (i+2) % 4;
        int i3 = (i+3) % 4;

        pc = rect.AtCorner(c);

        if (corners[c].width > 0.0 && corners[c].height > 0.0) {
            p0.x = pc.x + cornerMults[i].a * corners[c].width;
            p0.y = pc.y + cornerMults[i].b * corners[c].height;

            p3.x = pc.x + cornerMults[i3].a * corners[c].width;
            p3.y = pc.y + cornerMults[i3].b * corners[c].height;

            p1.x = p0.x + alpha * cornerMults[i2].a * corners[c].width;
            p1.y = p0.y + alpha * cornerMults[i2].b * corners[c].height;

            p2.x = p3.x - alpha * cornerMults[i3].a * corners[c].width;
            p2.y = p3.y - alpha * cornerMults[i3].b * corners[c].height;

            cairo_line_to (mCairo, p0.x, p0.y);
            cairo_curve_to (mCairo,
                            p1.x, p1.y,
                            p2.x, p2.y,
                            p3.x, p3.y);
        } else {
            cairo_line_to (mCairo, pc.x, pc.y);
        }
    }

    cairo_close_path (mCairo);
}

#ifdef DEBUG
void
gfxContext::WriteAsPNG(const char* aFile)
{ 
  nsRefPtr<gfxASurface> surf = CurrentSurface();
  if (surf) {
    surf->WriteAsPNG(aFile);
  } else {
    NS_WARNING("No surface found!");
  }
}

void 
gfxContext::DumpAsDataURL()
{ 
  nsRefPtr<gfxASurface> surf = CurrentSurface();
  if (surf) {
    surf->DumpAsDataURL();
  } else {
    NS_WARNING("No surface found!");
  }
}

void 
gfxContext::CopyAsDataURL()
{ 
  nsRefPtr<gfxASurface> surf = CurrentSurface();
  if (surf) {
    surf->CopyAsDataURL();
  } else {
    NS_WARNING("No surface found!");
  }
}
#endif
