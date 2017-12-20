/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_LayersHelpers_h
#define mozilla_gfx_layers_LayersHelpers_h

#include "mozilla/Maybe.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Polygon.h"
#include "nsRegion.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

class Layer;

// Compute compositor information for copying the backdrop for a mix-blend
// operation.
gfx::IntRect
ComputeBackdropCopyRect(const gfx::Rect& aRect,
                        const gfx::IntRect& aClipRect,
                        const gfx::Matrix4x4& aTransform,
                        const gfx::IntRect& aRenderTargetRect,
                        gfx::Matrix4x4* aOutTransform,
                        gfx::Rect* aOutLayerQuad = nullptr);

// Compute uv-coordinates for a rect inside a texture.
template <typename T>
static inline gfx::Rect
TextureRectToCoords(const T& aRect, const gfx::IntSize& aSize)
{
  return gfx::Rect(
    float(aRect.X()) / aSize.width,
    float(aRect.Y()) / aSize.height,
    float(aRect.Width()) / aSize.width,
    float(aRect.Height()) / aSize.height);
}

// This is defined in Compositor.cpp.
nsTArray<gfx::TexturedTriangle>
GenerateTexturedTriangles(const gfx::Polygon& aPolygon,
                          const gfx::Rect& aRect,
                          const gfx::Rect& aTexRect);

// This is defined in ContainerLayerComposite.cpp.
void TransformLayerGeometry(Layer* aLayer, Maybe<gfx::Polygon>& aGeometry);

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_LayersHelpers_h
