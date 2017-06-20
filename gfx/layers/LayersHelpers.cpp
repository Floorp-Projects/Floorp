/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayersHelpers.h"

namespace mozilla {
namespace layers {

using namespace gfx;

gfx::IntRect
ComputeBackdropCopyRect(const gfx::Rect& aRect,
                        const gfx::IntRect& aClipRect,
                        const gfx::Matrix4x4& aTransform,
                        const gfx::IntRect& aRenderTargetRect,
                        gfx::Matrix4x4* aOutTransform,
                        gfx::Rect* aOutLayerQuad)
{
  // Compute the clip.
  IntPoint rtOffset = aRenderTargetRect.TopLeft();
  IntSize rtSize = aRenderTargetRect.Size();

  gfx::IntRect renderBounds(0, 0, rtSize.width, rtSize.height);
  renderBounds.IntersectRect(renderBounds, aClipRect);
  renderBounds.MoveBy(rtOffset);

  // Apply the layer transform.
  RectDouble dest = aTransform.TransformAndClipBounds(
    RectDouble(aRect.x, aRect.y, aRect.width, aRect.height),
    RectDouble(renderBounds.x, renderBounds.y, renderBounds.width, renderBounds.height));
  dest -= rtOffset;

  // Ensure we don't round out to -1, which trips up Direct3D.
  dest.IntersectRect(dest, RectDouble(0, 0, rtSize.width, rtSize.height));

  if (aOutLayerQuad) {
    *aOutLayerQuad = Rect(dest.x, dest.y, dest.width, dest.height);
  }

  // Round out to integer.
  IntRect result;
  dest.RoundOut();
  dest.ToIntRect(&result);

  // Create a transform from adjusted clip space to render target space,
  // translate it for the backdrop rect, then transform it into the backdrop's
  // uv-space.
  Matrix4x4 transform;
  transform.PostScale(rtSize.width, rtSize.height, 1.0);
  transform.PostTranslate(-result.x, -result.y, 0.0);
  transform.PostScale(1 / float(result.width), 1 / float(result.height), 1.0);
  *aOutTransform = transform;
  return result;
}

} // namespace layers
} // namespace mozilla
