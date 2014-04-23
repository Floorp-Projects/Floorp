/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CLIENTTILEDTHEBESLAYER_H
#define GFX_CLIENTTILEDTHEBESLAYER_H

#include "ClientLayerManager.h"         // for ClientLayer, etc
#include "Layers.h"                     // for ThebesLayer, etc
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/TiledContentClient.h"
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsRegion.h"                   // for nsIntRegion

class gfxContext;

namespace mozilla {
namespace layers {

class ShadowableLayer;
class SpecificLayerAttributes;

/**
 * An implementation of ThebesLayer that ONLY supports remote
 * composition that is backed by tiles. This thebes layer implementation
 * is better suited to mobile hardware to work around slow implementation
 * of glTexImage2D (for OGL compositors), and restrait memory bandwidth.
 *
 * Tiled Thebes layers use a different protocol compared with other
 * layers. A copy of the tiled buffer is made and sent to the compositing
 * thread via the layers protocol. Tiles are uploaded by the buffers
 * asynchonously without using IPC, that means they are not safe for cross-
 * process use (bug 747811). Each tile has a TextureHost/Client pair but
 * they communicate directly rather than using the Texture protocol.
 *
 * There is no ContentClient for tiled layers. There is a ContentHost, however.
 */
class ClientTiledThebesLayer : public ThebesLayer,
                               public ClientLayer
{
  typedef ThebesLayer Base;

public:
  ClientTiledThebesLayer(ClientLayerManager* const aManager);
  ~ClientTiledThebesLayer();

  // Thebes Layer
  virtual Layer* AsLayer() { return this; }
  virtual void InvalidateRegion(const nsIntRegion& aRegion) {
    mInvalidRegion.Or(mInvalidRegion, aRegion);
    mValidRegion.Sub(mValidRegion, aRegion);
    mLowPrecisionValidRegion.Sub(mLowPrecisionValidRegion, aRegion);
  }

  // Shadow methods
  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs);
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void Disconnect()
  {
    ClientLayer::Disconnect();
  }

  virtual void RenderLayer();

  virtual void ClearCachedResources() MOZ_OVERRIDE;

private:
  ClientLayerManager* ClientManager()
  {
    return static_cast<ClientLayerManager*>(mManager);
  }

  /**
   * For the initial PaintThebes of a transaction, calculates all the data
   * needed for that paint and any repeated transactions.
   */
  void BeginPaint();

  /**
   * When a paint ends, updates any data necessary to persist until the next
   * paint. If aFinish is true, this will cause the paint to be marked as
   * finished.
   */
  void EndPaint(bool aFinish);

  RefPtr<TiledContentClient> mContentClient;
  nsIntRegion mLowPrecisionValidRegion;
  BasicTiledLayerPaintData mPaintData;
};

} // layers
} // mozilla

#endif
