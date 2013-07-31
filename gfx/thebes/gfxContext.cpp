/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#include "mozilla/Constants.h"

#include "cairo.h"

#include "gfxContext.h"

#include "gfxColor.h"
#include "gfxMatrix.h"
#include "gfxASurface.h"
#include "gfxPattern.h"
#include "gfxPlatform.h"
#include "gfxTeeSurface.h"
#include "GeckoProfiler.h"
#include <algorithm>

#if CAIRO_HAS_DWRITE_FONT
#include "gfxWindowsPlatform.h"
#endif

using namespace mozilla;
using namespace mozilla::gfx;

/* This class lives on the stack and allows gfxContext users to easily, and
 * performantly get a gfx::Pattern to use for drawing in their current context.
 */
class GeneralPattern
{
public:    
  GeneralPattern(gfxContext *aContext) : mContext(aContext), mPattern(NULL) {}
  ~GeneralPattern() { if (mPattern) { mPattern->~Pattern(); } }

  operator mozilla::gfx::Pattern&()
  {
    gfxContext::AzureState &state = mContext->CurrentState();

    if (state.pattern) {
      return *state.pattern->GetPattern(mContext->mDT, state.patternTransformChanged ? &state.patternTransform : nullptr);
    } else if (state.sourceSurface) {
      Matrix transform = state.surfTransform;

      if (state.patternTransformChanged) {
        Matrix mat = mContext->mTransform;
        mat.Invert();

        transform = transform * state.patternTransform * mat;
      }

      mPattern = new (mSurfacePattern.addr())
        SurfacePattern(state.sourceSurface, EXTEND_CLAMP, transform);
      return *mPattern;
    } else {
      mPattern = new (mColorPattern.addr())
        ColorPattern(state.color);
      return *mPattern;
    }
  }

private:
  union {
    mozilla::AlignedStorage2<mozilla::gfx::ColorPattern> mColorPattern;
    mozilla::AlignedStorage2<mozilla::gfx::SurfacePattern> mSurfacePattern;
  };

  gfxContext *mContext;
  Pattern *mPattern;
};

gfxContext::gfxContext(gfxASurface *surface)
  : mRefCairo(NULL)
  , mSurface(surface)
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

gfxContext::gfxContext(DrawTarget *aTarget)
  : mPathIsRect(false)
  , mTransformChanged(false)
  , mCairo(NULL)
  , mRefCairo(NULL)
  , mSurface(NULL)
  , mFlags(0)
  , mDT(aTarget)
  , mOriginalDT(aTarget)
{
  MOZ_COUNT_CTOR(gfxContext);

  mStateStack.SetLength(1);
  CurrentState().drawTarget = mDT;
  mDT->SetTransform(Matrix());
}

gfxContext::~gfxContext()
{
  if (mCairo) {
    cairo_destroy(mCairo);
  }
  if (mRefCairo) {
    cairo_destroy(mRefCairo);
  }
  if (mDT) {
    for (int i = mStateStack.Length() - 1; i >= 0; i--) {
      for (unsigned int c = 0; c < mStateStack[i].pushedClips.Length(); c++) {
        mDT->PopClip();
      }

      if (mStateStack[i].clipWasReset) {
        break;
      }
    }
    mDT->Flush();
  }
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
  if (mCairo) {
    cairo_surface_t *s = cairo_get_group_target(mCairo);
    if (s == mSurface->CairoSurface()) {
        if (dx && dy)
            cairo_surface_get_device_offset(s, dx, dy);
        nsRefPtr<gfxASurface> ret = mSurface;
        return ret.forget();
    }

    if (dx && dy)
        cairo_surface_get_device_offset(s, dx, dy);
    return gfxASurface::Wrap(s);
  } else {
    if (dx && dy) {
      *dx = *dy = 0;
    }
    // An Azure context doesn't have a surface backing it.
    return nullptr;
  }
}

cairo_t *
gfxContext::GetCairo()
{
  if (mCairo) {
    return mCairo;
  }

  if (mRefCairo) {
    // Set transform!
    return mRefCairo;
  }

  mRefCairo = cairo_create(gfxPlatform::GetPlatform()->ScreenReferenceSurface()->CairoSurface()); 

  return mRefCairo;
}

void
gfxContext::Save()
{
  if (mCairo) {
    cairo_save(mCairo);
  } else {
    CurrentState().transform = mTransform;
    mStateStack.AppendElement(AzureState(CurrentState()));
    CurrentState().clipWasReset = false;
    CurrentState().pushedClips.Clear();
  }
}

void
gfxContext::Restore()
{
  if (mCairo) {
    cairo_restore(mCairo);
  } else {
    for (unsigned int c = 0; c < CurrentState().pushedClips.Length(); c++) {
      mDT->PopClip();
    }

    if (CurrentState().clipWasReset &&
        CurrentState().drawTarget == mStateStack[mStateStack.Length() - 2].drawTarget) {
      PushClipsToDT(mDT);
    }

    mStateStack.RemoveElementAt(mStateStack.Length() - 1);

    mDT = CurrentState().drawTarget;

    ChangeTransform(CurrentState().transform, false);
  }
}

// drawing
void
gfxContext::NewPath()
{
  if (mCairo) {
    cairo_new_path(mCairo);
  } else {
    mPath = NULL;
    mPathBuilder = NULL;
    mPathIsRect = false;
    mTransformChanged = false;
  }
}

void
gfxContext::ClosePath()
{
  if (mCairo) {
    cairo_close_path(mCairo);
  } else {
    EnsurePathBuilder();
    mPathBuilder->Close();
  }
}

already_AddRefed<gfxPath> gfxContext::CopyPath() const
{
  if (mCairo) {
    nsRefPtr<gfxPath> path = new gfxPath(cairo_copy_path(mCairo));
    return path.forget();
  } else {
    // XXX - This is not yet supported for Azure.
    return nullptr;
  }
}

void gfxContext::AppendPath(gfxPath* path)
{
  if (mCairo) {
    if (path->mPath->status == CAIRO_STATUS_SUCCESS && path->mPath->num_data != 0)
        cairo_append_path(mCairo, path->mPath);
  } else {
    // XXX - This is not yet supported for Azure.
    return;
  }
}

gfxPoint
gfxContext::CurrentPoint()
{
  if (mCairo) {
    double x, y;
    cairo_get_current_point(mCairo, &x, &y);
    return gfxPoint(x, y);
  } else {
    EnsurePathBuilder();
    return ThebesPoint(mPathBuilder->CurrentPoint());
  }
}

void
gfxContext::Stroke()
{
  if (mCairo) {
    cairo_stroke_preserve(mCairo);
  } else {
    AzureState &state = CurrentState();
    if (mPathIsRect) {
      MOZ_ASSERT(!mTransformChanged);

      mDT->StrokeRect(mRect, GeneralPattern(this),
                      state.strokeOptions,
                      DrawOptions(1.0f, GetOp(), state.aaMode));
    } else {
      EnsurePath();

      mDT->Stroke(mPath, GeneralPattern(this), state.strokeOptions,
                  DrawOptions(1.0f, GetOp(), state.aaMode));
    }
  }
}

void
gfxContext::Fill()
{
  PROFILER_LABEL("gfxContext", "Fill");
  if (mCairo) {
    cairo_fill_preserve(mCairo);
  } else {
    FillAzure(1.0f);
  }
}

