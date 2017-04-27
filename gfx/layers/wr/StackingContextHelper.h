/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_STACKINGCONTEXTHELPER_H
#define GFX_STACKINGCONTEXTHELPER_H

#include "mozilla/Attributes.h"
#include "mozilla/gfx/MatrixFwd.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "Units.h"

namespace mozilla {
namespace layers {

class WebRenderLayer;

/**
 * This is a helper class that pushes/pops a stacking context, and manages
 * some of the coordinate space transformations needed.
 */
class MOZ_RAII StackingContextHelper
{
public:
  // Pushes a stacking context onto the provided DisplayListBuilder. It uses
  // the transform if provided, otherwise takes the transform from the layer.
  // It also takes the mix-blend-mode and bounds from the layer, and uses 1.0
  // for the opacity.
  StackingContextHelper(wr::DisplayListBuilder& aBuilder,
                        WebRenderLayer* aLayer,
                        const Maybe<gfx::Matrix4x4>& aTransform = Nothing());
  // Pops the stacking context
  ~StackingContextHelper();

  // When this StackingContextHelper is in scope, this function can be used
  // to convert a rect from the layer system's coordinate space to a WrRect
  // that is relative to the stacking context. This is useful because most
  // things that are pushed inside the stacking context need to be relative
  // to the stacking context.
  WrRect ToRelativeWrRect(const LayerRect& aRect);

private:
  wr::DisplayListBuilder* mBuilder;
  LayerPoint mOrigin;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_STACKINGCONTEXTHELPER_H */
