/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PATHRECORDING_H_
#define MOZILLA_GFX_PATHRECORDING_H_

#include "2D.h"
#include <vector>
#include <ostream>

#include "PathHelpers.h"

namespace mozilla {
namespace gfx {

struct PathOp
{
  enum OpType {
    OP_MOVETO = 0,
    OP_LINETO,
    OP_BEZIERTO,
    OP_QUADRATICBEZIERTO,
    OP_CLOSE
  };

  OpType mType;
  Point mP1;
  Point mP2;
  Point mP3;
};

const int32_t sPointCount[] = { 1, 1, 3, 2, 0, 0 };

class PathRecording;
class DrawEventRecorderPrivate;

class PathBuilderRecording : public PathBuilder
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PathBuilderRecording)
  PathBuilderRecording(PathBuilder *aBuilder, FillRule aFillRule)
    : mPathBuilder(aBuilder), mFillRule(aFillRule)
  {
  }

  /* Move the current point in the path, any figure currently being drawn will
   * be considered closed during fill operations, however when stroking the
   * closing line segment will not be drawn.
   */
  virtual void MoveTo(const Point &aPoint);
  /* Add a linesegment to the current figure */
  virtual void LineTo(const Point &aPoint);
  /* Add a cubic bezier curve to the current figure */
  virtual void BezierTo(const Point &aCP1,
                        const Point &aCP2,
                        const Point &aCP3);
  /* Add a quadratic bezier curve to the current figure */
  virtual void QuadraticBezierTo(const Point &aCP1,
                                 const Point &aCP2);
  /* Close the current figure, this will essentially generate a line segment
   * from the current point to the starting point for the current figure
   */
  virtual void Close();

  /* Add an arc to the current figure */
  virtual void Arc(const Point &aOrigin, float aRadius, float aStartAngle,
                   float aEndAngle, bool aAntiClockwise) {
    ArcToBezier(this, aOrigin, Size(aRadius, aRadius), aStartAngle, aEndAngle,
                aAntiClockwise);
  }

  /* Point the current subpath is at - or where the next subpath will start
   * if there is no active subpath.
   */
  virtual Point CurrentPoint() const;

  virtual already_AddRefed<Path> Finish();

  virtual BackendType GetBackendType() const { return BackendType::RECORDING; }

private:
  friend class PathRecording;

  RefPtr<PathBuilder> mPathBuilder;
  FillRule mFillRule;
  std::vector<PathOp> mPathOps;
};

class PathRecording : public Path
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PathRecording)
  PathRecording(Path *aPath, const std::vector<PathOp> aOps, FillRule aFillRule)
    : mPath(aPath), mPathOps(aOps), mFillRule(aFillRule)
  {
  }

  ~PathRecording();

  virtual BackendType GetBackendType() const { return BackendType::RECORDING; }
  virtual already_AddRefed<PathBuilder> CopyToBuilder(FillRule aFillRule) const;
  virtual already_AddRefed<PathBuilder> TransformedCopyToBuilder(const Matrix &aTransform,
                                                             FillRule aFillRule) const;
  virtual bool ContainsPoint(const Point &aPoint, const Matrix &aTransform) const
  { return mPath->ContainsPoint(aPoint, aTransform); }
  virtual bool StrokeContainsPoint(const StrokeOptions &aStrokeOptions,
                                   const Point &aPoint,
                                   const Matrix &aTransform) const
  { return mPath->StrokeContainsPoint(aStrokeOptions, aPoint, aTransform); }
  
  virtual Rect GetBounds(const Matrix &aTransform = Matrix()) const
  { return mPath->GetBounds(aTransform); }
  
  virtual Rect GetStrokedBounds(const StrokeOptions &aStrokeOptions,
                                const Matrix &aTransform = Matrix()) const
  { return mPath->GetStrokedBounds(aStrokeOptions, aTransform); }

  virtual void StreamToSink(PathSink *aSink) const { mPath->StreamToSink(aSink); }

  virtual FillRule GetFillRule() const { return mFillRule; }

  void StorePath(std::ostream &aStream) const;
  static void ReadPathToBuilder(std::istream &aStream, PathBuilder *aBuilder);

private:
  friend class DrawTargetWrapAndRecord;
  friend class DrawTargetRecording;
  friend class RecordedPathCreation;

  RefPtr<Path> mPath;
  std::vector<PathOp> mPathOps;
  FillRule mFillRule;

  // Event recorders that have this path in their event stream.
  std::vector<RefPtr<DrawEventRecorderPrivate>> mStoredRecorders;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_PATHRECORDING_H_ */
