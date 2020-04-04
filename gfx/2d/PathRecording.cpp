/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PathRecording.h"
#include "DrawEventRecorder.h"
#include "RecordedEventImpl.h"

namespace mozilla {
namespace gfx {

#define NEXT_PARAMS(_type)                                        \
  const _type params = *reinterpret_cast<const _type*>(nextByte); \
  nextByte += sizeof(_type);

bool PathOps::StreamToSink(PathSink& aPathSink) const {
  if (mPathData.empty()) {
    return true;
  }

  const uint8_t* nextByte = mPathData.data();
  const uint8_t* end = nextByte + mPathData.size();
  while (nextByte < end) {
    const OpType opType = *reinterpret_cast<const OpType*>(nextByte);
    nextByte += sizeof(OpType);
    switch (opType) {
      case OpType::OP_MOVETO: {
        NEXT_PARAMS(Point)
        aPathSink.MoveTo(params);
        break;
      }
      case OpType::OP_LINETO: {
        NEXT_PARAMS(Point)
        aPathSink.LineTo(params);
        break;
      }
      case OpType::OP_BEZIERTO: {
        NEXT_PARAMS(ThreePoints)
        aPathSink.BezierTo(params.p1, params.p2, params.p3);
        break;
      }
      case OpType::OP_QUADRATICBEZIERTO: {
        NEXT_PARAMS(TwoPoints)
        aPathSink.QuadraticBezierTo(params.p1, params.p2);
        break;
      }
      case OpType::OP_ARC: {
        NEXT_PARAMS(ArcParams)
        aPathSink.Arc(params.origin, params.radius, params.startAngle,
                      params.endAngle, params.antiClockwise);
        break;
      }
      case OpType::OP_CLOSE:
        aPathSink.Close();
        break;
      default:
        return false;
    }
  }

  return true;
}

PathOps PathOps::TransformedCopy(const Matrix& aTransform) const {
  PathOps newPathOps;
  const uint8_t* nextByte = mPathData.data();
  const uint8_t* end = nextByte + mPathData.size();
  while (nextByte < end) {
    const OpType opType = *reinterpret_cast<const OpType*>(nextByte);
    nextByte += sizeof(OpType);
    switch (opType) {
      case OpType::OP_MOVETO: {
        NEXT_PARAMS(Point)
        newPathOps.MoveTo(aTransform.TransformPoint(params));
        break;
      }
      case OpType::OP_LINETO: {
        NEXT_PARAMS(Point)
        newPathOps.LineTo(aTransform.TransformPoint(params));
        break;
      }
      case OpType::OP_BEZIERTO: {
        NEXT_PARAMS(ThreePoints)
        newPathOps.BezierTo(aTransform.TransformPoint(params.p1),
                            aTransform.TransformPoint(params.p2),
                            aTransform.TransformPoint(params.p3));
        break;
      }
      case OpType::OP_QUADRATICBEZIERTO: {
        NEXT_PARAMS(TwoPoints)
        newPathOps.QuadraticBezierTo(aTransform.TransformPoint(params.p1),
                                     aTransform.TransformPoint(params.p2));
        break;
      }
      case OpType::OP_ARC: {
        NEXT_PARAMS(ArcParams)
        ArcToBezier(&newPathOps, params.origin,
                    gfx::Size(params.radius, params.radius), params.startAngle,
                    params.endAngle, params.antiClockwise, 0.0f, aTransform);
        break;
      }
      case OpType::OP_CLOSE:
        newPathOps.Close();
        break;
      default:
        MOZ_CRASH("We control mOpTypes, so this should never happen.");
    }
  }

  return newPathOps;
}

#undef NEXT_PARAMS

size_t PathOps::NumberOfOps() const {
  size_t size = 0;
  const uint8_t* nextByte = mPathData.data();
  const uint8_t* end = nextByte + mPathData.size();
  while (nextByte < end) {
    size++;
    const OpType opType = *reinterpret_cast<const OpType*>(nextByte);
    nextByte += sizeof(OpType);
    switch (opType) {
      case OpType::OP_MOVETO:
        nextByte += sizeof(Point);
        break;
      case OpType::OP_LINETO:
        nextByte += sizeof(Point);
        break;
      case OpType::OP_BEZIERTO:
        nextByte += sizeof(ThreePoints);
        break;
      case OpType::OP_QUADRATICBEZIERTO:
        nextByte += sizeof(TwoPoints);
        break;
      case OpType::OP_ARC:
        nextByte += sizeof(ArcParams);
        break;
      case OpType::OP_CLOSE:
        break;
      default:
        MOZ_CRASH("We control mOpTypes, so this should never happen.");
    }
  }

  return size;
}

void PathBuilderRecording::MoveTo(const Point& aPoint) {
  mPathOps.MoveTo(aPoint);
  mPathBuilder->MoveTo(aPoint);
}

void PathBuilderRecording::LineTo(const Point& aPoint) {
  mPathOps.LineTo(aPoint);
  mPathBuilder->LineTo(aPoint);
}

void PathBuilderRecording::BezierTo(const Point& aCP1, const Point& aCP2,
                                    const Point& aCP3) {
  mPathOps.BezierTo(aCP1, aCP2, aCP3);
  mPathBuilder->BezierTo(aCP1, aCP2, aCP3);
}

void PathBuilderRecording::QuadraticBezierTo(const Point& aCP1,
                                             const Point& aCP2) {
  mPathOps.QuadraticBezierTo(aCP1, aCP2);
  mPathBuilder->QuadraticBezierTo(aCP1, aCP2);
}

void PathBuilderRecording::Close() {
  mPathOps.Close();
  mPathBuilder->Close();
}

void PathBuilderRecording::Arc(const Point& aOrigin, float aRadius,
                               float aStartAngle, float aEndAngle,
                               bool aAntiClockwise) {
  mPathOps.Arc(aOrigin, aRadius, aStartAngle, aEndAngle, aAntiClockwise);
  mPathBuilder->Arc(aOrigin, aRadius, aStartAngle, aEndAngle, aAntiClockwise);
}

already_AddRefed<Path> PathBuilderRecording::Finish() {
  // We rely on mPathBuilder to track begin and current point, but that stops
  // when we call Finish, so we need to store them first.
  Point beginPoint = BeginPoint();
  Point currentPoint = CurrentPoint();
  RefPtr<Path> path = mPathBuilder->Finish();
  return MakeAndAddRef<PathRecording>(path, std::move(mPathOps), mFillRule,
                                      currentPoint, beginPoint);
}

PathRecording::PathRecording(Path* aPath, PathOps&& aOps, FillRule aFillRule,
                             const Point& aCurrentPoint,
                             const Point& aBeginPoint)
    : mPath(aPath),
      mPathOps(std::move(aOps)),
      mFillRule(aFillRule),
      mCurrentPoint(aCurrentPoint),
      mBeginPoint(aBeginPoint) {}

PathRecording::~PathRecording() {
  for (size_t i = 0; i < mStoredRecorders.size(); i++) {
    mStoredRecorders[i]->RemoveStoredObject(this);
    mStoredRecorders[i]->RecordEvent(RecordedPathDestruction(this));
  }
}

already_AddRefed<PathBuilder> PathRecording::CopyToBuilder(
    FillRule aFillRule) const {
  RefPtr<PathBuilder> pathBuilder = mPath->CopyToBuilder(aFillRule);
  RefPtr<PathBuilderRecording> recording =
      new PathBuilderRecording(pathBuilder, mPathOps, aFillRule);
  recording->SetCurrentPoint(mCurrentPoint);
  recording->SetBeginPoint(mBeginPoint);
  return recording.forget();
}

already_AddRefed<PathBuilder> PathRecording::TransformedCopyToBuilder(
    const Matrix& aTransform, FillRule aFillRule) const {
  RefPtr<PathBuilder> pathBuilder =
      mPath->TransformedCopyToBuilder(aTransform, aFillRule);
  RefPtr<PathBuilderRecording> recording = new PathBuilderRecording(
      pathBuilder, mPathOps.TransformedCopy(aTransform), aFillRule);

  recording->SetCurrentPoint(aTransform.TransformPoint(mCurrentPoint));
  recording->SetBeginPoint(aTransform.TransformPoint(mBeginPoint));

  return recording.forget();
}

}  // namespace gfx
}  // namespace mozilla
