/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PathCapture.h"

namespace mozilla {
namespace gfx {

using namespace std;

void PathBuilderCapture::MoveTo(const Point& aPoint) {
  PathOp op;
  op.mType = PathOp::OP_MOVETO;
  op.mP1 = aPoint;
  mPathOps.push_back(op);
  mCurrentPoint = aPoint;
}

void PathBuilderCapture::LineTo(const Point& aPoint) {
  PathOp op;
  op.mType = PathOp::OP_LINETO;
  op.mP1 = aPoint;
  mPathOps.push_back(op);
  mCurrentPoint = aPoint;
}

void PathBuilderCapture::BezierTo(const Point& aCP1, const Point& aCP2,
                                  const Point& aCP3) {
  PathOp op;
  op.mType = PathOp::OP_BEZIERTO;
  op.mP1 = aCP1;
  op.mP2 = aCP2;
  op.mP3 = aCP3;
  mPathOps.push_back(op);
  mCurrentPoint = aCP3;
}

void PathBuilderCapture::QuadraticBezierTo(const Point& aCP1,
                                           const Point& aCP2) {
  PathOp op;
  op.mType = PathOp::OP_QUADRATICBEZIERTO;
  op.mP1 = aCP1;
  op.mP2 = aCP2;
  mPathOps.push_back(op);
  mCurrentPoint = aCP2;
}

void PathBuilderCapture::Arc(const Point& aOrigin, float aRadius,
                             float aStartAngle, float aEndAngle,
                             bool aAntiClockwise) {
  PathOp op;
  op.mType = PathOp::OP_ARC;
  op.mP1 = aOrigin;
  op.mRadius = aRadius;
  op.mStartAngle = aStartAngle;
  op.mEndAngle = aEndAngle;
  op.mAntiClockwise = aAntiClockwise;
  mPathOps.push_back(op);
}

void PathBuilderCapture::Close() {
  PathOp op;
  op.mType = PathOp::OP_CLOSE;
  mPathOps.push_back(op);
}

Point PathBuilderCapture::CurrentPoint() const { return mCurrentPoint; }

already_AddRefed<Path> PathBuilderCapture::Finish() {
  return MakeAndAddRef<PathCapture>(std::move(mPathOps), mFillRule, mDT,
                                    mCurrentPoint);
}

already_AddRefed<PathBuilder> PathCapture::CopyToBuilder(
    FillRule aFillRule) const {
  RefPtr<PathBuilderCapture> capture = new PathBuilderCapture(aFillRule, mDT);
  capture->mPathOps = mPathOps;
  capture->mCurrentPoint = mCurrentPoint;
  return capture.forget();
}

already_AddRefed<PathBuilder> PathCapture::TransformedCopyToBuilder(
    const Matrix& aTransform, FillRule aFillRule) const {
  RefPtr<PathBuilderCapture> capture = new PathBuilderCapture(aFillRule, mDT);
  typedef std::vector<PathOp> pathOpVec;
  for (pathOpVec::const_iterator iter = mPathOps.begin();
       iter != mPathOps.end(); iter++) {
    PathOp newPathOp;
    newPathOp.mType = iter->mType;
    if (newPathOp.mType == PathOp::OpType::OP_ARC) {
      struct ArcTransformer {
        ArcTransformer(pathOpVec& aVector, const Matrix& aTransform)
            : mVector(&aVector), mTransform(&aTransform) {}
        void BezierTo(const Point& aCP1, const Point& aCP2, const Point& aCP3) {
          PathOp newPathOp;
          newPathOp.mType = PathOp::OP_BEZIERTO;
          newPathOp.mP1 = mTransform->TransformPoint(aCP1);
          newPathOp.mP2 = mTransform->TransformPoint(aCP2);
          newPathOp.mP3 = mTransform->TransformPoint(aCP3);
          mVector->push_back(newPathOp);
        }
        void LineTo(const Point& aPoint) {
          PathOp newPathOp;
          newPathOp.mType = PathOp::OP_LINETO;
          newPathOp.mP1 = mTransform->TransformPoint(aPoint);
          mVector->push_back(newPathOp);
        }
        pathOpVec* mVector;
        const Matrix* mTransform;
      };

      ArcTransformer arcTransformer(capture->mPathOps, aTransform);
      ArcToBezier(&arcTransformer, iter->mP1,
                  Size(iter->mRadius, iter->mRadius), iter->mStartAngle,
                  iter->mEndAngle, iter->mAntiClockwise);
    } else {
      if (sPointCount[newPathOp.mType] >= 1) {
        newPathOp.mP1 = aTransform.TransformPoint(iter->mP1);
      }
      if (sPointCount[newPathOp.mType] >= 2) {
        newPathOp.mP2 = aTransform.TransformPoint(iter->mP2);
      }
      if (sPointCount[newPathOp.mType] >= 3) {
        newPathOp.mP3 = aTransform.TransformPoint(iter->mP3);
      }
      capture->mPathOps.push_back(newPathOp);
    }
  }
  capture->mCurrentPoint = aTransform.TransformPoint(mCurrentPoint);
  return capture.forget();
}
bool PathCapture::ContainsPoint(const Point& aPoint,
                                const Matrix& aTransform) const {
  if (!EnsureRealizedPath()) {
    return false;
  }
  return mRealizedPath->ContainsPoint(aPoint, aTransform);
}

bool PathCapture::StrokeContainsPoint(const StrokeOptions& aStrokeOptions,
                                      const Point& aPoint,
                                      const Matrix& aTransform) const {
  if (!EnsureRealizedPath()) {
    return false;
  }
  return mRealizedPath->StrokeContainsPoint(aStrokeOptions, aPoint, aTransform);
}

Rect PathCapture::GetBounds(const Matrix& aTransform) const {
  if (!EnsureRealizedPath()) {
    return Rect();
  }
  return mRealizedPath->GetBounds(aTransform);
}

Rect PathCapture::GetStrokedBounds(const StrokeOptions& aStrokeOptions,
                                   const Matrix& aTransform) const {
  if (!EnsureRealizedPath()) {
    return Rect();
  }
  return mRealizedPath->GetStrokedBounds(aStrokeOptions, aTransform);
}

void PathCapture::StreamToSink(PathSink* aSink) const {
  for (const PathOp& op : mPathOps) {
    switch (op.mType) {
      case PathOp::OP_MOVETO:
        aSink->MoveTo(op.mP1);
        break;
      case PathOp::OP_LINETO:
        aSink->LineTo(op.mP1);
        break;
      case PathOp::OP_BEZIERTO:
        aSink->BezierTo(op.mP1, op.mP2, op.mP3);
        break;
      case PathOp::OP_QUADRATICBEZIERTO:
        aSink->QuadraticBezierTo(op.mP1, op.mP2);
        break;
      case PathOp::OP_ARC:
        aSink->Arc(op.mP1, op.mRadius, op.mStartAngle, op.mEndAngle,
                   op.mAntiClockwise);
        break;
      case PathOp::OP_CLOSE:
        aSink->Close();
        break;
    }
  }
}

bool PathCapture::EnsureRealizedPath() const {
  RefPtr<PathBuilder> builder = mDT->CreatePathBuilder(mFillRule);
  if (!builder) {
    return false;
  }
  StreamToSink(builder);
  mRealizedPath = builder->Finish();
  return true;
}

Path* PathCapture::GetRealizedPath() const {
  if (!EnsureRealizedPath()) {
    return nullptr;
  }

  return mRealizedPath.get();
}

}  // namespace gfx
}  // namespace mozilla
