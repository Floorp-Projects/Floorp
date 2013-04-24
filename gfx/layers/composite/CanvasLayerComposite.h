/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CanvasLayerComposite_H
#define GFX_CanvasLayerComposite_H


#include "mozilla/layers/LayerManagerComposite.h"
#include "gfxASurface.h"
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
#include "mozilla/X11Util.h"
#endif

namespace mozilla {
namespace layers {

// Canvas layers use ImageHosts (but CanvasClients) because compositing a
// canvas is identical to compositing an image.
class ImageHost;

class CanvasLayerComposite : public CanvasLayer,
                             public LayerComposite
{
public:
  CanvasLayerComposite(LayerManagerComposite* aManager);

  virtual ~CanvasLayerComposite();

  // CanvasLayer impl
  virtual void Initialize(const Data& aData) MOZ_OVERRIDE
  {
    NS_RUNTIMEABORT("Incompatibe surface type");
  }

  virtual LayerRenderState GetRenderState() MOZ_OVERRIDE;

  virtual void SetCompositableHost(CompositableHost* aHost) MOZ_OVERRIDE;

  virtual void Disconnect() MOZ_OVERRIDE
  {
    Destroy();
  }

  virtual Layer* GetLayer() MOZ_OVERRIDE;
  virtual void RenderLayer(const nsIntPoint& aOffset,
                           const nsIntRect& aClipRect) MOZ_OVERRIDE;

  virtual void CleanupResources() MOZ_OVERRIDE;

  CompositableHost* GetCompositableHost() MOZ_OVERRIDE;

  virtual LayerComposite* AsLayerComposite() MOZ_OVERRIDE { return this; }

  void SetBounds(nsIntRect aBounds) { mBounds = aBounds; }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() const MOZ_OVERRIDE { return "CanvasLayerComposite"; }

protected:
  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix) MOZ_OVERRIDE;
#endif

private:
  RefPtr<ImageHost> mImageHost;
};

} /* layers */
} /* mozilla */
#endif /* GFX_CanvasLayerComposite_H */