void
gfxContext::FillWithOpacity(gfxFloat aOpacity)
{
  if (mCairo) {
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
  } else {
    FillAzure(Float(aOpacity));
  }
}

void
gfxContext::MoveTo(const gfxPoint& pt)
{
  if (mCairo) {
    cairo_move_to(mCairo, pt.x, pt.y);
  } else {
    EnsurePathBuilder();
    mPathBuilder->MoveTo(ToPoint(pt));
  }
}

void
gfxContext::NewSubPath()
{
  if (mCairo) {
    cairo_new_sub_path(mCairo);
  } else {
    // XXX - This has no users, we should kill it, it should be equivelant to a
    // MoveTo to the path's current point.
  }
}

void
gfxContext::LineTo(const gfxPoint& pt)
{
  if (mCairo) {
    cairo_line_to(mCairo, pt.x, pt.y);
  } else {
    EnsurePathBuilder();
    mPathBuilder->LineTo(ToPoint(pt));
  }
}

void
gfxContext::CurveTo(const gfxPoint& pt1, const gfxPoint& pt2, const gfxPoint& pt3)
{
  if (mCairo) {
    cairo_curve_to(mCairo, pt1.x, pt1.y, pt2.x, pt2.y, pt3.x, pt3.y);
  } else {
    EnsurePathBuilder();
    mPathBuilder->BezierTo(ToPoint(pt1), ToPoint(pt2), ToPoint(pt3));
  }
}

void
gfxContext::QuadraticCurveTo(const gfxPoint& pt1, const gfxPoint& pt2)
{
  if (mCairo) {
    double cx, cy;
    cairo_get_current_point(mCairo, &cx, &cy);
    cairo_curve_to(mCairo,
                   (cx + pt1.x * 2.0) / 3.0,
                   (cy + pt1.y * 2.0) / 3.0,
                   (pt1.x * 2.0 + pt2.x) / 3.0,
                   (pt1.y * 2.0 + pt2.y) / 3.0,
                   pt2.x,
                   pt2.y);
  } else {
    EnsurePathBuilder();
    mPathBuilder->QuadraticBezierTo(ToPoint(pt1), ToPoint(pt2));
  }
}

void
gfxContext::Arc(const gfxPoint& center, gfxFloat radius,
                gfxFloat angle1, gfxFloat angle2)
{
  if (mCairo) {
    cairo_arc(mCairo, center.x, center.y, radius, angle1, angle2);
  } else {
    EnsurePathBuilder();
    mPathBuilder->Arc(ToPoint(center), Float(radius), Float(angle1), Float(angle2));
  }
}

void
gfxContext::NegativeArc(const gfxPoint& center, gfxFloat radius,
                        gfxFloat angle1, gfxFloat angle2)
{
  if (mCairo) {
    cairo_arc_negative(mCairo, center.x, center.y, radius, angle1, angle2);
  } else {
    EnsurePathBuilder();
    mPathBuilder->Arc(ToPoint(center), Float(radius), Float(angle2), Float(angle1));
  }
}

void
gfxContext::Line(const gfxPoint& start, const gfxPoint& end)
{
  if (mCairo) {
    MoveTo(start);
    LineTo(end);
  } else {
    EnsurePathBuilder();
    mPathBuilder->MoveTo(ToPoint(start));
    mPathBuilder->LineTo(ToPoint(end));
  }
}

