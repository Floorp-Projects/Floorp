/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PATHRECORDING_H_
#define MOZILLA_GFX_PATHRECORDING_H_

#include "2D.h"
#include <vector>
#include <ostream>

#include "PathHelpers.h"
#include "RecordingTypes.h"

namespace mozilla {
namespace gfx {

struct Circle {
  Point origin;
  float radius;
  bool closed = false;
};

struct Line {
  Point origin;
  Point destination;
};

class PathOps {
 public:
  PathOps() = default;

  template <class S>
  explicit PathOps(S& aStream);

  PathOps(const PathOps& aOther) = default;
  PathOps& operator=(const PathOps&) = delete;  // assign using std::move()!

  PathOps(PathOps&& aOther) = default;
  PathOps& operator=(PathOps&& aOther) = default;

  template <class S>
  void Record(S& aStream) const;

  bool StreamToSink(PathSink& aPathSink) const;

  bool CheckedStreamToSink(PathSink& aPathSink) const;

  PathOps TransformedCopy(const Matrix& aTransform) const;

  size_t NumberOfOps() const;

  void MoveTo(const Point& aPoint) { AppendPathOp(OpType::OP_MOVETO, aPoint); }

  void LineTo(const Point& aPoint) { AppendPathOp(OpType::OP_LINETO, aPoint); }

  void BezierTo(const Point& aCP1, const Point& aCP2, const Point& aCP3) {
    AppendPathOp(OpType::OP_BEZIERTO, ThreePoints{aCP1, aCP2, aCP3});
  }

  void QuadraticBezierTo(const Point& aCP1, const Point& aCP2) {
    AppendPathOp(OpType::OP_QUADRATICBEZIERTO, TwoPoints{aCP1, aCP2});
  }

  void Arc(const Point& aOrigin, float aRadius, float aStartAngle,
           float aEndAngle, bool aAntiClockwise) {
    AppendPathOp(OpType::OP_ARC, ArcParams{aOrigin, aRadius, aStartAngle,
                                           aEndAngle, aAntiClockwise});
  }

  void Close() {
    size_t oldSize = mPathData.size();
    mPathData.resize(oldSize + sizeof(OpType));
    *reinterpret_cast<OpType*>(mPathData.data() + oldSize) = OpType::OP_CLOSE;
  }

  Maybe<Circle> AsCircle() const;
  Maybe<Line> AsLine() const;

  bool IsActive() const { return !mPathData.empty(); }

  bool IsEmpty() const;

 private:
  enum class OpType : uint32_t {
    OP_MOVETO = 0,
    OP_LINETO,
    OP_BEZIERTO,
    OP_QUADRATICBEZIERTO,
    OP_ARC,
    OP_CLOSE,
    OP_INVALID
  };

  template <typename T>
  void AppendPathOp(const OpType& aOpType, const T& aOpParams) {
    size_t oldSize = mPathData.size();
    mPathData.resize(oldSize + sizeof(OpType) + sizeof(T));
    memcpy(mPathData.data() + oldSize, &aOpType, sizeof(OpType));
    oldSize += sizeof(OpType);
    memcpy(mPathData.data() + oldSize, &aOpParams, sizeof(T));
  }

  struct TwoPoints {
    Point p1;
    Point p2;
  };

  struct ThreePoints {
    Point p1;
    Point p2;
    Point p3;
  };

  struct ArcParams {
    Point origin;
    float radius;
    float startAngle;
    float endAngle;
    bool antiClockwise;
  };

  std::vector<uint8_t> mPathData;
};

template <class S>
PathOps::PathOps(S& aStream) {
  ReadVector(aStream, mPathData);
}

template <class S>
inline void PathOps::Record(S& aStream) const {
  WriteVector(aStream, mPathData);
}

class PathRecording;
class DrawEventRecorderPrivate;

class PathBuilderRecording final : public PathBuilder {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PathBuilderRecording, override)

  PathBuilderRecording(BackendType aBackend, FillRule aFillRule)
      : mBackendType(aBackend), mFillRule(aFillRule) {}

