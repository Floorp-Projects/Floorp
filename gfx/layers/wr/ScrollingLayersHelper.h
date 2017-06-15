/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SCROLLINGLAYERSHELPER_H
#define GFX_SCROLLINGLAYERSHELPER_H

#include "mozilla/Attributes.h"

namespace mozilla {

namespace wr {
class DisplayListBuilder;
}

namespace layers {

class LayerClip;
class StackingContextHelper;
class WebRenderLayer;

class MOZ_RAII ScrollingLayersHelper
{
public:
  ScrollingLayersHelper(WebRenderLayer* aLayer,
                        wr::DisplayListBuilder& aBuilder,
                        const StackingContextHelper& aSc);
  ~ScrollingLayersHelper();

private:
  void PushLayerLocalClip(const StackingContextHelper& aStackingContext);
  void PushLayerClip(const LayerClip& aClip,
                     const StackingContextHelper& aSc);

  WebRenderLayer* mLayer;
  wr::DisplayListBuilder* mBuilder;
  bool mPushedLayerLocalClip;
};

} // namespace layers
} // namespace mozilla

#endif
