/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PathCG.h"
#include <math.h>
#include "Logging.h"
#include "PathHelpers.h"

namespace mozilla {
namespace gfx {

static inline Rect
CGRectToRect(CGRect rect)
{
  return Rect(rect.origin.x,
              rect.origin.y,
              rect.size.width,
              rect.size.height);
}

static inline Point
CGPointToPoint(CGPoint point)
{
  return Point(point.x, point.y);
}

static inline void
SetStrokeOptions(CGContextRef cg, const StrokeOptions &aStrokeOptions)
{
  switch (aStrokeOptions.mLineCap)
  {
    case CapStyle::BUTT:
      CGContextSetLineCap(cg, kCGLineCapButt);
      break;
    case CapStyle::ROUND:
      CGContextSetLineCap(cg, kCGLineCapRound);
      break;
    case CapStyle::SQUARE:
      CGContextSetLineCap(cg, kCGLineCapSquare);
      break;
  }
 
  switch (aStrokeOptions.mLineJoin)
  {
    case JoinStyle::BEVEL:
      CGContextSetLineJoin(cg, kCGLineJoinBevel);
      break;
    case JoinStyle::ROUND:
      CGContextSetLineJoin(cg, kCGLineJoinRound);
      break;
    case JoinStyle::MITER:
    case JoinStyle::MITER_OR_BEVEL:
      CGContextSetLineJoin(cg, kCGLineJoinMiter);
      break;
  }
 
  CGContextSetLineWidth(cg, aStrokeOptions.mLineWidth);
  CGContextSetMiterLimit(cg, aStrokeOptions.mMiterLimit);
 
  // XXX: rename mDashLength to dashLength
  if (aStrokeOptions.mDashLength > 0) {
    // we use a regular array instead of a std::vector here because we don't want to leak the <vector> include
    CGFloat *dashes = new CGFloat[aStrokeOptions.mDashLength];
    for (size_t i=0; i<aStrokeOptions.mDashLength; i++) {
      dashes[i] = aStrokeOptions.mDashPattern[i];
    }
    CGContextSetLineDash(cg, aStrokeOptions.mDashOffset, dashes, aStrokeOptions.mDashLength);
    delete[] dashes;
  }
}

static inline CGAffineTransform
GfxMatrixToCGAffineTransform(const Matrix &m)
{
  CGAffineTransform t;
  t.a = m._11;
  t.b = m._12;
  t.c = m._21;
  t.d = m._22;
  t.tx = m._31;
  t.ty = m._32;
  return t;
}

PathBuilderCG::~PathBuilderCG()
{
  CGPathRelease(mCGPath);
}

void
PathBuilderCG::MoveTo(const Point &aPoint)
{
  if (!aPoint.IsFinite()) {
    return;
  }
  CGPathMoveToPoint(mCGPath, nullptr, aPoint.x, aPoint.y);
}

void
PathBuilderCG::LineTo(const Point &aPoint)
{
  if (!aPoint.IsFinite()) {
    return;
  }

  if (CGPathIsEmpty(mCGPath))
    MoveTo(aPoint);
  else
    CGPathAddLineToPoint(mCGPath, nullptr, aPoint.x, aPoint.y);
}

void
PathBuilderCG::BezierTo(const Point &aCP1,
                         const Point &aCP2,
                         const Point &aCP3)
{
  if (!aCP1.IsFinite() || !aCP2.IsFinite() || !aCP3.IsFinite()) {
    return;
  }

  if (CGPathIsEmpty(mCGPath))
    MoveTo(aCP1);
  CGPathAddCurveToPoint(mCGPath, nullptr,
                          aCP1.x, aCP1.y,
                          aCP2.x, aCP2.y,
                          aCP3.x, aCP3.y);

}

void
PathBuilderCG::QuadraticBezierTo(const Point &aCP1,
                                 const Point &aCP2)
{
  if (!aCP1.IsFinite() || !aCP2.IsFinite()) {
    return;
  }

  if (CGPathIsEmpty(mCGPath))
    MoveTo(aCP1);
  CGPathAddQuadCurveToPoint(mCGPath, nullptr,
                            aCP1.x, aCP1.y,
                            aCP2.x, aCP2.y);
}

void
PathBuilderCG::Close()
{
  if (!CGPathIsEmpty(mCGPath))
    CGPathCloseSubpath(mCGPath);
}

void
PathBuilderCG::Arc(const Point &aOrigin, Float aRadius, Float aStartAngle,
                 Float aEndAngle, bool aAntiClockwise)
{
  if (!aOrigin.IsFinite() || !IsFinite(aRadius) ||
      !IsFinite(aStartAngle) || !IsFinite(aEndAngle)) {
    return;
  }

  // Disabled for now due to a CG bug when using CGPathAddArc with stroke
  // dashing and rotation transforms that are multiples of 90 degrees. See:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=949661#c8
#if 0
  // Core Graphic's initial coordinate system is y-axis up, whereas Moz2D's is
  // y-axis down. Core Graphics therefore considers "clockwise" to mean "sweep
  // in the direction of decreasing angle" whereas Moz2D considers it to mean
  // "sweep in the direction of increasing angle". In other words if this
  // Moz2D method is instructed to sweep anti-clockwise we need to tell
  // CGPathAddArc to sweep clockwise, and vice versa. Hence why we pass the
  // value of aAntiClockwise directly to CGPathAddArc's "clockwise" bool
  // parameter.
  CGPathAddArc(mCGPath, nullptr,
               aOrigin.x, aOrigin.y,
               aRadius,
               aStartAngle,
               aEndAngle,
               aAntiClockwise);
#endif
  ArcToBezier(this, aOrigin, Size(aRadius, aRadius), aStartAngle, aEndAngle,
              aAntiClockwise);
}

Point
PathBuilderCG::CurrentPoint() const
{
  Point ret;
  if (!CGPathIsEmpty(mCGPath)) {
    CGPoint pt = CGPathGetCurrentPoint(mCGPath);
    ret.MoveTo(pt.x, pt.y);
  }
  return ret;
}

void
PathBuilderCG::EnsureActive(const Point &aPoint)
{
}

already_AddRefed<Path>
PathBuilderCG::Finish()
{
  return MakeAndAddRef<PathCG>(mCGPath, mFillRule);
}

already_AddRefed<PathBuilder>
PathCG::CopyToBuilder(FillRule aFillRule) const
{
  CGMutablePathRef path = CGPathCreateMutableCopy(mPath);
  return MakeAndAddRef<PathBuilderCG>(path, aFillRule);
}



already_AddRefed<PathBuilder>
PathCG::TransformedCopyToBuilder(const Matrix &aTransform, FillRule aFillRule) const
{
  // 10.7 adds CGPathCreateMutableCopyByTransformingPath it might be faster than doing
  // this by hand

  struct TransformApplier {
    CGMutablePathRef path;
    CGAffineTransform transform;
    static void
    TranformCGPathApplierFunc(void *vinfo, const CGPathElement *element)
    {
      TransformApplier *info = reinterpret_cast<TransformApplier*>(vinfo);
      switch (element->type) {
        case kCGPathElementMoveToPoint:
          {
            CGPoint pt = element->points[0];
            CGPathMoveToPoint(info->path, &info->transform, pt.x, pt.y);
            break;
          }
        case kCGPathElementAddLineToPoint:
          {
            CGPoint pt = element->points[0];
            CGPathAddLineToPoint(info->path, &info->transform, pt.x, pt.y);
            break;
          }
        case kCGPathElementAddQuadCurveToPoint:
          {
            CGPoint cpt = element->points[0];
            CGPoint pt  = element->points[1];
            CGPathAddQuadCurveToPoint(info->path, &info->transform, cpt.x, cpt.y, pt.x, pt.y);
            break;
          }
        case kCGPathElementAddCurveToPoint:
          {
            CGPoint cpt1 = element->points[0];
            CGPoint cpt2 = element->points[1];
            CGPoint pt   = element->points[2];
            CGPathAddCurveToPoint(info->path, &info->transform, cpt1.x, cpt1.y, cpt2.x, cpt2.y, pt.x, pt.y);
            break;
          }
        case kCGPathElementCloseSubpath:
          {
            CGPathCloseSubpath(info->path);
            break;
          }
      }
    }
  };

  TransformApplier ta;
  ta.path = CGPathCreateMutable();
  ta.transform = GfxMatrixToCGAffineTransform(aTransform);

  CGPathApply(mPath, &ta, TransformApplier::TranformCGPathApplierFunc);
  return MakeAndAddRef<PathBuilderCG>(ta.path, aFillRule);
}

static void
StreamPathToSinkApplierFunc(void *vinfo, const CGPathElement *element)
{
  PathSink *sink = reinterpret_cast<PathSink*>(vinfo);
  switch (element->type) {
    case kCGPathElementMoveToPoint:
      {
        CGPoint pt = element->points[0];
        sink->MoveTo(CGPointToPoint(pt));
        break;
      }
    case kCGPathElementAddLineToPoint:
      {
        CGPoint pt = element->points[0];
        sink->LineTo(CGPointToPoint(pt));
        break;
      }
    case kCGPathElementAddQuadCurveToPoint:
      {
        CGPoint cpt = element->points[0];
        CGPoint pt  = element->points[1];
        sink->QuadraticBezierTo(CGPointToPoint(cpt),
                                CGPointToPoint(pt));
        break;
      }
    case kCGPathElementAddCurveToPoint:
      {
        CGPoint cpt1 = element->points[0];
        CGPoint cpt2 = element->points[1];
        CGPoint pt   = element->points[2];
        sink->BezierTo(CGPointToPoint(cpt1),
                       CGPointToPoint(cpt2),
                       CGPointToPoint(pt));
        break;
      }
    case kCGPathElementCloseSubpath:
      {
        sink->Close();
        break;
      }
  }
}

void
PathCG::StreamToSink(PathSink *aSink) const
{
  CGPathApply(mPath, aSink, StreamPathToSinkApplierFunc);
}

bool
PathCG::ContainsPoint(const Point &aPoint, const Matrix &aTransform) const
{
  Matrix inverse = aTransform;
  inverse.Invert();
  Point transformedPoint = inverse.TransformPoint(aPoint);
  // We could probably drop the input transform and just transform the point at the caller?
  CGPoint point = {transformedPoint.x, transformedPoint.y};

  // The transform parameter of CGPathContainsPoint doesn't seem to work properly on OS X 10.5
  // so we transform aPoint ourselves.
  return CGPathContainsPoint(mPath, nullptr, point, mFillRule == FillRule::FILL_EVEN_ODD);
}

static size_t
PutBytesNull(void *info, const void *buffer, size_t count)
{
  return count;
}

/* The idea of a scratch context comes from WebKit */
static CGContextRef
CreateScratchContext()
{
  CGDataConsumerCallbacks callbacks = {PutBytesNull, nullptr};
  CGDataConsumerRef consumer = CGDataConsumerCreate(nullptr, &callbacks);
  CGContextRef cg = CGPDFContextCreate(consumer, nullptr, nullptr);
  CGDataConsumerRelease(consumer);
  return cg;
}

static CGContextRef
ScratchContext()
{
  static CGContextRef cg = CreateScratchContext();
  return cg;
}

bool
PathCG::StrokeContainsPoint(const StrokeOptions &aStrokeOptions,
                            const Point &aPoint,
                            const Matrix &aTransform) const
{
  Matrix inverse = aTransform;
  inverse.Invert();
  Point transformedPoint = inverse.TransformPoint(aPoint);
  // We could probably drop the input transform and just transform the point at the caller?
  CGPoint point = {transformedPoint.x, transformedPoint.y};

  CGContextRef cg = ScratchContext();

  CGContextSaveGState(cg);

  CGContextBeginPath(cg);
  CGContextAddPath(cg, mPath);

  SetStrokeOptions(cg, aStrokeOptions);

  CGContextReplacePathWithStrokedPath(cg);
  CGContextRestoreGState(cg);

  CGPathRef sPath = CGContextCopyPath(cg);
  bool inStroke = CGPathContainsPoint(sPath, nullptr, point, false);
  CGPathRelease(sPath);

  return inStroke;
}

//XXX: what should these functions return for an empty path?
// currently they return CGRectNull {inf,inf, 0, 0}
Rect
PathCG::GetBounds(const Matrix &aTransform) const
{
  //XXX: are these bounds tight enough
  Rect bounds = CGRectToRect(CGPathGetBoundingBox(mPath));

  //XXX: currently this returns the bounds of the transformed bounds
  // this is strictly looser than the bounds of the transformed path
  return aTransform.TransformBounds(bounds);
}

Rect
PathCG::GetStrokedBounds(const StrokeOptions &aStrokeOptions,
                         const Matrix &aTransform) const
{
  // 10.7 has CGPathCreateCopyByStrokingPath which we could use
  // instead of this scratch context business
  CGContextRef cg = ScratchContext();

  CGContextSaveGState(cg);

  CGContextBeginPath(cg);
  CGContextAddPath(cg, mPath);

  SetStrokeOptions(cg, aStrokeOptions);

  CGContextReplacePathWithStrokedPath(cg);
  Rect bounds = CGRectToRect(CGContextGetPathBoundingBox(cg));

  CGContextRestoreGState(cg);

  if (!bounds.IsFinite()) {
    return Rect();
  }

  return aTransform.TransformBounds(bounds);
}


} // namespace gfx

} // namespace mozilla
