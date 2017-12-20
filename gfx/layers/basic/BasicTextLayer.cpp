/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicLayersImpl.h"            // for FillRectWithMask, etc
#include "Layers.h"                     // for ColorLayer, etc
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
#include "mozilla/layers/LayersMessages.h" // for GlyphArray

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class BasicTextLayer : public TextLayer, public BasicImplData {
public:
  explicit BasicTextLayer(BasicLayerManager* aLayerManager) :
    TextLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicTextLayer);
  }

protected:
  virtual ~BasicTextLayer()
  {
    MOZ_COUNT_DTOR(BasicTextLayer);
  }

public:
  virtual void SetVisibleRegion(const LayerIntRegion& aRegion) override
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    TextLayer::SetVisibleRegion(aRegion);
  }

  virtual void Paint(DrawTarget* aDT,
                     const gfx::Point& aDeviceOffset,
                     Layer* aMaskLayer) override
  {
    if (IsHidden() || !mFont) {
      return;
    }

    Rect snapped(mBounds.X(), mBounds.Y(), mBounds.Width(), mBounds.Height());
    MaybeSnapToDevicePixels(snapped, *aDT, true);

    // We don't currently support subpixel-AA in TextLayers since we
    // don't check if there's an opaque background color behind them.
    // We should fix this before using them in production.
    aDT->SetPermitSubpixelAA(false);

    for (GlyphArray& g : mGlyphs) {
      GlyphBuffer buffer = { g.glyphs().Elements(), (uint32_t)g.glyphs().Length() };
      aDT->FillGlyphs(mFont, buffer, ColorPattern(g.color().value()));
    }
  }

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

already_AddRefed<TextLayer>
BasicLayerManager::CreateTextLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  RefPtr<TextLayer> layer = new BasicTextLayer(this);
  return layer.forget();
}

} // namespace layers
} // namespace mozilla
