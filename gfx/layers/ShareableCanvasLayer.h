/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SHAREABLECANVASLAYER_H
#define GFX_SHAREABLECANVASLAYER_H

#include "CompositorTypes.h"
#include "CopyableCanvasLayer.h"
#include "mozilla/layers/CanvasClient.h"

namespace mozilla {
namespace gl {
class SurfaceFactory;
} // namespace gl

namespace layers {

class ShareableCanvasLayer : public CopyableCanvasLayer
{
  typedef CanvasClient::CanvasClientType CanvasClientType;
public:
  ShareableCanvasLayer(LayerManager* aLayerManager, void *aImplData);

protected:
  virtual ~ShareableCanvasLayer();

public:
  virtual void Initialize(const Data& aData) override;

  virtual CompositableForwarder* GetForwarder() = 0;

  virtual void AttachCompositable() = 0;

  void UpdateCompositableClient();

  const TextureFlags& Flags() const { return mFlags; }

protected:

  bool UpdateTarget(gfx::DrawTarget* aDestTarget = nullptr);

  CanvasClientType GetCanvasClientType();

  RefPtr<CanvasClient> mCanvasClient;

  UniquePtr<gl::SurfaceFactory> mFactory;

  TextureFlags mFlags;

  friend class CanvasClient2D;
  friend class CanvasClientSharedSurface;
};

} // namespace layers
} // namespace mozilla

#endif