// XXX snapToPixels is only valid when snapping for filled
// rectangles and for even-width stroked rectangles.
// For odd-width stroked rectangles, we need to offset x/y by
// 0.5...
void
gfxContext::Rectangle(const gfxRect& rect, bool snapToPixels)
{
  if (mCairo) {
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
  } else {
    Rect rec = ToRect(rect);

    if (snapToPixels) {
      gfxRect newRect(rect);
      if (UserToDevicePixelSnapped(newRect, true)) {
        gfxMatrix mat = ThebesMatrix(mTransform);
        mat.Invert();

        // We need the user space rect.
        rec = ToRect(mat.TransformBounds(newRect));
      }
    }

    if (!mPathBuilder && !mPathIsRect) {
      mPathIsRect = true;
      mRect = rec;
      return;
    }

    EnsurePathBuilder();

    mPathBuilder->MoveTo(rec.TopLeft());
    mPathBuilder->LineTo(rec.TopRight());
    mPathBuilder->LineTo(rec.BottomRight());
    mPathBuilder->LineTo(rec.BottomLeft());
    mPathBuilder->Close();
  }
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
gfxContext::Polygon(const gfxPoint *points, uint32_t numPoints)
{
  if (mCairo) {
    if (numPoints == 0)
        return;

    cairo_move_to(mCairo, points[0].x, points[0].y);
    for (uint32_t i = 1; i < numPoints; ++i) {
        cairo_line_to(mCairo, points[i].x, points[i].y);
    }
  } else {
    if (numPoints == 0) {
      return;
    }

    EnsurePathBuilder();

    mPathBuilder->MoveTo(ToPoint(points[0]));
    for (uint32_t i = 1; i < numPoints; i++) {
      mPathBuilder->LineTo(ToPoint(points[i]));
    }
  }
}

void
gfxContext::DrawSurface(gfxASurface *surface, const gfxSize& size)
{
  if (mCairo) {
    cairo_save(mCairo);
    cairo_set_source_surface(mCairo, surface->CairoSurface(), 0, 0);
    cairo_new_path(mCairo);

    // pixel-snap this
    Rectangle(gfxRect(gfxPoint(0.0, 0.0), size), true);

    cairo_fill(mCairo);
    cairo_restore(mCairo);
  } else {
    // Lifetime needs to be limited here since we may wrap surface's data.
    RefPtr<SourceSurface> surf =
      gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(mDT, surface);

    Rect rect(0, 0, Float(size.width), Float(size.height));
    rect.Intersect(Rect(0, 0, Float(surf->GetSize().width), Float(surf->GetSize().height)));

    // XXX - Should fix pixel snapping.
    mDT->DrawSurface(surf, rect, rect);
  }
}

// transform stuff
void
gfxContext::Translate(const gfxPoint& pt)
{
  if (mCairo) {
    cairo_translate(mCairo, pt.x, pt.y);
  } else {
    Matrix newMatrix = mTransform;

    ChangeTransform(newMatrix.Translate(Float(pt.x), Float(pt.y)));
  }
}

void
gfxContext::Scale(gfxFloat x, gfxFloat y)
{
  if (mCairo) {
    cairo_scale(mCairo, x, y);
  } else {
    Matrix newMatrix = mTransform;

    ChangeTransform(newMatrix.Scale(Float(x), Float(y)));
  }
}

void
gfxContext::Rotate(gfxFloat angle)
{
  if (mCairo) {
    cairo_rotate(mCairo, angle);
  } else {
    Matrix rotation = Matrix::Rotation(Float(angle));
    ChangeTransform(rotation * mTransform);
  }
}

void
gfxContext::Multiply(const gfxMatrix& matrix)
{
  if (mCairo) {
    const cairo_matrix_t& mat = reinterpret_cast<const cairo_matrix_t&>(matrix);
    cairo_transform(mCairo, &mat);
  } else {
    ChangeTransform(ToMatrix(matrix) * mTransform);
  }
}

void
gfxContext::MultiplyAndNudgeToIntegers(const gfxMatrix& matrix)
{
  if (mCairo) {
    const cairo_matrix_t& mat = reinterpret_cast<const cairo_matrix_t&>(matrix);
    cairo_transform(mCairo, &mat);
    // XXX nudging to integers not currently supported for Thebes
  } else {
    Matrix transform = ToMatrix(matrix) * mTransform;
    transform.NudgeToIntegers();
    ChangeTransform(transform);
  }
}

void
gfxContext::SetMatrix(const gfxMatrix& matrix)
{
  if (mCairo) {
    const cairo_matrix_t& mat = reinterpret_cast<const cairo_matrix_t&>(matrix);
    cairo_set_matrix(mCairo, &mat);
  } else {
    Matrix mat;
    mat.Translate(-CurrentState().deviceOffset.x, -CurrentState().deviceOffset.y);
    ChangeTransform(ToMatrix(matrix));
  }
}

void
gfxContext::IdentityMatrix()
{
  if (mCairo) {
    cairo_identity_matrix(mCairo);
  } else {
    ChangeTransform(Matrix());
  }
}

gfxMatrix
gfxContext::CurrentMatrix() const
{
  if (mCairo) {
    cairo_matrix_t mat;
    cairo_get_matrix(mCairo, &mat);
    return gfxMatrix(*reinterpret_cast<gfxMatrix*>(&mat));
  } else {
    return ThebesMatrix(mTransform);
  }
}

void
gfxContext::NudgeCurrentMatrixToIntegers()
{
  if (mCairo) {
    cairo_matrix_t mat;
    cairo_get_matrix(mCairo, &mat);
    gfxMatrix(*reinterpret_cast<gfxMatrix*>(&mat)).NudgeToIntegers();
    cairo_set_matrix(mCairo, &mat);
  } else {
    gfxMatrix matrix = ThebesMatrix(mTransform);
    matrix.NudgeToIntegers();
    ChangeTransform(ToMatrix(matrix));
  }
}

gfxPoint
gfxContext::DeviceToUser(const gfxPoint& point) const
{
  if (mCairo) {
    gfxPoint ret = point;
    cairo_device_to_user(mCairo, &ret.x, &ret.y);
    return ret;
  } else {
    Matrix matrix = mTransform;

    matrix.Invert();

    return ThebesPoint(matrix * ToPoint(point));
  }
}

gfxSize
gfxContext::DeviceToUser(const gfxSize& size) const
{
  if (mCairo) {
    gfxSize ret = size;
    cairo_device_to_user_distance(mCairo, &ret.width, &ret.height);
    return ret;
  } else {
    Matrix matrix = mTransform;

    matrix.Invert();

    return ThebesSize(matrix * ToSize(size));
  }
}

gfxRect
gfxContext::DeviceToUser(const gfxRect& rect) const
{
  if (mCairo) {
    gfxRect ret = rect;
    cairo_device_to_user(mCairo, &ret.x, &ret.y);
    cairo_device_to_user_distance(mCairo, &ret.width, &ret.height);
    return ret;
  } else {
    Matrix matrix = mTransform;

    matrix.Invert();

    return ThebesRect(matrix.TransformBounds(ToRect(rect)));
  }
}

gfxPoint
gfxContext::UserToDevice(const gfxPoint& point) const
{
  if (mCairo) {
    gfxPoint ret = point;
    cairo_user_to_device(mCairo, &ret.x, &ret.y);
    return ret;
  } else {
    return ThebesPoint(mTransform * ToPoint(point));
  }
}

gfxSize
gfxContext::UserToDevice(const gfxSize& size) const
{
  if (mCairo) {
    gfxSize ret = size;
    cairo_user_to_device_distance(mCairo, &ret.width, &ret.height);
    return ret;
  } else {
    const Matrix &matrix = mTransform;

    gfxSize newSize = size;
    newSize.width = newSize.width * matrix._11 + newSize.height * matrix._12;
    newSize.height = newSize.width * matrix._21 + newSize.height * matrix._22;
    return newSize;
  }
}

gfxRect
gfxContext::UserToDevice(const gfxRect& rect) const
{
  if (mCairo) {
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
        xmin = std::min(xmin, x[i]);
        xmax = std::max(xmax, x[i]);
        ymin = std::min(ymin, y[i]);
        ymax = std::max(ymax, y[i]);
    }

    return gfxRect(xmin, ymin, xmax - xmin, ymax - ymin);
  } else {
    const Matrix &matrix = mTransform;
    return ThebesRect(matrix.TransformBounds(ToRect(rect)));
  }
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
  if (mCairo) {
    cairo_matrix_t mat;
    cairo_get_matrix(mCairo, &mat);
    if (!ignoreScale &&
        (!WITHIN_E(mat.xx,1.0) || !WITHIN_E(mat.yy,1.0) ||
          !WITHIN_E(mat.xy,0.0) || !WITHIN_E(mat.yx,0.0)))
        return false;
  } else {
    Matrix mat = mTransform;
    if (!ignoreScale &&
        (!WITHIN_E(mat._11,1.0) || !WITHIN_E(mat._22,1.0) ||
          !WITHIN_E(mat._12,0.0) || !WITHIN_E(mat._21,0.0)))
        return false;
  }
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

      rect.MoveTo(gfxPoint(std::min(p1.x, p3.x), std::min(p1.y, p3.y)));
      rect.SizeTo(gfxSize(std::max(p1.x, p3.x) - rect.X(),
                          std::max(p1.y, p3.y) - rect.Y()));
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
  const gfxFloat epsilon = 0.0000001;
#define WITHIN_E(a,b) (fabs((a)-(b)) < epsilon)
  if (mCairo) {
    cairo_matrix_t mat;
    cairo_get_matrix(mCairo, &mat);
    if (!ignoreScale &&
        (!WITHIN_E(mat.xx,1.0) || !WITHIN_E(mat.yy,1.0) ||
          !WITHIN_E(mat.xy,0.0) || !WITHIN_E(mat.yx,0.0)))
        return false;
  } else {
    Matrix mat = mTransform;
    if (!ignoreScale &&
        (!WITHIN_E(mat._11,1.0) || !WITHIN_E(mat._22,1.0) ||
          !WITHIN_E(mat._12,0.0) || !WITHIN_E(mat._21,0.0)))
        return false;
  }
#undef WITHIN_E

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
  Rectangle(r, true);
  SetPattern(pattern);
}

void
gfxContext::SetAntialiasMode(AntialiasMode mode)
{
  if (mCairo) {
    if (mode == MODE_ALIASED) {
        cairo_set_antialias(mCairo, CAIRO_ANTIALIAS_NONE);
    } else if (mode == MODE_COVERAGE) {
        cairo_set_antialias(mCairo, CAIRO_ANTIALIAS_DEFAULT);
    }
  } else {
    if (mode == MODE_ALIASED) {
      CurrentState().aaMode = AA_NONE;
    } else if (mode == MODE_COVERAGE) {
      CurrentState().aaMode = AA_SUBPIXEL;
    }
  }
}

