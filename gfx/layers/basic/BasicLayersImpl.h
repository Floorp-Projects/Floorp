/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICLAYERSIMPL_H
#define GFX_BASICLAYERSIMPL_H

#include "BasicImplData.h"              // for BasicImplData
#include "BasicLayers.h"                // for BasicLayerManager
#include "ReadbackLayer.h"              // for ReadbackLayer
#include "gfxContext.h"                 // for gfxContext, etc
#include "mozilla/Attributes.h"         // for MOZ_DELETE, MOZ_STACK_CLASS
#include "mozilla/Maybe.h"              // for Maybe
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for gfxContext::Release, etc
#include "nsRegion.h"                   // for nsIntRegion

namespace mozilla {
namespace gfx {
class DrawTarget;
}

namespace layers {

class AutoMoz2DMaskData;
class BasicContainerLayer;
class Layer;

class AutoSetOperator {
public:
  AutoSetOperator(gfxContext* aContext, gfxContext::GraphicsOperator aOperator) {
    if (aOperator != gfxContext::OPERATOR_OVER) {
      aContext->SetOperator(aOperator);
      mContext = aContext;
    }
  }
  ~AutoSetOperator() {
    if (mContext) {
      mContext->SetOperator(gfxContext::OPERATOR_OVER);
    }
  }
private:
  nsRefPtr<gfxContext> mContext;
};

class BasicReadbackLayer : public ReadbackLayer,
                           public BasicImplData
{
public:
  BasicReadbackLayer(BasicLayerManager* aLayerManager) :
    ReadbackLayer(aLayerManager,
                  static_cast<BasicImplData*>(MOZ_THIS_IN_INITIALIZER_LIST()))
  {
    MOZ_COUNT_CTOR(BasicReadbackLayer);
  }
  virtual ~BasicReadbackLayer()
  {
    MOZ_COUNT_DTOR(BasicReadbackLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ReadbackLayer::SetVisibleRegion(aRegion);
  }

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

/*
 * Extract a mask surface for a mask layer
 * Returns true and through outparams a surface for the mask layer if
 * a mask layer is present and has a valid surface and transform;
 * false otherwise.
 * The transform for the layer will be put in aMaskData
 */
bool
GetMaskData(Layer* aMaskLayer, AutoMoz2DMaskData* aMaskData);

// Paint the current source to a context using a mask, if present
void
PaintWithMask(gfxContext* aContext, float aOpacity, Layer* aMaskLayer);

// Fill the rect with the source, using a mask and opacity, if present
void
FillRectWithMask(gfx::DrawTarget* aDT,
                 const gfx::Rect& aRect,
                 const gfx::Color& aColor,
                 const gfx::DrawOptions& aOptions,
                 gfx::SourceSurface* aMaskSource = nullptr,
                 const gfx::Matrix* aMaskTransform = nullptr);
void
FillRectWithMask(gfx::DrawTarget* aDT,
                 const gfx::Rect& aRect,
                 gfx::SourceSurface* aSurface,
                 gfx::Filter aFilter,
                 const gfx::DrawOptions& aOptions,
                 gfx::ExtendMode aExtendMode,
                 gfx::SourceSurface* aMaskSource = nullptr,
                 const gfx::Matrix* aMaskTransform = nullptr,
                 const gfx::Matrix* aSurfaceTransform = nullptr);
void
FillRectWithMask(gfx::DrawTarget* aDT,
                 const gfx::Rect& aRect,
                 gfx::SourceSurface* aSurface,
                 gfx::Filter aFilter,
                 const gfx::DrawOptions& aOptions,
                 Layer* aMaskLayer);
void
FillRectWithMask(gfx::DrawTarget* aDT,
                 const gfx::Rect& aRect,
                 const gfx::Color& aColor,
                 const gfx::DrawOptions& aOptions,
                 Layer* aMaskLayer);

BasicImplData*
ToData(Layer* aLayer);

/**
 * Returns the operator to be used when blending and compositing this layer.
 * Currently there is no way to specify both a blending and a compositing
 * operator other than normal and source over respectively.
 *
 * If the layer has
 * an effective blend mode operator other than normal, as returned by
 * GetEffectiveMixBlendMode, this operator is used for blending, and source
 * over is used for compositing.
 * If the blend mode for this layer is normal, the compositing operator
 * returned by GetOperator is used.
 */
gfx::CompositionOp
GetEffectiveOperator(Layer* aLayer);

}
}

#endif
