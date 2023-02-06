/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "2D.h"
#include <vector>

namespace mozilla {
namespace gfx {

struct FlatPathOp {
  enum OpType {
    OP_MOVETO,
    OP_LINETO,
  };

  OpType mType;
  Point mPoint;
};

class FlattenedPath : public PathSink {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FlattenedPath, override)

  virtual void MoveTo(const Point& aPoint) override;
  virtual void LineTo(const Point& aPoint) override;
  virtual void BezierTo(const Point& aCP1, const Point& aCP2,
                        const Point& aCP3) override;
  virtual void QuadraticBezierTo(const Point& aCP1, const Point& aCP2) override;
  virtual void Close() override;
  virtual void Arc(const Point& aOrigin, float aRadius, float aStartAngle,
                   float aEndAngle, bool aAntiClockwise = false) override;

  virtual Point CurrentPoint() const override {
    return mPathOps.empty() ? Point() : mPathOps[mPathOps.size() - 1].mPoint;
  }

  Float ComputeLength();
  Point ComputePointAtLength(Float aLength, Point* aTangent);

 private:
  Float mCachedLength = 0.0f;
  bool mCalculatedLength = false;

  std::vector<FlatPathOp> mPathOps;

  // Used to accelerate ComputePointAtLength for the common case of iterating
  // forward along the path.
  struct {
    uint32_t mIndex = 0;
    Float mLength = 0.0f;
    Point mCurrentPoint;
    Point mLastPointSinceMove;

    void Reset() {
      mIndex = 0;
      mLength = 0.0f;
      mCurrentPoint = Point();
      mLastPointSinceMove = Point();
    }
  } mCursor;
};

}  // namespace gfx
}  // namespace mozilla
