/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LayerManagerCompositeUtils_H
#define GFX_LayerManagerCompositeUtils_H

#include <cstddef>                 // for size_t
#include "Layers.h"                // for Layer
#include "Units.h"                 // for LayerIntRegion
#include "mozilla/RefPtr.h"        // for RefPtr
#include "mozilla/gfx/BaseRect.h"  // for operator-
#include "mozilla/gfx/Matrix.h"    // for Matrix4x4
#include "mozilla/gfx/Rect.h"  // for IntRect, Rect, RoundedOut, IntRectTyped
#include "mozilla/layers/Compositor.h"  // for Compositor, INIT_MODE_CLEAR
#include "mozilla/layers/Effects.h"     // for EffectChain, EffectRenderTarget
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerManagerComposite::AutoAddMaskEffect, LayerComposite, LayerManagerComposite
#include "mozilla/layers/TextureHost.h"  // for CompositingRenderTarget

namespace mozilla {
namespace layers {

// Render aLayer using aCompositor and apply all mask layers of aLayer: The
// layer's own mask layer (aLayer->GetMaskLayer()), and any ancestor mask
// layers.
// If more than one mask layer needs to be applied, we use intermediate surfaces
// (CompositingRenderTargets) for rendering, applying one mask layer at a time.
// Callers need to provide a callback function aRenderCallback that does the
// actual rendering of the source. It needs to have the following form:
// void (EffectChain& effectChain, const Rect& clipRect)
// aRenderCallback is called exactly once, inside this function, unless aLayer's
// visible region is completely clipped out (in that case, aRenderCallback won't
// be called at all).
// This function calls aLayer->AsHostLayer()->AddBlendModeEffect for the
// final rendering pass.
//
// (This function should really live in LayerManagerComposite.cpp, but we
// need to use templates for passing lambdas until bug 1164522 is resolved.)
template <typename RenderCallbackType>
void RenderWithAllMasks(Layer* aLayer, Compositor* aCompositor,
                        const gfx::IntRect& aClipRect,
                        RenderCallbackType aRenderCallback) {
  Layer* firstMask = nullptr;
  size_t maskLayerCount = 0;
  size_t nextAncestorMaskLayer = 0;

  size_t ancestorMaskLayerCount = aLayer->GetAncestorMaskLayerCount();
  if (Layer* ownMask = aLayer->GetMaskLayer()) {
    firstMask = ownMask;
    maskLayerCount = ancestorMaskLayerCount + 1;
    nextAncestorMaskLayer = 0;
  } else if (ancestorMaskLayerCount > 0) {
    firstMask = aLayer->GetAncestorMaskLayerAt(0);
    maskLayerCount = ancestorMaskLayerCount;
    nextAncestorMaskLayer = 1;
  } else {
    // no mask layers at all
  }

  if (maskLayerCount <= 1) {
    // This is the common case. Render in one pass and return.
    EffectChain effectChain(aLayer);
    LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(firstMask,
                                                            effectChain);
    static_cast<LayerComposite*>(aLayer->AsHostLayer())
        ->AddBlendModeEffect(effectChain);
    aRenderCallback(effectChain, aClipRect);
    return;
  }

  // We have multiple mask layers.
  // We split our list of mask layers into three parts:
  //  (1) The first mask
  //  (2) The list of intermediate masks (every mask except first and last)
  //  (3) The final mask.
  // Part (2) can be empty.
  // For parts (1) and (2) we need to allocate intermediate surfaces to render
  // into. The final mask gets rendered into the original render target.

  // Calculate the size of the intermediate surfaces.
  gfx::Rect visibleRect(
      aLayer->GetLocalVisibleRegion().GetBounds().ToUnknownRect());
  gfx::Matrix4x4 transform = aLayer->GetEffectiveTransform();
  // TODO: Use RenderTargetIntRect and TransformBy here
  gfx::IntRect surfaceRect = RoundedOut(
      transform.TransformAndClipBounds(visibleRect, gfx::Rect(aClipRect)));
  if (surfaceRect.IsEmpty()) {
    return;
  }

  RefPtr<CompositingRenderTarget> originalTarget =
      aCompositor->GetCurrentRenderTarget();

  RefPtr<CompositingRenderTarget> firstTarget =
      aCompositor->CreateRenderTarget(surfaceRect, INIT_MODE_CLEAR);
  if (!firstTarget) {
    return;
  }

  // Render the source while applying the first mask.
  aCompositor->SetRenderTarget(firstTarget);
  {
    EffectChain firstEffectChain(aLayer);
    LayerManagerComposite::AutoAddMaskEffect firstMaskEffect(firstMask,
                                                             firstEffectChain);
    aRenderCallback(firstEffectChain, aClipRect - surfaceRect.TopLeft());
    // firstTarget now contains the transformed source with the first mask and
    // opacity already applied.
  }

  // Apply the intermediate masks.
  gfx::IntRect intermediateClip(surfaceRect - surfaceRect.TopLeft());
  RefPtr<CompositingRenderTarget> previousTarget = firstTarget;
  for (size_t i = nextAncestorMaskLayer; i < ancestorMaskLayerCount - 1; i++) {
    Layer* intermediateMask = aLayer->GetAncestorMaskLayerAt(i);
    RefPtr<CompositingRenderTarget> intermediateTarget =
        aCompositor->CreateRenderTarget(surfaceRect, INIT_MODE_CLEAR);
    if (!intermediateTarget) {
      break;
    }
    aCompositor->SetRenderTarget(intermediateTarget);
    EffectChain intermediateEffectChain(aLayer);
    LayerManagerComposite::AutoAddMaskEffect intermediateMaskEffect(
        intermediateMask, intermediateEffectChain);
    if (intermediateMaskEffect.Failed()) {
      continue;
    }
    intermediateEffectChain.mPrimaryEffect =
        new EffectRenderTarget(previousTarget);
    aCompositor->DrawQuad(gfx::Rect(surfaceRect), intermediateClip,
                          intermediateEffectChain, 1.0, gfx::Matrix4x4());
    previousTarget = intermediateTarget;
  }

  aCompositor->SetRenderTarget(originalTarget);

  // Apply the final mask, rendering into originalTarget.
  EffectChain finalEffectChain(aLayer);
  finalEffectChain.mPrimaryEffect = new EffectRenderTarget(previousTarget);
  Layer* finalMask = aLayer->GetAncestorMaskLayerAt(ancestorMaskLayerCount - 1);

  // The blend mode needs to be applied in this final step, because this is
  // where we're blending with the actual background (which is in
  // originalTarget).
  static_cast<LayerComposite*>(aLayer->AsHostLayer())
      ->AddBlendModeEffect(finalEffectChain);
  LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(finalMask,
                                                          finalEffectChain);
  if (!autoMaskEffect.Failed()) {
    aCompositor->DrawQuad(gfx::Rect(surfaceRect), aClipRect, finalEffectChain,
                          1.0, gfx::Matrix4x4());
  }
}

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_LayerManagerCompositeUtils_H */
