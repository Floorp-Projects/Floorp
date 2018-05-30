/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TRIANGLE_H
#define MOZILLA_GFX_TRIANGLE_H

#include <algorithm>

#include "mozilla/Move.h"
#include "Point.h"
#include "Rect.h"

namespace mozilla {
namespace gfx {

/**
 * A simple triangle data structure.
 */
template<class Units, class F = Float>
struct TriangleTyped
{
  PointTyped<Units, F> p1, p2, p3;

  TriangleTyped()
    : p1(), p2(), p3() {}

  TriangleTyped(PointTyped<Units, F> aP1,
                PointTyped<Units, F> aP2,
                PointTyped<Units, F> aP3)
    : p1(aP1), p2(aP2), p3(aP3) {}

  RectTyped<Units, F> BoundingBox() const
  {
    F minX = std::min(std::min(p1.x, p2.x), p3.x);
    F maxX = std::max(std::max(p1.x, p2.x), p3.x);

    F minY = std::min(std::min(p1.y, p2.y), p3.y);
    F maxY = std::max(std::max(p1.y, p2.y), p3.y);

    return RectTyped<Units, F>(minX, minY, maxX - minX, maxY - minY);
  }
};

typedef TriangleTyped<UnknownUnits, Float> Triangle;

template<class Units, class F = Float>
struct TexturedTriangleTyped : public TriangleTyped<Units, F>
{
  explicit TexturedTriangleTyped(const TriangleTyped<Units, F>& aTriangle)
    : TriangleTyped<Units, F>(aTriangle) {}

  explicit TexturedTriangleTyped(TriangleTyped<Units, F>&& aTriangle)
    : TriangleTyped<Units, F>(std::move(aTriangle)) {}

  TriangleTyped<Units, F> textureCoords;
};

typedef TexturedTriangleTyped<UnknownUnits, Float> TexturedTriangle;

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_TRIANGLE_H */
