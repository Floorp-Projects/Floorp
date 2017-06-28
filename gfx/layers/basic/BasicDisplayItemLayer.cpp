/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicLayersImpl.h"            // for FillRectWithMask, etc
#include "Layers.h"                     // for Layer, etc
#include "BasicImplData.h"              // for BasicImplData
#include "BasicLayers.h"                // for BasicLayerManager
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxRect.h"                    // for gfxRect
#include "gfx2DGlue.h"
#include "mozilla/mozalloc.h"           // for operator new
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/gfx/Helpers.h"
#include "nsDisplayList.h"              // for nsDisplayItem
#include "nsCaret.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class BasicDisplayItemLayer : public DisplayItemLayer, public BasicImplData {
public:
  explicit BasicDisplayItemLayer(BasicLayerManager* aLayerManager) :
    DisplayItemLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicDisplayItemLayer);
  }

protected:
  virtual ~BasicDisplayItemLayer()
  {
    MOZ_COUNT_DTOR(BasicDisplayItemLayer);
  }

public:
  virtual void SetVisibleRegion(const LayerIntRegion& aRegion) override
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    DisplayItemLayer::SetVisibleRegion(aRegion);
  }

  virtual void Paint(DrawTarget* aDT,
                     const gfx::Point& aDeviceOffset,
                     Layer* aMaskLayer) override
  {
    if (IsHidden() || !mItem || !mBuilder) {
      return;
    }

    AutoRestoreTransform autoRestoreTransform(aDT);
    Matrix transform = aDT->GetTransform();
    RefPtr<gfxContext> context = gfxContext::CreateOrNull(aDT, aDeviceOffset);
    context->SetMatrix(ThebesMatrix(transform));

    mItem->Paint(mBuilder, context);
  }

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

already_AddRefed<DisplayItemLayer>
BasicLayerManager::CreateDisplayItemLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  RefPtr<DisplayItemLayer> layer = new BasicDisplayItemLayer(this);
  return layer.forget();
}

} // namespace layers
} // namespace mozilla
