/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PathRecording.h"
#include "DrawEventRecorder.h"
#include "RecordedEventImpl.h"

namespace mozilla {
namespace gfx {

using namespace std;

void
PathBuilderRecording::MoveTo(const Point &aPoint)
{
  PathOp op;
  op.mType = PathOp::OP_MOVETO;
  op.mP1 = aPoint;
  mPathOps.push_back(op);
  mPathBuilder->MoveTo(aPoint);
}

void
PathBuilderRecording::LineTo(const Point &aPoint)
{
  PathOp op;
  op.mType = PathOp::OP_LINETO;
  op.mP1 = aPoint;
  mPathOps.push_back(op);
  mPathBuilder->LineTo(aPoint);
}

void
PathBuilderRecording::BezierTo(const Point &aCP1, const Point &aCP2, const Point &aCP3)
{
  PathOp op;
  op.mType = PathOp::OP_BEZIERTO;
  op.mP1 = aCP1;
  op.mP2 = aCP2;
  op.mP3 = aCP3;
  mPathOps.push_back(op);
  mPathBuilder->BezierTo(aCP1, aCP2, aCP3);
}

void
PathBuilderRecording::QuadraticBezierTo(const Point &aCP1, const Point &aCP2)
{
  PathOp op;
  op.mType = PathOp::OP_QUADRATICBEZIERTO;
  op.mP1 = aCP1;
  op.mP2 = aCP2;
  mPathOps.push_back(op);
  mPathBuilder->QuadraticBezierTo(aCP1, aCP2);
}

void
PathBuilderRecording::Close()
{
  PathOp op;
  op.mType = PathOp::OP_CLOSE;
  mPathOps.push_back(op);
  mPathBuilder->Close();
}

Point
PathBuilderRecording::CurrentPoint() const
{
  return mPathBuilder->CurrentPoint();
}

already_AddRefed<Path>
PathBuilderRecording::Finish()
{
  RefPtr<Path> path = mPathBuilder->Finish();
  return MakeAndAddRef<PathRecording>(path, mPathOps, mFillRule);
}

PathRecording::~PathRecording()
{
  for (size_t i = 0; i < mStoredRecorders.size(); i++) {
    mStoredRecorders[i]->RemoveStoredObject(this);
    mStoredRecorders[i]->RecordEvent(RecordedPathDestruction(this));
  }
}

already_AddRefed<PathBuilder>
PathRecording::CopyToBuilder(FillRule aFillRule) const
{
  RefPtr<PathBuilder> pathBuilder = mPath->CopyToBuilder(aFillRule);
  RefPtr<PathBuilderRecording> recording = new PathBuilderRecording(pathBuilder, aFillRule);
  recording->mPathOps = mPathOps;
  return recording.forget();
}

already_AddRefed<PathBuilder>
PathRecording::TransformedCopyToBuilder(const Matrix &aTransform, FillRule aFillRule) const
{
  RefPtr<PathBuilder> pathBuilder = mPath->TransformedCopyToBuilder(aTransform, aFillRule);
  RefPtr<PathBuilderRecording> recording = new PathBuilderRecording(pathBuilder, aFillRule);
  typedef std::vector<PathOp> pathOpVec;
  for (pathOpVec::const_iterator iter = mPathOps.begin(); iter != mPathOps.end(); iter++) {
    PathOp newPathOp;
    newPathOp.mType = iter->mType;
    if (sPointCount[newPathOp.mType] >= 1) {
      newPathOp.mP1 = aTransform.TransformPoint(iter->mP1);
    }
    if (sPointCount[newPathOp.mType] >= 2) {
      newPathOp.mP2 = aTransform.TransformPoint(iter->mP2);
    }
    if (sPointCount[newPathOp.mType] >= 3) {
      newPathOp.mP3 = aTransform.TransformPoint(iter->mP3);
    }
    recording->mPathOps.push_back(newPathOp);
  }
  return recording.forget();
}

} // namespace gfx
} // namespace mozilla