gfxContext::AntialiasMode
gfxContext::CurrentAntialiasMode() const
{
  if (mCairo) {
    cairo_antialias_t aa = cairo_get_antialias(mCairo);
    if (aa == CAIRO_ANTIALIAS_NONE)
        return MODE_ALIASED;
    return MODE_COVERAGE;
  } else {
    if (CurrentState().aaMode == AA_NONE) {
      return MODE_ALIASED;
    }
    return MODE_COVERAGE;
  }
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
          SetDash(nullptr, 0, 0.0);
          break;
  }
}

void
gfxContext::SetDash(gfxFloat *dashes, int ndash, gfxFloat offset)
{
  if (mCairo) {
    cairo_set_dash(mCairo, dashes, ndash, offset);
  } else {
    AzureState &state = CurrentState();

    state.dashPattern.SetLength(ndash);
    for (int i = 0; i < ndash; i++) {
      state.dashPattern[i] = Float(dashes[i]);
    }
    state.strokeOptions.mDashLength = ndash;
    state.strokeOptions.mDashOffset = Float(offset);
    state.strokeOptions.mDashPattern = ndash ? state.dashPattern.Elements() : NULL;
  }
}

bool
gfxContext::CurrentDash(FallibleTArray<gfxFloat>& dashes, gfxFloat* offset) const
{
  if (mCairo) {
    int count = cairo_get_dash_count(mCairo);
    if (count <= 0 || !dashes.SetLength(count)) {
        return false;
    }
    cairo_get_dash(mCairo, dashes.Elements(), offset);
    return true;
  } else {
    const AzureState &state = CurrentState();
    int count = state.strokeOptions.mDashLength;

    if (count <= 0 || !dashes.SetLength(count)) {
      return false;
    }

    for (int i = 0; i < count; i++) {
      dashes[i] = state.dashPattern[i];
    }

    *offset = state.strokeOptions.mDashOffset;

    return true;
  }
}

gfxFloat
gfxContext::CurrentDashOffset() const
{
  if (mCairo) {
    if (cairo_get_dash_count(mCairo) <= 0) {
        return 0.0;
    }
    gfxFloat offset;
    cairo_get_dash(mCairo, NULL, &offset);
    return offset;
  } else {
    return CurrentState().strokeOptions.mDashOffset;
  }
}

void
gfxContext::SetLineWidth(gfxFloat width)
{
  if (mCairo) {
    cairo_set_line_width(mCairo, width);
  } else {
    CurrentState().strokeOptions.mLineWidth = Float(width);
  }
}

gfxFloat
gfxContext::CurrentLineWidth() const
{
  if (mCairo) {
    return cairo_get_line_width(mCairo);
  } else {
    return CurrentState().strokeOptions.mLineWidth;
  }
}

void
gfxContext::SetOperator(GraphicsOperator op)
{
  if (mCairo) {
    if (mFlags & FLAG_SIMPLIFY_OPERATORS) {
        if (op != OPERATOR_SOURCE &&
            op != OPERATOR_CLEAR &&
            op != OPERATOR_OVER)
            op = OPERATOR_OVER;
    }

    cairo_set_operator(mCairo, (cairo_operator_t)op);
  } else {
    if (op == OPERATOR_CLEAR) {
      CurrentState().opIsClear = true;
      return;
    }
    CurrentState().opIsClear = false;
    CurrentState().op = CompositionOpForOp(op);
  }
}

gfxContext::GraphicsOperator
gfxContext::CurrentOperator() const
{
  if (mCairo) {
    return (GraphicsOperator)cairo_get_operator(mCairo);
  } else {
    return ThebesOp(CurrentState().op);
  }
}

void
gfxContext::SetLineCap(GraphicsLineCap cap)
{
  if (mCairo) {
    cairo_set_line_cap(mCairo, (cairo_line_cap_t)cap);
  } else {
    CurrentState().strokeOptions.mLineCap = ToCapStyle(cap);
  }
}

gfxContext::GraphicsLineCap
gfxContext::CurrentLineCap() const
{
  if (mCairo) {
    return (GraphicsLineCap)cairo_get_line_cap(mCairo);
  } else {
    return ThebesLineCap(CurrentState().strokeOptions.mLineCap);
  }
}

void
gfxContext::SetLineJoin(GraphicsLineJoin join)
{
  if (mCairo) {
    cairo_set_line_join(mCairo, (cairo_line_join_t)join);
  } else {
    CurrentState().strokeOptions.mLineJoin = ToJoinStyle(join);
  }
}

gfxContext::GraphicsLineJoin
gfxContext::CurrentLineJoin() const
{
  if (mCairo) {
    return (GraphicsLineJoin)cairo_get_line_join(mCairo);
  } else {
    return ThebesLineJoin(CurrentState().strokeOptions.mLineJoin);
  }
}

void
gfxContext::SetMiterLimit(gfxFloat limit)
{
  if (mCairo) {
    cairo_set_miter_limit(mCairo, limit);
  } else {
    CurrentState().strokeOptions.mMiterLimit = Float(limit);
  }
}

gfxFloat
gfxContext::CurrentMiterLimit() const
{
  if (mCairo) {
    return cairo_get_miter_limit(mCairo);
  } else {
    return CurrentState().strokeOptions.mMiterLimit;
  }
}

void
gfxContext::SetFillRule(FillRule rule)
{
  if (mCairo) {
    cairo_set_fill_rule(mCairo, (cairo_fill_rule_t)rule);
  } else {
    CurrentState().fillRule = rule == FILL_RULE_WINDING ? FILL_WINDING : FILL_EVEN_ODD;
  }
}

gfxContext::FillRule
gfxContext::CurrentFillRule() const
{
  if (mCairo) {
    return (FillRule)cairo_get_fill_rule(mCairo);
  } else {
    return FILL_RULE_WINDING;
  }
}

// clipping
void
gfxContext::Clip(const gfxRect& rect)
{
  if (mCairo) {
    cairo_new_path(mCairo);
    cairo_rectangle(mCairo, rect.X(), rect.Y(), rect.Width(), rect.Height());
    cairo_clip(mCairo);
  } else {
    AzureState::PushedClip clip = { NULL, ToRect(rect), mTransform };
    CurrentState().pushedClips.AppendElement(clip);
    mDT->PushClipRect(ToRect(rect));
    NewPath();
  }
}

void
gfxContext::Clip()
{
  if (mCairo) {
    cairo_clip_preserve(mCairo);
  } else {
    if (mPathIsRect) {
      MOZ_ASSERT(!mTransformChanged);

      AzureState::PushedClip clip = { NULL, mRect, mTransform };
      CurrentState().pushedClips.AppendElement(clip);
      mDT->PushClipRect(mRect);
    } else {
      EnsurePath();
      mDT->PushClip(mPath);
      AzureState::PushedClip clip = { mPath, Rect(), mTransform };
      CurrentState().pushedClips.AppendElement(clip);
    }
  }
}

void
gfxContext::ResetClip()
{
  if (mCairo) {
    cairo_reset_clip(mCairo);
  } else {
    for (int i = mStateStack.Length() - 1; i >= 0; i--) {
      for (unsigned int c = 0; c < mStateStack[i].pushedClips.Length(); c++) {
        mDT->PopClip();
      }

      if (mStateStack[i].clipWasReset) {
        break;
      }
    }
    CurrentState().pushedClips.Clear();
    CurrentState().clipWasReset = true;
  }
}

