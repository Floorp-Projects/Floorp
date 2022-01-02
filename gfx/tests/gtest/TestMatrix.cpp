/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/gfx/Matrix.h"

using namespace mozilla;
using namespace mozilla::gfx;

TEST(Matrix, TransformAndClipRect)
{
  Rect c(100, 100, 100, 100);
  Matrix4x4 m;
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(50, 50, 20, 20), c).IsEmpty());
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(250, 50, 20, 20), c).IsEmpty());
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(250, 250, 20, 20), c).IsEmpty());
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(50, 250, 20, 20), c).IsEmpty());

  EXPECT_TRUE(m.TransformAndClipBounds(Rect(50, 50, 100, 20), c).IsEmpty());
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(150, 50, 100, 20), c).IsEmpty());
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(50, 250, 100, 20), c).IsEmpty());
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(150, 250, 100, 20), c).IsEmpty());

  EXPECT_TRUE(m.TransformAndClipBounds(Rect(50, 50, 20, 100), c).IsEmpty());
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(50, 150, 20, 100), c).IsEmpty());
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(250, 50, 20, 100), c).IsEmpty());
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(250, 150, 20, 100), c).IsEmpty());

  EXPECT_TRUE(m.TransformAndClipBounds(Rect(50, 50, 100, 100), c)
                  .IsEqualInterior(Rect(100, 100, 50, 50)));
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(150, 50, 100, 100), c)
                  .IsEqualInterior(Rect(150, 100, 50, 50)));
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(150, 150, 100, 100), c)
                  .IsEqualInterior(Rect(150, 150, 50, 50)));
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(50, 150, 100, 100), c)
                  .IsEqualInterior(Rect(100, 150, 50, 50)));

  EXPECT_TRUE(m.TransformAndClipBounds(Rect(110, 110, 80, 80), c)
                  .IsEqualInterior(Rect(110, 110, 80, 80)));

  EXPECT_TRUE(m.TransformAndClipBounds(Rect(50, 50, 200, 200), c)
                  .IsEqualInterior(Rect(100, 100, 100, 100)));

  EXPECT_TRUE(m.TransformAndClipBounds(Rect(50, 50, 200, 100), c)
                  .IsEqualInterior(Rect(100, 100, 100, 50)));
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(50, 150, 200, 100), c)
                  .IsEqualInterior(Rect(100, 150, 100, 50)));
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(50, 50, 100, 200), c)
                  .IsEqualInterior(Rect(100, 100, 50, 100)));
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(150, 50, 100, 200), c)
                  .IsEqualInterior(Rect(150, 100, 50, 100)));

  Matrix4x4 m2 = Matrix4x4::From2D(Matrix(22.68, 0, 0, 12, 16, 164));
  EXPECT_TRUE(
      m2.TransformAndClipBounds(Rect(0, 0, 100, 100), Rect(1024, 1024, 0, 0))
          .IsEmpty());

  // Empty rectangles should still have meaningful corners.
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(150, 50, 0, 200), c)
                  .IsEqualEdges(Rect(150, 100, 0, 100)));
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(150, 150, 0, 0), c)
                  .IsEqualEdges(Rect(150, 150, 0, 0)));
  EXPECT_TRUE(m.TransformAndClipBounds(Rect(150, 100, 300, 0), c)
                  .IsEqualEdges(Rect(150, 100, 50, 0)));
}

TEST(Matrix, RotateTransform)
{
  gfx::Matrix4x4 transformMatrix;
  gfx::Point3D trans, scale;
  gfx::Quaternion orient;

  auto floor = [&](float aValue, int aDecimal) {
    const int digit = pow(10, aDecimal);
    const float result = (int)(aValue * digit);
    return result / digit;
  };

  // Test rotate 45 degree on x-axis.
  gfx::Quaternion expectedOrient(0.382f, 0.0f, 0.0f, 0.923f);
  transformMatrix.RotateX(0.785f);
  // the orient would be (x:0.3825, y:0, z:0, w: 0.9239)
  transformMatrix.Decompose(trans, orient, scale);
  EXPECT_EQ(floor(orient.x, 3), expectedOrient.x);
  EXPECT_EQ(floor(orient.y, 3), expectedOrient.y);
  EXPECT_EQ(floor(orient.z, 3), expectedOrient.z);
  EXPECT_EQ(floor(orient.w, 3), expectedOrient.w);

  // Test set rotate matrix from a quaternion and
  // compare it with the result from decompose.
  transformMatrix = gfx::Matrix4x4();
  transformMatrix.SetRotationFromQuaternion(orient);
  transformMatrix.Decompose(trans, orient, scale);
  EXPECT_EQ(floor(orient.x, 3), expectedOrient.x);
  EXPECT_EQ(floor(orient.y, 3), expectedOrient.y);
  EXPECT_EQ(floor(orient.z, 3), expectedOrient.z);
  EXPECT_EQ(floor(orient.w, 3), expectedOrient.w);

  // Test rotate -45 degree on axis: (0.577f, 0.577f, 0.577f).
  transformMatrix = gfx::Matrix4x4();
  transformMatrix.SetRotateAxisAngle(0.577f, 0.577f, 0.577f, -0.785f);
  // the orient would be (x:-0.2208, y:-0.2208, z:-0.2208, w: 0.9239)
  transformMatrix.Decompose(trans, orient, scale);

  expectedOrient.Set(-0.220f, -0.220f, -0.220f, 0.923f);
  EXPECT_EQ(floor(orient.x, 3), expectedOrient.x);
  EXPECT_EQ(floor(orient.y, 3), expectedOrient.y);
  EXPECT_EQ(floor(orient.z, 3), expectedOrient.z);
  EXPECT_EQ(floor(orient.w, 3), expectedOrient.w);

  // Test set rotate matrix from a quaternion and
  // compare it with the result from decompose.
  transformMatrix = gfx::Matrix4x4();
  transformMatrix.SetRotationFromQuaternion(orient);
  transformMatrix.Decompose(trans, orient, scale);
  EXPECT_EQ(floor(orient.x, 3), expectedOrient.x);
  EXPECT_EQ(floor(orient.y, 3), expectedOrient.y);
  EXPECT_EQ(floor(orient.z, 3), expectedOrient.z);
  EXPECT_EQ(floor(orient.w, 3), expectedOrient.w);
}

TEST(Matrix4x4Flagged, Mult)
{
  Matrix4x4Flagged simple =
      Matrix4x4::Translation(Point(42, 42)) * Matrix4x4::Scaling(3, 3, 1);
  // For the general matrix, put a value in every field to make sure
  // nothing gets dropped.
  Matrix4x4 general(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2);

  // Use Matrix4x4::operator*(Matrix4x4).
  // For the purposes of this test, assume that's correct.
  Matrix4x4Flagged realResult = Matrix4x4Flagged(simple.GetMatrix() * general);

  // Check that Matrix4x4Flagged::operator*(Matrix4x4Flagged) produces the same
  // result.
  Matrix4x4Flagged flaggedResult = simple * Matrix4x4Flagged(general);
  EXPECT_EQ(realResult, flaggedResult);

  // Check that Matrix4x4Flagged::operator*(Matrix4x4) produces the same result.
  Matrix4x4Flagged mixedResult = simple * general;
  EXPECT_EQ(realResult, mixedResult);
}
