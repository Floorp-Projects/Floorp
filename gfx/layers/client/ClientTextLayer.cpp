/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "Layers.h"                     // for ColorLayer, etc
#include "mozilla/layers/LayersMessages.h"  // for ColorLayerAttributes, etc
#include "mozilla/mozalloc.h"           // for operator new
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRegion.h"                   // for nsIntRegion

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

class ClientTextLayer : public TextLayer,
                        public ClientLayer {
public:
  explicit ClientTextLayer(ClientLayerManager* aLayerManager) :
    TextLayer(aLayerManager, static_cast<ClientLayer*>(this)),
    mSwapped(false)
  {
    MOZ_COUNT_CTOR(ClientTextLayer);
  }

protected:
  virtual ~ClientTextLayer()
  {
    MOZ_COUNT_DTOR(ClientTextLayer);
  }

public:
  virtual void SetVisibleRegion(const LayerIntRegion& aRegion)
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    TextLayer::SetVisibleRegion(aRegion);
  }

  virtual void RenderLayer()
  {
    RenderMaskLayers(this);
  }

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
  {
    NS_ASSERTION(!mSwapped, "Trying to access glyph array after it's been swapped!");
    aAttrs = TextLayerAttributes(GetBounds(), nsTArray<GlyphArray>(), uintptr_t(mFont.get()));
    aAttrs.get_TextLayerAttributes().glyphs().SwapElements(mGlyphs);
    mSwapped = true;
  }

  virtual void SetGlyphs(nsTArray<GlyphArray>&& aGlyphs)
  {
    TextLayer::SetGlyphs(Move(aGlyphs));
    mSwapped = false;
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

protected:
  ClientLayerManager* ClientManager()
  {
    return static_cast<ClientLayerManager*>(mManager);
  }

  bool mSwapped;
};

already_AddRefed<TextLayer>
ClientLayerManager::CreateTextLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  RefPtr<ClientTextLayer> layer =
    new ClientTextLayer(this);
  CREATE_SHADOW(Text);
  return layer.forget();
}

} // namespace layers
} // namespace mozilla