void
gfxContext::UpdateSurfaceClip()
{
  if (mCairo) {
    NewPath();
    // we paint an empty rectangle to ensure the clip is propagated to
    // the destination surface
    SetDeviceColor(gfxRGBA(0,0,0,0));
    Rectangle(gfxRect(0,1,1,0));
    Fill();
  }
}

gfxRect
gfxContext::GetClipExtents()
{
  if (mCairo) {
    double xmin, ymin, xmax, ymax;
    cairo_clip_extents(mCairo, &xmin, &ymin, &xmax, &ymax);
    return gfxRect(xmin, ymin, xmax - xmin, ymax - ymin);
  } else {
    Rect rect = GetAzureDeviceSpaceClipBounds();

    if (rect.width == 0 || rect.height == 0) {
      return gfxRect(0, 0, 0, 0);
    }

    Matrix mat = mTransform;
    mat.Invert();
    rect = mat.TransformBounds(rect);

    return ThebesRect(rect);
  }
}

bool
gfxContext::ClipContainsRect(const gfxRect& aRect)
{
  if (mCairo) {
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
  } else {
    unsigned int lastReset = 0;
    for (int i = mStateStack.Length() - 2; i > 0; i--) {
      if (mStateStack[i].clipWasReset) {
        lastReset = i;
      }
    }

    // Since we always return false when the clip list contains a
    // non-rectangular clip or a non-rectilinear transform, our 'total' clip
    // is always a rectangle if we hit the end of this function.
    Rect clipBounds(0, 0, Float(mDT->GetSize().width), Float(mDT->GetSize().height));

    for (unsigned int i = lastReset; i < mStateStack.Length(); i++) {
      for (unsigned int c = 0; c < mStateStack[i].pushedClips.Length(); c++) {
        AzureState::PushedClip &clip = mStateStack[i].pushedClips[c];
        if (clip.path || !clip.transform.IsRectilinear()) {
          // Cairo behavior is we return false if the clip contains a non-
          // rectangle.
          return false;
        } else {
          Rect clipRect = mTransform.TransformBounds(clip.rect);

          clipBounds.IntersectRect(clipBounds, clipRect);
        }
      }
    }

    return clipBounds.Contains(ToRect(aRect));
  }
}

// rendering sources

void
gfxContext::SetColor(const gfxRGBA& c)
{
  if (mCairo) {
    if (gfxPlatform::GetCMSMode() == eCMSMode_All) {

        gfxRGBA cms;
        qcms_transform *transform = gfxPlatform::GetCMSRGBTransform();
        if (transform)
          gfxPlatform::TransformPixel(c, cms, transform);

        // Use the original alpha to avoid unnecessary float->byte->float
        // conversion errors
        cairo_set_source_rgba(mCairo, cms.r, cms.g, cms.b, c.a);
    }
    else
        cairo_set_source_rgba(mCairo, c.r, c.g, c.b, c.a);
  } else {
    CurrentState().pattern = NULL;
    CurrentState().sourceSurfCairo = NULL;
    CurrentState().sourceSurface = NULL;

    if (gfxPlatform::GetCMSMode() == eCMSMode_All) {

        gfxRGBA cms;
        qcms_transform *transform = gfxPlatform::GetCMSRGBTransform();
        if (transform)
          gfxPlatform::TransformPixel(c, cms, transform);

        // Use the original alpha to avoid unnecessary float->byte->float
        // conversion errors
        CurrentState().color = ToColor(cms);
    }
    else
        CurrentState().color = ToColor(c);
  }
}

void
gfxContext::SetDeviceColor(const gfxRGBA& c)
{
  if (mCairo) {
    cairo_set_source_rgba(mCairo, c.r, c.g, c.b, c.a);
  } else {
    CurrentState().pattern = NULL;
    CurrentState().sourceSurfCairo = NULL;
    CurrentState().sourceSurface = NULL;
    CurrentState().color = ToColor(c);
  }
}

bool
gfxContext::GetDeviceColor(gfxRGBA& c)
{
  if (mCairo) {
    return cairo_pattern_get_rgba(cairo_get_source(mCairo),
                                  &c.r,
                                  &c.g,
                                  &c.b,
                                  &c.a) == CAIRO_STATUS_SUCCESS;
  } else {
    if (CurrentState().sourceSurface) {
      return false;
    }
    if (CurrentState().pattern) {
      gfxRGBA color;
      return CurrentState().pattern->GetSolidColor(c);
    }

    c = ThebesRGBA(CurrentState().color);
    return true;
  }
}

void
gfxContext::SetSource(gfxASurface *surface, const gfxPoint& offset)
{
  if (mCairo) {
    NS_ASSERTION(surface->GetAllowUseAsSource(), "Surface not allowed to be used as source!");
    cairo_set_source_surface(mCairo, surface->CairoSurface(), offset.x, offset.y);
  } else {
    CurrentState().surfTransform = Matrix(1.0f, 0, 0, 1.0f, Float(offset.x), Float(offset.y));
    CurrentState().pattern = NULL;
    CurrentState().patternTransformChanged = false;
    // Keep the underlying cairo surface around while we keep the
    // sourceSurface.
    CurrentState().sourceSurfCairo = surface;
    CurrentState().sourceSurface =
      gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(mDT, surface);
  }
}

void
gfxContext::SetPattern(gfxPattern *pattern)
{
  if (mCairo) {
    cairo_set_source(mCairo, pattern->CairoPattern());
  } else {
    CurrentState().sourceSurfCairo = NULL;
    CurrentState().sourceSurface = NULL;
    CurrentState().patternTransformChanged = false;
    CurrentState().pattern = pattern;
  }
}

already_AddRefed<gfxPattern>
gfxContext::GetPattern()
{
  if (mCairo) {
    cairo_pattern_t *pat = cairo_get_source(mCairo);
    NS_ASSERTION(pat, "I was told this couldn't be null");

    nsRefPtr<gfxPattern> wrapper;
    if (pat)
        wrapper = new gfxPattern(pat);
    else
        wrapper = new gfxPattern(gfxRGBA(0,0,0,0));

    return wrapper.forget();
  } else {
    nsRefPtr<gfxPattern> pat;
    
    AzureState &state = CurrentState();
    if (state.pattern) {
      pat = state.pattern;
    } else if (state.sourceSurface) {
      NS_ASSERTION(false, "Ugh, this isn't good.");
    } else {
      pat = new gfxPattern(ThebesRGBA(state.color));
    }
    return pat.forget();
  }
}


