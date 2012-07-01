/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicLayersImpl.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

already_AddRefed<gfxASurface>
GetMaskSurfaceAndTransform(Layer* aMaskLayer, gfxMatrix* aMaskTransform)
{
   if (aMaskLayer) {
     nsRefPtr<gfxASurface> maskSurface =
       static_cast<BasicImplData*>(aMaskLayer->ImplData())->GetAsSurface();
     if (maskSurface) {
       bool maskIs2D =
         aMaskLayer->GetEffectiveTransform().CanDraw2D(aMaskTransform);
       NS_ASSERTION(maskIs2D, "How did we end up with a 3D transform here?!");
       return maskSurface.forget();
     }
   }
   return nsnull;
}

void
PaintWithMask(gfxContext* aContext, float aOpacity, Layer* aMaskLayer)
{
  gfxMatrix maskTransform;
  if (nsRefPtr<gfxASurface> maskSurface =
        GetMaskSurfaceAndTransform(aMaskLayer, &maskTransform)) {
    if (aOpacity < 1.0) {
      aContext->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
      aContext->Paint(aOpacity);
      aContext->PopGroupToSource();
    }
    aContext->SetMatrix(maskTransform);
    aContext->Mask(maskSurface);
    return;
  }

  // if there is no mask, just paint normally
  aContext->Paint(aOpacity);
}

void
FillWithMask(gfxContext* aContext, float aOpacity, Layer* aMaskLayer)
{
  gfxMatrix maskTransform;
  if (nsRefPtr<gfxASurface> maskSurface =
        GetMaskSurfaceAndTransform(aMaskLayer, &maskTransform)) {
    if (aOpacity < 1.0) {
      aContext->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
      aContext->FillWithOpacity(aOpacity);
      aContext->PopGroupToSource();
      aContext->SetMatrix(maskTransform);
      aContext->Mask(maskSurface);
    } else {
      aContext->Save();
      aContext->Clip();
      aContext->SetMatrix(maskTransform);
      aContext->Mask(maskSurface);
      aContext->NewPath();
      aContext->Restore();
    }
    return;
  }

  // if there is no mask, just fill normally
  aContext->FillWithOpacity(aOpacity);
}

BasicImplData*
ToData(Layer* aLayer)
{
  return static_cast<BasicImplData*>(aLayer->ImplData());
}

ShadowableLayer*
ToShadowable(Layer* aLayer)
{
  return aLayer->AsShadowableLayer();
}

bool
ShouldShadow(Layer* aLayer)
{
  if (!ToShadowable(aLayer)) {
    NS_ABORT_IF_FALSE(aLayer->GetType() == Layer::TYPE_READBACK,
                      "Only expect not to shadow ReadbackLayers");
    return false;
  }
  return true;
}


}
}