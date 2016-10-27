/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_COMPOSITABLEFORWARDER
#define MOZILLA_LAYERS_COMPOSITABLEFORWARDER

#include <stdint.h>                     // for int32_t, uint64_t
#include "gfxTypes.h"
#include "mozilla/Attributes.h"         // for override
#include "mozilla/UniquePtr.h"
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "mozilla/layers/TextureForwarder.h"  // for TextureForwarder
#include "nsRegion.h"                   // for nsIntRegion
#include "mozilla/gfx/Rect.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"

namespace mozilla {
namespace layers {

class CompositableClient;
class AsyncTransactionTracker;
class ImageContainer;
class SurfaceDescriptor;
class SurfaceDescriptorTiles;
class ThebesBufferData;
class PTextureChild;

/**
 * A transaction is a set of changes that happenned on the content side, that
 * should be sent to the compositor side.
 * CompositableForwarder is an interface to manage a transaction of
 * compositable objetcs.
 *
 * ShadowLayerForwarder is an example of a CompositableForwarder (that can
 * additionally forward modifications of the Layer tree).
 * ImageBridgeChild is another CompositableForwarder.
 *
 * CompositableForwarder implements KnowsCompositor for simplicity as all
 * implementations of CompositableForwarder currently also implement KnowsCompositor.
 * This dependency could be split if we add new use cases.
 */
class CompositableForwarder : public KnowsCompositor
{
public:
  /**
   * Setup the IPDL actor for aCompositable to be part of layers
   * transactions.
   */
  virtual void Connect(CompositableClient* aCompositable,
                       ImageContainer* aImageContainer = nullptr) = 0;

  /**
   * Tell the CompositableHost on the compositor side what TiledLayerBuffer to
   * use for the next composition.
   */
  virtual void UseTiledLayerBuffer(CompositableClient* aCompositable,
                                   const SurfaceDescriptorTiles& aTiledDescriptor) = 0;

  /**
   * Communicate to the compositor that aRegion in the texture identified by
   * aCompositable and aIdentifier has been updated to aThebesBuffer.
   */
  virtual void UpdateTextureRegion(CompositableClient* aCompositable,
                                   const ThebesBufferData& aThebesBufferData,
                                   const nsIntRegion& aUpdatedRegion) = 0;

  virtual void Destroy(CompositableChild* aCompositable);

  virtual bool DestroyInTransaction(PTextureChild* aTexture, bool synchronously) = 0;
  virtual bool DestroyInTransaction(PCompositableChild* aCompositable, bool synchronously) = 0;

  /**
   * Tell the CompositableHost on the compositor side to remove the texture
   * from the CompositableHost.
   * This function does not delete the TextureHost corresponding to the
   * TextureClient passed in parameter.
   * When the TextureClient has TEXTURE_DEALLOCATE_CLIENT flag,
   * the transaction becomes synchronous.
   */
  virtual void RemoveTextureFromCompositable(CompositableClient* aCompositable,
                                             TextureClient* aTexture) = 0;

  /**
   * Tell the CompositableHost on the compositor side to remove the texture
   * from the CompositableHost. The compositor side sends back transaction
   * complete message.
   * This function does not delete the TextureHost corresponding to the
   * TextureClient passed in parameter.
   * It is used when the TextureClient recycled.
   * Only ImageBridge implements it.
   */
  virtual void RemoveTextureFromCompositableAsync(AsyncTransactionTracker* aAsyncTransactionTracker,
                                                  CompositableClient* aCompositable,
                                                  TextureClient* aTexture) {}

  struct TimedTextureClient {
    TimedTextureClient()
        : mTextureClient(nullptr), mFrameID(0), mProducerID(0) {}

    TextureClient* mTextureClient;
    TimeStamp mTimeStamp;
    nsIntRect mPictureRect;
    int32_t mFrameID;
    int32_t mProducerID;
  };
  /**
   * Tell the CompositableHost on the compositor side what textures to use for
   * the next composition.
   */
  virtual void UseTextures(CompositableClient* aCompositable,
                           const nsTArray<TimedTextureClient>& aTextures) = 0;
  virtual void UseComponentAlphaTextures(CompositableClient* aCompositable,
                                         TextureClient* aClientOnBlack,
                                         TextureClient* aClientOnWhite) = 0;

  virtual void UpdateFwdTransactionId() = 0;
  virtual uint64_t GetFwdTransactionId() = 0;

  virtual bool InForwarderThread() = 0;

  void AssertInForwarderThread() {
    MOZ_ASSERT(InForwarderThread());
  }

protected:
  nsTArray<RefPtr<TextureClient> > mTexturesToRemove;
  nsTArray<RefPtr<CompositableClient>> mCompositableClientsToRemove;
};

} // namespace layers
} // namespace mozilla

#endif