// masking
void
gfxContext::Mask(gfxPattern *pattern)
{
  if (mCairo) {
    cairo_mask(mCairo, pattern->CairoPattern());
  } else {
    if (pattern->Extend() == gfxPattern::EXTEND_NONE) {
      // In this situation the mask will be fully transparent (i.e. nothing
      // will be drawn) outside of the bounds of the surface. We can support
      // that by clipping out drawing to that area.
      Point offset;
      if (pattern->IsAzure()) {
        // This is an Azure pattern. i.e. this was the result of a PopGroup and
        // then the extend mode was changed to EXTEND_NONE.
        // XXX - We may need some additional magic here in theory to support
        // device offsets in these patterns, but no problems have been observed
        // yet because of this. And it would complicate things a little further.
        offset = Point(0.f, 0.f);
      } else if (pattern->GetType() == gfxPattern::PATTERN_SURFACE) {
        nsRefPtr<gfxASurface> asurf = pattern->GetSurface();
        gfxPoint deviceOffset = asurf->GetDeviceOffset();
        offset = Point(-deviceOffset.x, -deviceOffset.y);

        // this lets GetAzureSurface work
        pattern->GetPattern(mDT);
      }

      if (pattern->IsAzure() || pattern->GetType() == gfxPattern::PATTERN_SURFACE) {
        RefPtr<SourceSurface> mask = pattern->GetAzureSurface();
        Matrix mat = ToMatrix(pattern->GetInverseMatrix());
        Matrix old = mTransform;
        // add in the inverse of the pattern transform so that when we
        // MaskSurface we are transformed to the place matching the pattern transform
        mat = mat * mTransform;

        ChangeTransform(mat);
        mDT->MaskSurface(GeneralPattern(this), mask, offset, DrawOptions(1.0f, CurrentState().op, CurrentState().aaMode));
        ChangeTransform(old);
        return;
      }
    }
    mDT->Mask(GeneralPattern(this), *pattern->GetPattern(mDT), DrawOptions(1.0f, CurrentState().op, CurrentState().aaMode));
  }
}

void
gfxContext::Mask(gfxASurface *surface, const gfxPoint& offset)
{
  PROFILER_LABEL("gfxContext", "Mask");
  if (mCairo) {
    cairo_mask_surface(mCairo, surface->CairoSurface(), offset.x, offset.y);
  } else {
    // Lifetime needs to be limited here as we may simply wrap surface's data.
    RefPtr<SourceSurface> sourceSurf =
      gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(mDT, surface);

    gfxPoint pt = surface->GetDeviceOffset();

    // We clip here to bind to the mask surface bounds, see above.
    mDT->MaskSurface(GeneralPattern(this), 
              sourceSurf,
              Point(offset.x - pt.x, offset.y -  pt.y),
              DrawOptions(1.0f, CurrentState().op, CurrentState().aaMode));
  }
}

void
gfxContext::Paint(gfxFloat alpha)
{
  PROFILER_LABEL("gfxContext", "Paint");
  if (mCairo) {
    cairo_paint_with_alpha(mCairo, alpha);
  } else {
    AzureState &state = CurrentState();

    Matrix mat = mDT->GetTransform();
    mat.Invert();
    Rect paintRect = mat.TransformBounds(Rect(Point(0, 0), Size(mDT->GetSize())));

    if (state.opIsClear) {
      mDT->ClearRect(paintRect);
    } else {
      mDT->FillRect(paintRect, GeneralPattern(this),
                    DrawOptions(Float(alpha), GetOp()));
    }
  }
}

// groups

void
gfxContext::PushGroup(gfxASurface::gfxContentType content)
{
  if (mCairo) {
    cairo_push_group_with_content(mCairo, (cairo_content_t) content);
  } else {
    PushNewDT(content);

    PushClipsToDT(mDT);
    mDT->SetTransform(GetDTTransform());
  }
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
  if (mCairo) {
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
          for (uint32_t i = 0; i < ss.Length(); ++i) {
              CopySurface(ss[i], ds[i], translation);
          }
        } else {
          CopySurface(s, d, gfxPoint(0, 0));
        }
        d->SetOpaqueRect(s->GetOpaqueRect());
        return;
      }
    }
  } else {
    IntRect clipExtents;
    if (mDT->GetFormat() != FORMAT_B8G8R8X8) {
      gfxRect clipRect = GetRoundOutDeviceClipExtents(this);
      clipExtents = IntRect(clipRect.x, clipRect.y, clipRect.width, clipRect.height);
    }
    if (mDT->GetFormat() == FORMAT_B8G8R8X8 ||
        mDT->GetOpaqueRect().Contains(clipExtents)) {
      DrawTarget *oldDT = mDT;
      RefPtr<SourceSurface> source = mDT->Snapshot();
      Point oldDeviceOffset = CurrentState().deviceOffset;

      PushNewDT(gfxASurface::CONTENT_COLOR);

      Point offset = CurrentState().deviceOffset - oldDeviceOffset;
      Rect surfRect(0, 0, Float(mDT->GetSize().width), Float(mDT->GetSize().height));
      Rect sourceRect = surfRect;
      sourceRect.x += offset.x;
      sourceRect.y += offset.y;

      mDT->SetTransform(Matrix());
      mDT->DrawSurface(source, surfRect, sourceRect);
      mDT->SetOpaqueRect(oldDT->GetOpaqueRect());

      PushClipsToDT(mDT);
      mDT->SetTransform(GetDTTransform());
      return;
    }
  }
  PushGroup(content);
}

already_AddRefed<gfxPattern>
gfxContext::PopGroup()
{
  if (mCairo) {
    cairo_pattern_t *pat = cairo_pop_group(mCairo);
    nsRefPtr<gfxPattern> wrapper = new gfxPattern(pat);
    cairo_pattern_destroy(pat);
    return wrapper.forget();
  } else {
    RefPtr<SourceSurface> src = mDT->Snapshot();
    Point deviceOffset = CurrentState().deviceOffset;

    Restore();

    Matrix mat = mTransform;
    mat.Invert();

    Matrix deviceOffsetTranslation;
    deviceOffsetTranslation.Translate(deviceOffset.x, deviceOffset.y);

    nsRefPtr<gfxPattern> pat = new gfxPattern(src, deviceOffsetTranslation * mat);

    return pat.forget();
  }
}

void
gfxContext::PopGroupToSource()
{
  if (mCairo) {
    cairo_pop_group_to_source(mCairo);
  } else {
    RefPtr<SourceSurface> src = mDT->Snapshot();
    Point deviceOffset = CurrentState().deviceOffset;
    Restore();
    CurrentState().sourceSurfCairo = NULL;
    CurrentState().sourceSurface = src;
    CurrentState().pattern = NULL;
    CurrentState().patternTransformChanged = false;

    Matrix mat = mTransform;
    mat.Invert();

    Matrix deviceOffsetTranslation;
    deviceOffsetTranslation.Translate(deviceOffset.x, deviceOffset.y);
    CurrentState().surfTransform = deviceOffsetTranslation * mat;
  }
}

bool
gfxContext::PointInFill(const gfxPoint& pt)
{
  if (mCairo) {
    return cairo_in_fill(mCairo, pt.x, pt.y);
  } else {
    return mPath->ContainsPoint(ToPoint(pt), mTransform);
  }
}

bool
gfxContext::PointInStroke(const gfxPoint& pt)
{
  if (mCairo) {
    return cairo_in_stroke(mCairo, pt.x, pt.y);
  } else {
    return mPath->StrokeContainsPoint(CurrentState().strokeOptions,
                                      ToPoint(pt),
                                      mTransform);
  }
}

gfxRect
gfxContext::GetUserPathExtent()
{
  if (mCairo) {
    double xmin, ymin, xmax, ymax;
    cairo_path_extents(mCairo, &xmin, &ymin, &xmax, &ymax);
    return gfxRect(xmin, ymin, xmax - xmin, ymax - ymin);
  } else {
    return ThebesRect(mPath->GetBounds());
  }
}

