/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICLAYERSIMPL_H
#define GFX_BASICLAYERSIMPL_H

#include "BasicImplData.h"              // for BasicImplData
#include "BasicLayers.h"                // for BasicLayerManager
#include "ReadbackLayer.h"              // for ReadbackLayer
#include "gfxASurface.h"                // for gfxASurface
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxMatrix.h"                  // for gfxMatrix
#include "ipc/AutoOpenSurface.h"        // for AutoOpenSurface
#include "mozilla/Attributes.h"         // for MOZ_DELETE, MOZ_STACK_CLASS
#include "mozilla/Util.h"               // for Maybe
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for gfxContext::Release, etc
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc

namespace mozilla {
namespace layers {

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

/**
 * Drawing with a mask requires a mask surface and a transform.
 * Sometimes the mask surface is a direct gfxASurface, but other times
 * it's a SurfaceDescriptor.  For SurfaceDescriptor, we need to use a
 * scoped AutoOpenSurface to get a gfxASurface for the
 * SurfaceDescriptor.
 *
 * This helper class manages the gfxASurface-or-SurfaceDescriptor
 * logic.
 */
class MOZ_STACK_CLASS AutoMaskData {
public:
  AutoMaskData() { }
  ~AutoMaskData() { }

  /**
   * Construct this out of either a gfxASurface or a
   * SurfaceDescriptor.  Construct() must only be called once.
   * GetSurface() and GetTransform() must not be called until this has
   * been constructed.
   */

  void Construct(const gfxMatrix& aTransform,
                 gfxASurface* aSurface);

  void Construct(const gfxMatrix& aTransform,
                 const SurfaceDescriptor& aSurface);

  /** The returned surface can't escape the scope of |this|. */
  gfxASurface* GetSurface();
  const gfxMatrix& GetTransform();

private:
  bool IsConstructed();

  gfxMatrix mTransform;
  nsRefPtr<gfxASurface> mSurface;
  Maybe<AutoOpenSurface> mSurfaceOpener;

  AutoMaskData(const AutoMaskData&) MOZ_DELETE;
  AutoMaskData& operator=(const AutoMaskData&) MOZ_DELETE;
};

/*
 * Extract a mask surface for a mask layer
 * Returns true and through outparams a surface for the mask layer if
 * a mask layer is present and has a valid surface and transform;
 * false otherwise.
 * The transform for the layer will be put in aMaskData
 */
bool
GetMaskData(Layer* aMaskLayer, AutoMaskData* aMaskData);

// Paint the current source to a context using a mask, if present
void
PaintWithMask(gfxContext* aContext, float aOpacity, Layer* aMaskLayer);

// Fill the current path with the current source, using a
// mask and opacity, if present
void
FillWithMask(gfxContext* aContext, float aOpacity, Layer* aMaskLayer);

BasicImplData*
ToData(Layer* aLayer);

}
}

#endif
