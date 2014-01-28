/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicLayersImpl.h"
#include <new>                          // for operator new
#include "Layers.h"                     // for Layer, etc
#include "basic/BasicImplData.h"        // for BasicImplData
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/DebugOnly.h"          // for DebugOnly
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "AutoMaskData.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

void
AutoMaskData::Construct(const gfx::Matrix& aTransform,
                        gfxASurface* aSurface)
{
  MOZ_ASSERT(!IsConstructed());
  mTransform = aTransform;
  mSurface = aSurface;
}

void
AutoMaskData::Construct(const gfx::Matrix& aTransform,
                        const SurfaceDescriptor& aSurface)
{
  MOZ_ASSERT(!IsConstructed());
  mTransform = aTransform;
  mSurfaceOpener.construct(OPEN_READ_ONLY, aSurface);
}

gfxASurface*
AutoMaskData::GetSurface()
{
  MOZ_ASSERT(IsConstructed());
  if (mSurface) {
    return mSurface.get();
  }
  return mSurfaceOpener.ref().Get();
}

const gfx::Matrix&
AutoMaskData::GetTransform()
{
  MOZ_ASSERT(IsConstructed());
  return mTransform;
}

bool
AutoMaskData::IsConstructed()
{
  return !!mSurface || !mSurfaceOpener.empty();
}


bool
GetMaskData(Layer* aMaskLayer, AutoMaskData* aMaskData)
{
  if (aMaskLayer) {
    nsRefPtr<gfxASurface> surface;
    SurfaceDescriptor descriptor;
    if (static_cast<BasicImplData*>(aMaskLayer->ImplData())
        ->GetAsSurface(getter_AddRefs(surface), &descriptor) &&
        (surface || IsSurfaceDescriptorValid(descriptor))) {
      Matrix transform;
      Matrix4x4 effectiveTransform = aMaskLayer->GetEffectiveTransform();
      DebugOnly<bool> maskIs2D = effectiveTransform.CanDraw2D(&transform);
      NS_ASSERTION(maskIs2D, "How did we end up with a 3D transform here?!");
      if (surface) {
        aMaskData->Construct(transform, surface);
      } else {
        aMaskData->Construct(transform, descriptor);
      }
      return true;
    }
  }
  return false;
}

void
PaintWithMask(gfxContext* aContext, float aOpacity, Layer* aMaskLayer)
{
  AutoMaskData mask;
  if (GetMaskData(aMaskLayer, &mask)) {
    if (aOpacity < 1.0) {
      aContext->PushGroup(gfxContentType::COLOR_ALPHA);
      aContext->Paint(aOpacity);
      aContext->PopGroupToSource();
    }
    aContext->SetMatrix(ThebesMatrix(mask.GetTransform()));
    aContext->Mask(mask.GetSurface());
    return;
  }

  // if there is no mask, just paint normally
  aContext->Paint(aOpacity);
}

void
FillWithMask(gfxContext* aContext, float aOpacity, Layer* aMaskLayer)
{
  AutoMaskData mask;
  if (GetMaskData(aMaskLayer, &mask)) {
    if (aOpacity < 1.0) {
      aContext->PushGroup(gfxContentType::COLOR_ALPHA);
      aContext->FillWithOpacity(aOpacity);
      aContext->PopGroupToSource();
      aContext->SetMatrix(ThebesMatrix(mask.GetTransform()));
      aContext->Mask(mask.GetSurface());
    } else {
      aContext->Save();
      aContext->Clip();
      aContext->SetMatrix(ThebesMatrix(mask.GetTransform()));
      aContext->Mask(mask.GetSurface());
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