gfxRect
gfxContext::GetUserFillExtent()
{
  if (mCairo) {
    double xmin, ymin, xmax, ymax;
    cairo_fill_extents(mCairo, &xmin, &ymin, &xmax, &ymax);
    return gfxRect(xmin, ymin, xmax - xmin, ymax - ymin);
  } else {
    return ThebesRect(mPath->GetBounds());
  }
}

gfxRect
gfxContext::GetUserStrokeExtent()
{
  if (mCairo) {
    double xmin, ymin, xmax, ymax;
    cairo_stroke_extents(mCairo, &xmin, &ymin, &xmax, &ymax);
    return gfxRect(xmin, ymin, xmax - xmin, ymax - ymin);
  } else {
    return ThebesRect(mPath->GetStrokedBounds(CurrentState().strokeOptions, mTransform));
  }
}

already_AddRefed<gfxFlattenedPath>
gfxContext::GetFlattenedPath()
{
  if (mCairo) {
    nsRefPtr<gfxFlattenedPath> path =
        new gfxFlattenedPath(cairo_copy_path_flat(mCairo));
    return path.forget();
  } else {
    // XXX - Used by SVG, needs fixing.
    return nullptr;
  }
}

bool
gfxContext::HasError()
{
  if (mCairo) {
    return cairo_status(mCairo) != CAIRO_STATUS_SUCCESS;
  } else {
    // As far as this is concerned, an Azure context is never in error.
    return false;
  }
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

  if (mCairo) {
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
  } else {
    EnsurePathBuilder();

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
        mPathBuilder->MoveTo(Point(Float(rect.X() + corners[NS_CORNER_TOP_LEFT].width), Float(rect.Y())));
    else
        mPathBuilder->MoveTo(Point(Float(rect.X() + rect.Width() - corners[NS_CORNER_TOP_RIGHT].width), Float(rect.Y())));

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

            mPathBuilder->LineTo(ToPoint(p0));
            mPathBuilder->BezierTo(ToPoint(p1), ToPoint(p2), ToPoint(p3));
        } else {
            mPathBuilder->LineTo(ToPoint(pc));
        }
    }

    mPathBuilder->Close();
  }
}

#ifdef MOZ_DUMP_PAINTING
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

void
gfxContext::EnsurePath()
{
  if (mPathBuilder) {
    mPath = mPathBuilder->Finish();
    mPathBuilder = NULL;
  }

  if (mPath) {
    if (mTransformChanged) {
      Matrix mat = mTransform;
      mat.Invert();
      mat = mPathTransform * mat;
      mPathBuilder = mPath->TransformedCopyToBuilder(mat, CurrentState().fillRule);
      mPath = mPathBuilder->Finish();
      mPathBuilder = NULL;

      mTransformChanged = false;
    }

    if (CurrentState().fillRule == mPath->GetFillRule()) {
      return;
    }

    mPathBuilder = mPath->CopyToBuilder(CurrentState().fillRule);

    mPath = mPathBuilder->Finish();
    mPathBuilder = NULL;
    return;
  }

  EnsurePathBuilder();
  mPath = mPathBuilder->Finish();
  mPathBuilder = NULL;
}

void
gfxContext::EnsurePathBuilder()
{
  if (mPathBuilder && !mTransformChanged) {
    return;
  }

  if (mPath) {
    if (!mTransformChanged) {
      mPathBuilder = mPath->CopyToBuilder(CurrentState().fillRule);
      mPath = NULL;
    } else {
      Matrix invTransform = mTransform;
      invTransform.Invert();
      Matrix toNewUS = mPathTransform * invTransform;
      mPathBuilder = mPath->TransformedCopyToBuilder(toNewUS, CurrentState().fillRule);
    }
    return;
  }

  DebugOnly<PathBuilder*> oldPath = mPathBuilder.get();

  if (!mPathBuilder) {
    mPathBuilder = mDT->CreatePathBuilder(CurrentState().fillRule);

    if (mPathIsRect) {
      mPathBuilder->MoveTo(mRect.TopLeft());
      mPathBuilder->LineTo(mRect.TopRight());
      mPathBuilder->LineTo(mRect.BottomRight());
      mPathBuilder->LineTo(mRect.BottomLeft());
      mPathBuilder->Close();
    }
  }

  if (mTransformChanged) {
    // This could be an else if since this should never happen when
    // mPathBuilder is NULL and mPath is NULL. But this way we can assert
    // if all the state is as expected.
    MOZ_ASSERT(oldPath);
    MOZ_ASSERT(!mPathIsRect);

    Matrix invTransform = mTransform;
    invTransform.Invert();
    Matrix toNewUS = mPathTransform * invTransform;

    RefPtr<Path> path = mPathBuilder->Finish();
    mPathBuilder = path->TransformedCopyToBuilder(toNewUS, CurrentState().fillRule);
  }

  mPathIsRect = false;
}

void
gfxContext::FillAzure(Float aOpacity)
{
  AzureState &state = CurrentState();

  CompositionOp op = GetOp();

  if (mPathIsRect) {
    MOZ_ASSERT(!mTransformChanged);

    if (state.opIsClear) {
      mDT->ClearRect(mRect);
    } else if (op == OP_SOURCE) {
      // Emulate cairo operator source which is bound by mask!
      mDT->ClearRect(mRect);
      mDT->FillRect(mRect, GeneralPattern(this), DrawOptions(aOpacity));
    } else {
      mDT->FillRect(mRect, GeneralPattern(this), DrawOptions(aOpacity, op, state.aaMode));
    }
  } else {
    EnsurePath();

    NS_ASSERTION(!state.opIsClear, "We shouldn't be clearing complex paths!");

    mDT->Fill(mPath, GeneralPattern(this), DrawOptions(aOpacity, op, state.aaMode));
  }
}

void
gfxContext::PushClipsToDT(DrawTarget *aDT)
{
  // Tricky, we have to restore all clips -since the last time- the clip
  // was reset. If we didn't reset the clip, just popping the clips we
  // added was fine.
  unsigned int lastReset = 0;
  for (int i = mStateStack.Length() - 2; i > 0; i--) {
    if (mStateStack[i].clipWasReset) {
      lastReset = i;
    }
  }

  // Don't need to save the old transform, we'll be setting a new one soon!

  // Push all clips from the last state on the stack where the clip was
  // reset to the clip before ours.
  for (unsigned int i = lastReset; i < mStateStack.Length() - 1; i++) {
    for (unsigned int c = 0; c < mStateStack[i].pushedClips.Length(); c++) {
      aDT->SetTransform(mStateStack[i].pushedClips[c].transform * GetDeviceTransform());
      if (mStateStack[i].pushedClips[c].path) {
        aDT->PushClip(mStateStack[i].pushedClips[c].path);
      } else {
        aDT->PushClipRect(mStateStack[i].pushedClips[c].rect);
      }
    }
  }
}

CompositionOp
gfxContext::GetOp()
{
  if (CurrentState().op != OP_SOURCE) {
    return CurrentState().op;
  }

  AzureState &state = CurrentState();
  if (state.pattern) {
    if (state.pattern->IsOpaque()) {
      return OP_OVER;
    } else {
      return OP_SOURCE;
    }
  } else if (state.sourceSurface) {
    if (state.sourceSurface->GetFormat() == FORMAT_B8G8R8X8) {
      return OP_OVER;
    } else {
      return OP_SOURCE;
    }
  } else {
    if (state.color.a > 0.999) {
      return OP_OVER;
    } else {
      return OP_SOURCE;
    }
  }
}

