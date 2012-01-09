/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
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

#include "PathCG.h"
#include <math.h>
#include "DrawTargetCG.h"
#include "Logging.h"

namespace mozilla {
namespace gfx {

PathBuilderCG::~PathBuilderCG()
{
  CGPathRelease(mCGPath);
}

void
PathBuilderCG::MoveTo(const Point &aPoint)
{
  CGPathMoveToPoint(mCGPath, NULL, aPoint.x, aPoint.y);
}

void
PathBuilderCG::LineTo(const Point &aPoint)
{
  if (CGPathIsEmpty(mCGPath))
    MoveTo(aPoint);
  else
    CGPathAddLineToPoint(mCGPath, NULL, aPoint.x, aPoint.y);
}

void
PathBuilderCG::BezierTo(const Point &aCP1,
                         const Point &aCP2,
                         const Point &aCP3)
{

  if (CGPathIsEmpty(mCGPath))
    MoveTo(aCP1);
  else
    CGPathAddCurveToPoint(mCGPath, NULL,
                          aCP1.x, aCP1.y,
                          aCP2.x, aCP2.y,
                          aCP3.x, aCP3.y);

}

void
PathBuilderCG::QuadraticBezierTo(const Point &aCP1,
                                  const Point &aCP2)
{
  if (CGPathIsEmpty(mCGPath))
    MoveTo(aCP1);
  else
    CGPathAddQuadCurveToPoint(mCGPath, NULL,
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
}

Point
PathBuilderCG::CurrentPoint() const
{
  CGPoint pt = CGPathGetCurrentPoint(mCGPath);
  Point ret(pt.x, pt.y);
  return ret;
}

void
PathBuilderCG::EnsureActive(const Point &aPoint)
{
}

TemporaryRef<Path>
PathBuilderCG::Finish()
{
  RefPtr<PathCG> path = new PathCG(mCGPath, mFillRule);
  return path;
}

TemporaryRef<PathBuilder>
PathCG::CopyToBuilder(FillRule aFillRule) const
{
  CGMutablePathRef path = CGPathCreateMutableCopy(mPath);
  RefPtr<PathBuilderCG> builder = new PathBuilderCG(path, aFillRule);
  return builder;
}



TemporaryRef<PathBuilder>
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
            CGPoint pt  = element->points[0];
            CGPoint cpt = element->points[1];
            CGPathAddQuadCurveToPoint(info->path, &info->transform, cpt.x, cpt.y, pt.x, pt.y);
            break;
          }
        case kCGPathElementAddCurveToPoint:
          {
            CGPoint pt   = element->points[0];
            CGPoint cpt1 = element->points[1];
            CGPoint cpt2 = element->points[2];
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
  RefPtr<PathBuilderCG> builder = new PathBuilderCG(ta.path, aFillRule);
  return builder;
}


bool
PathCG::ContainsPoint(const Point &aPoint, const Matrix &aTransform) const
{
  Matrix inverse = aTransform;
  inverse.Invert();
  Point transformedPoint = inverse*aPoint;
  // We could probably drop the input transform and just transform the point at the caller?
  CGPoint point = {transformedPoint.x, transformedPoint.y};

  // The transform parameter of CGPathContainsPoint doesn't seem to work properly on OS X 10.5
  // so we transform aPoint ourselves.
  return CGPathContainsPoint(mPath, NULL, point, mFillRule == FILL_EVEN_ODD);
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
  CGDataConsumerCallbacks callbacks = {PutBytesNull, NULL};
  CGDataConsumerRef consumer = CGDataConsumerCreate(NULL, &callbacks);
  CGContextRef cg = CGPDFContextCreate(consumer, NULL, NULL);
  CGDataConsumerRelease(consumer);
  return cg;
}

static CGContextRef
ScratchContext()
{
  static CGContextRef cg = CreateScratchContext();
  return cg;
}

//XXX: what should these functions return for an empty path?
// currently they return CGRectNull {inf,inf, 0, 0}
Rect
PathCG::GetBounds(const Matrix &aTransform) const
{
  //XXX: are these bounds tight enough
  Rect bounds = CGRectToRect(CGPathGetBoundingBox(mPath));
  //XXX: curretnly this returns the bounds of the transformed bounds
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

  return aTransform.TransformBounds(bounds);
}


}

}
