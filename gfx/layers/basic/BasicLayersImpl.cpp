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

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

bool
GetMaskData(Layer* aMaskLayer,
            const Point& aDeviceOffset,
            AutoMoz2DMaskData* aMaskData)
{
  if (aMaskLayer) {
    RefPtr<SourceSurface> surface =
      static_cast<BasicImplData*>(aMaskLayer->ImplData())->GetAsSourceSurface();
    if (surface) {
      Matrix transform;
      Matrix4x4 effectiveTransform = aMaskLayer->GetEffectiveTransform();
      DebugOnly<bool> maskIs2D = effectiveTransform.CanDraw2D(&transform);
      NS_ASSERTION(maskIs2D, "How did we end up with a 3D transform here?!");
      transform.Translate(-aDeviceOffset.x, -aDeviceOffset.y);
      aMaskData->Construct(transform, surface);
      return true;
    }
  }
  return false;
}

void
PaintWithMask(gfxContext* aContext, float aOpacity, Layer* aMaskLayer)
{
  AutoMoz2DMaskData mask;
  if (GetMaskData(aMaskLayer, Point(), &mask)) {
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
FillRectWithMask(DrawTarget* aDT,
                 const Rect& aRect,
                 const Color& aColor,
                 const DrawOptions& aOptions,
                 SourceSurface* aMaskSource,
                 const Matrix* aMaskTransform)
{
  if (aMaskSource && aMaskTransform) {
    aDT->PushClipRect(aRect);
    Matrix oldTransform = aDT->GetTransform();

    aDT->SetTransform(*aMaskTransform);
    aDT->MaskSurface(ColorPattern(aColor), aMaskSource, Point(), aOptions);
    aDT->SetTransform(oldTransform);
    aDT->PopClip();
    return;
  }

  aDT->FillRect(aRect, ColorPattern(aColor), aOptions);
}
void
FillRectWithMask(DrawTarget* aDT,
                 const gfx::Point& aDeviceOffset,
                 const Rect& aRect,
                 const Color& aColor,
                 const DrawOptions& aOptions,
                 Layer* aMaskLayer)
{
  AutoMoz2DMaskData mask;
  if (GetMaskData(aMaskLayer, aDeviceOffset, &mask)) {
    const Matrix& maskTransform = mask.GetTransform();
    FillRectWithMask(aDT, aRect, aColor, aOptions, mask.GetSurface(), &maskTransform);
    return;
  }

  FillRectWithMask(aDT, aRect, aColor, aOptions);
}

void
FillRectWithMask(DrawTarget* aDT,
                 const Rect& aRect,
                 SourceSurface* aSurface,
                 Filter aFilter,
                 const DrawOptions& aOptions,
                 ExtendMode aExtendMode,
                 SourceSurface* aMaskSource,
                 const Matrix* aMaskTransform,
                 const Matrix* aSurfaceTransform)
{
  if (aMaskSource && aMaskTransform) {
    aDT->PushClipRect(aRect);
    Matrix oldTransform = aDT->GetTransform();

    Matrix inverseMask = *aMaskTransform;
    inverseMask.Invert();

    Matrix transform = oldTransform * inverseMask;
    if (aSurfaceTransform) {
      transform = (*aSurfaceTransform) * transform;
    }

    SurfacePattern source(aSurface, aExtendMode, transform, aFilter);

    aDT->SetTransform(*aMaskTransform);
    aDT->MaskSurface(source, aMaskSource, Point(0, 0), aOptions);
    aDT->SetTransform(oldTransform);
    aDT->PopClip();
    return;
  }

  aDT->FillRect(aRect,
                SurfacePattern(aSurface, aExtendMode,
                               aSurfaceTransform ? (*aSurfaceTransform) : Matrix(),
                               aFilter), aOptions);
}

void
FillRectWithMask(DrawTarget* aDT,
                 const gfx::Point& aDeviceOffset,
                 const Rect& aRect,
                 SourceSurface* aSurface,
                 Filter aFilter,
                 const DrawOptions& aOptions,
                 Layer* aMaskLayer)
{
  AutoMoz2DMaskData mask;
  if (GetMaskData(aMaskLayer, aDeviceOffset, &mask)) {
    const Matrix& maskTransform = mask.GetTransform();
    FillRectWithMask(aDT, aRect, aSurface, aFilter, aOptions, ExtendMode::CLAMP,
                     mask.GetSurface(), &maskTransform);
    return;
  }

  FillRectWithMask(aDT, aRect, aSurface, aFilter, aOptions, ExtendMode::CLAMP);
}

BasicImplData*
ToData(Layer* aLayer)
{
  return static_cast<BasicImplData*>(aLayer->ImplData());
}

gfx::CompositionOp
GetEffectiveOperator(Layer* aLayer)
{
  CompositionOp op = aLayer->GetEffectiveMixBlendMode();

  if (op != CompositionOp::OP_OVER) {
    return op;
  }

  return ToData(aLayer)->GetOperator();
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