/* SVG font code can change the transform after having set the pattern on the
 * context. When the pattern is set it is in user space, if the transform is
 * changed after doing so the pattern needs to be converted back into userspace.
 * We just store the old pattern transform here so that we only do the work
 * needed here if the pattern is actually used.
 * We need to avoid doing this when this ChangeTransform comes from a restore,
 * since the current pattern and the current transform are both part of the
 * state we know the new CurrentState()'s values are valid. But if we assume
 * a change they might become invalid since patternTransformChanged is part of
 * the state and might be false for the restored AzureState.
 */
void
gfxContext::ChangeTransform(const Matrix &aNewMatrix, bool aUpdatePatternTransform)
{
  AzureState &state = CurrentState();

  if (aUpdatePatternTransform && (state.pattern || state.sourceSurface)
      && !state.patternTransformChanged) {
    state.patternTransform = mTransform;
    state.patternTransformChanged = true;
  }

  if (mPathIsRect) {
    Matrix invMatrix = aNewMatrix;
    
    invMatrix.Invert();

    Matrix toNewUS = mTransform * invMatrix;

    if (toNewUS.IsRectilinear()) {
      mRect = toNewUS.TransformBounds(mRect);
      mRect.NudgeToIntegers();
    } else {
      mPathBuilder = mDT->CreatePathBuilder(CurrentState().fillRule);
      
      mPathBuilder->MoveTo(toNewUS * mRect.TopLeft());
      mPathBuilder->LineTo(toNewUS * mRect.TopRight());
      mPathBuilder->LineTo(toNewUS * mRect.BottomRight());
      mPathBuilder->LineTo(toNewUS * mRect.BottomLeft());
      mPathBuilder->Close();

      mPathIsRect = false;
    }

    // No need to consider the transform changed now!
    mTransformChanged = false;
  } else if ((mPath || mPathBuilder) && !mTransformChanged) {
    mTransformChanged = true;
    mPathTransform = mTransform;
  }

  mTransform = aNewMatrix;

  mDT->SetTransform(GetDTTransform());
}

Rect
gfxContext::GetAzureDeviceSpaceClipBounds()
{
  unsigned int lastReset = 0;
  for (int i = mStateStack.Length() - 1; i > 0; i--) {
    if (mStateStack[i].clipWasReset) {
      lastReset = i;
    }
  }

  Rect rect(CurrentState().deviceOffset.x, CurrentState().deviceOffset.y,
            Float(mDT->GetSize().width), Float(mDT->GetSize().height));
  for (unsigned int i = lastReset; i < mStateStack.Length(); i++) {
    for (unsigned int c = 0; c < mStateStack[i].pushedClips.Length(); c++) {
      AzureState::PushedClip &clip = mStateStack[i].pushedClips[c];
      if (clip.path) {
        Rect bounds = clip.path->GetBounds(clip.transform);
        rect.IntersectRect(rect, bounds);
      } else {
        rect.IntersectRect(rect, clip.transform.TransformBounds(clip.rect));
      }
    }
  }

  return rect;
}

Matrix
gfxContext::GetDeviceTransform() const
{
  Matrix mat;
  mat.Translate(-CurrentState().deviceOffset.x, -CurrentState().deviceOffset.y);
  return mat;
}

Matrix
gfxContext::GetDTTransform() const
{
  Matrix mat = mTransform;
  mat._31 -= CurrentState().deviceOffset.x;
  mat._32 -= CurrentState().deviceOffset.y;
  return mat;
}

void
gfxContext::PushNewDT(gfxASurface::gfxContentType content)
{
  Rect clipBounds = GetAzureDeviceSpaceClipBounds();
  clipBounds.RoundOut();

  clipBounds.width = std::max(1.0f, clipBounds.width);
  clipBounds.height = std::max(1.0f, clipBounds.height);

  RefPtr<DrawTarget> newDT =
    mDT->CreateSimilarDrawTarget(IntSize(int32_t(clipBounds.width), int32_t(clipBounds.height)),
                                  gfxPlatform::GetPlatform()->Optimal2DFormatForContent(content));

  Save();

  CurrentState().drawTarget = newDT;
  CurrentState().deviceOffset = clipBounds.TopLeft();

  mDT = newDT;
}

/**
 * Work out whether cairo will snap inter-glyph spacing to pixels.
 *
 * Layout does not align text to pixel boundaries, so, with font drawing
 * backends that snap glyph positions to pixels, it is important that
 * inter-glyph spacing within words is always an integer number of pixels.
 * This ensures that the drawing backend snaps all of the word's glyphs in the
 * same direction and so inter-glyph spacing remains the same.
 */
void
gfxContext::GetRoundOffsetsToPixels(bool *aRoundX, bool *aRoundY)
{
    *aRoundX = false;
    // Could do something fancy here for ScaleFactors of
    // AxisAlignedTransforms, but we leave things simple.
    // Not much point rounding if a matrix will mess things up anyway.
    // Also return false for non-cairo contexts.
    if (CurrentMatrix().HasNonTranslation() || mDT) {
        *aRoundY = false;
        return;
    }

    // All raster backends snap glyphs to pixels vertically.
    // Print backends set CAIRO_HINT_METRICS_OFF.
    *aRoundY = true;

    cairo_t *cr = GetCairo();
    cairo_scaled_font_t *scaled_font = cairo_get_scaled_font(cr);
    // Sometimes hint metrics gets set for us, most notably for printing.
    cairo_font_options_t *font_options = cairo_font_options_create();
    cairo_scaled_font_get_font_options(scaled_font, font_options);
    cairo_hint_metrics_t hint_metrics =
        cairo_font_options_get_hint_metrics(font_options);
    cairo_font_options_destroy(font_options);

    switch (hint_metrics) {
    case CAIRO_HINT_METRICS_OFF:
        *aRoundY = false;
        return;
    case CAIRO_HINT_METRICS_DEFAULT:
        // Here we mimic what cairo surface/font backends do.  Printing
        // surfaces have already been handled by hint_metrics.  The
        // fallback show_glyphs implementation composites pixel-aligned
        // glyph surfaces, so we just pick surface/font combinations that
        // override this.
        switch (cairo_scaled_font_get_type(scaled_font)) {
#if CAIRO_HAS_DWRITE_FONT // dwrite backend is not in std cairo releases yet
        case CAIRO_FONT_TYPE_DWRITE:
            // show_glyphs is implemented on the font and so is used for
            // all surface types; however, it may pixel-snap depending on
            // the dwrite rendering mode
            if (!cairo_dwrite_scaled_font_get_force_GDI_classic(scaled_font) &&
                gfxWindowsPlatform::GetPlatform()->DWriteMeasuringMode() ==
                    DWRITE_MEASURING_MODE_NATURAL) {
                return;
            }
#endif
        case CAIRO_FONT_TYPE_QUARTZ:
            // Quartz surfaces implement show_glyphs for Quartz fonts
            if (cairo_surface_get_type(cairo_get_target(cr)) ==
                CAIRO_SURFACE_TYPE_QUARTZ) {
                return;
            }
        default:
            break;
        }
        // fall through:
    case CAIRO_HINT_METRICS_ON:
        break;
    }
    *aRoundX = true;
    return;
}