  PathBuilderRecording(BackendType aBackend, PathOps&& aPathOps,
                       FillRule aFillRule)
      : mBackendType(aBackend),
        mFillRule(aFillRule),
        mPathOps(std::move(aPathOps)) {}

  /* Move the current point in the path, any figure currently being drawn will
   * be considered closed during fill operations, however when stroking the
   * closing line segment will not be drawn.
   */
  void MoveTo(const Point& aPoint) final;

  /* Add a linesegment to the current figure */
  void LineTo(const Point& aPoint) final;

  /* Add a cubic bezier curve to the current figure */
  void BezierTo(const Point& aCP1, const Point& aCP2, const Point& aCP3) final;

  /* Add a quadratic bezier curve to the current figure */
  void QuadraticBezierTo(const Point& aCP1, const Point& aCP2) final;

  /* Close the current figure, this will essentially generate a line segment
   * from the current point to the starting point for the current figure
   */
  void Close() final;

  /* Add an arc to the current figure */
  void Arc(const Point& aOrigin, float aRadius, float aStartAngle,
           float aEndAngle, bool aAntiClockwise) final;

  already_AddRefed<Path> Finish() final;

  BackendType GetBackendType() const final { return BackendType::RECORDING; }

  bool IsActive() const final { return mPathOps.IsActive(); }

 private:
  BackendType mBackendType;
  FillRule mFillRule;
  PathOps mPathOps;
};

class PathRecording final : public Path {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PathRecording, override)

  PathRecording(BackendType aBackend, PathOps&& aOps, FillRule aFillRule,
                const Point& aCurrentPoint, const Point& aBeginPoint);

  ~PathRecording();

  BackendType GetBackendType() const final { return BackendType::RECORDING; }
  already_AddRefed<PathBuilder> CopyToBuilder(FillRule aFillRule) const final;
  already_AddRefed<PathBuilder> TransformedCopyToBuilder(
      const Matrix& aTransform, FillRule aFillRule) const final;
  bool ContainsPoint(const Point& aPoint,
                     const Matrix& aTransform) const final {
    EnsurePath();
    return mPath->ContainsPoint(aPoint, aTransform);
  }
  bool StrokeContainsPoint(const StrokeOptions& aStrokeOptions,
                           const Point& aPoint,
                           const Matrix& aTransform) const final {
    EnsurePath();
    return mPath->StrokeContainsPoint(aStrokeOptions, aPoint, aTransform);
  }

  Rect GetBounds(const Matrix& aTransform = Matrix()) const final {
    EnsurePath();
    return mPath->GetBounds(aTransform);
  }

  Rect GetStrokedBounds(const StrokeOptions& aStrokeOptions,
                        const Matrix& aTransform = Matrix()) const final {
    EnsurePath();
    return mPath->GetStrokedBounds(aStrokeOptions, aTransform);
  }

  Maybe<Rect> AsRect() const final {
    EnsurePath();
    return mPath->AsRect();
  }

  Maybe<Circle> AsCircle() const { return mPathOps.AsCircle(); }
  Maybe<Line> AsLine() const { return mPathOps.AsLine(); }

  void StreamToSink(PathSink* aSink) const final {
    mPathOps.StreamToSink(*aSink);
  }

  FillRule GetFillRule() const final { return mFillRule; }

  bool IsEmpty() const final { return mPathOps.IsEmpty(); }

 private:
  friend class DrawTargetWrapAndRecord;
  friend class DrawTargetRecording;
  friend class RecordedPathCreation;

  void EnsurePath() const;

  BackendType mBackendType;
  mutable RefPtr<Path> mPath;
  PathOps mPathOps;
  FillRule mFillRule;
  Point mCurrentPoint;
  Point mBeginPoint;

  // Event recorders that have this path in their event stream.
  std::vector<RefPtr<DrawEventRecorderPrivate>> mStoredRecorders;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_PATHRECORDING_H_ */
