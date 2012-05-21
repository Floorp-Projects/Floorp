/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Layers.h"
#include "RenderTrace.h"

// If rendertrace is off let's no compile this code
#ifdef MOZ_RENDERTRACE


namespace mozilla {
namespace layers {

static int colorId = 0;

static gfx3DMatrix GetRootTransform(Layer *aLayer) {
  gfx3DMatrix layerTrans = aLayer->GetTransform().ProjectTo2D();
  if (aLayer->GetParent() != NULL) {
    return GetRootTransform(aLayer->GetParent()) * layerTrans;
  }
  return layerTrans;
}

void RenderTraceLayers(Layer *aLayer, const char *aColor, const gfx3DMatrix aRootTransform, bool aReset) {
  if (!aLayer)
    return;

  gfx3DMatrix trans = aRootTransform * aLayer->GetTransform().ProjectTo2D();
  nsIntRect clipRect = aLayer->GetEffectiveVisibleRegion().GetBounds();
  gfxRect rect(clipRect.x, clipRect.y, clipRect.width, clipRect.height);
  trans.TransformBounds(rect);

  if (strcmp(aLayer->Name(), "ContainerLayer") != 0 &&
      strcmp(aLayer->Name(), "ShadowContainerLayer") != 0) {
    printf_stderr("%s RENDERTRACE %u rect #%02X%s %i %i %i %i\n",
      aLayer->Name(), (int)PR_IntervalNow(),
      colorId, aColor,
      (int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);
  }

  colorId++;

  for (Layer* child = aLayer->GetFirstChild();
        child; child = child->GetNextSibling()) {
    RenderTraceLayers(child, aColor, aRootTransform, false);
  }

  if (aReset) colorId = 0;
}

void RenderTraceInvalidateStart(Layer *aLayer, const char *aColor, const nsIntRect aRect) {
  gfx3DMatrix trans = GetRootTransform(aLayer);
  gfxRect rect(aRect.x, aRect.y, aRect.width, aRect.height);
  trans.TransformBounds(rect);

  printf_stderr("%s RENDERTRACE %u fillrect #%s %i %i %i %i\n",
    aLayer->Name(), (int)PR_IntervalNow(),
    aColor,
    (int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);
}
void RenderTraceInvalidateEnd(Layer *aLayer, const char *aColor) {
  // Clear with an empty rect
  RenderTraceInvalidateStart(aLayer, aColor, nsIntRect());
}

void renderTraceEventStart(const char *aComment, const char *aColor) {
  printf_stderr("%s RENDERTRACE %u fillrect #%s 0 0 10 10\n",
    aComment, (int)PR_IntervalNow(), aColor);
}

void renderTraceEventEnd(const char *aComment, const char *aColor) {
  printf_stderr("%s RENDERTRACE %u fillrect #%s 0 0 0 0\n",
    aComment, (int)PR_IntervalNow(), aColor);
}

void renderTraceEventEnd(const char *aColor) {
  renderTraceEventEnd("", aColor);
}

}
}

#endif

