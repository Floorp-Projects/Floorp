/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TEST_POLYGONUTILS_H
#define GFX_TEST_POLYGONUTILS_H

#include "gtest/gtest.h"

#include "nsTArray.h"
#include "Point.h"
#include "Polygon.h"
#include "Triangle.h"

namespace mozilla {
namespace gfx {

bool FuzzyEquals(const Point4D& lhs, const Point4D& rhs);
bool FuzzyEquals(const Point3D& lhs, const Point3D& rhs);
bool FuzzyEquals(const Point& lhs, const Point& rhs);

bool operator==(const Triangle& lhs, const Triangle& rhs);
bool operator==(const Polygon& lhs, const Polygon& rhs);

// Compares two arrays with the equality operator.
template<typename T>
void AssertArrayEQ(const nsTArray<T>& rhs, const nsTArray<T>& lhs)
{
  ASSERT_EQ(lhs.Length(), rhs.Length());

  for (size_t i = 0; i < lhs.Length(); ++i) {
    EXPECT_TRUE(lhs[i] == rhs[i]);
  }
}

} // namespace gfx
} // namespace mozilla

#endif /* GFX_TEST_POLYGONUTILS_H */
