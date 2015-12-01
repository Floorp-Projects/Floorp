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
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "nsRegion.h"                   // for nsIntRegion
#include "mozilla/gfx/Rect.h"

namespace mozilla {
namespace layers {

class CompositableClient;
class AsyncTransactionTracker;
class ImageContainer;
struct TextureFactoryIdentifier;
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
 */
class CompositableForwarder : public ISurfaceAllocator
{
public:

  CompositableForwarder()
    : mSerial(++sSerialCounter)
  {}

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
   * Create a TextureChild/Parent pair as as well as the TextureHost on the parent side.
   */
  virtual PTextureChild* CreateTexture(
    const SurfaceDescriptor& aSharedData,
    LayersBackend aLayersBackend,
    TextureFlags aFlags) = 0;

  /**
   * Communicate to the compositor that aRegion in the texture identified by
   * aCompositable and aIdentifier has been updated to aThebesBuffer.
   */
  virtual void UpdateTextureRegion(CompositableClient* aCompositable,
                                   const ThebesBufferData& aThebesBufferData,
                                   const nsIntRegion& aUpdatedRegion) = 0;

#ifdef MOZ_WIDGET_GONK
  virtual void UseOverlaySource(CompositableClient* aCompositabl,
                                const OverlaySource& aOverlay,
                                const gfx::IntRect& aPictureRect) = 0;
#endif

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

  /**
   * Holds a reference to a TextureClient until after the next
   * compositor transaction, and then drops it.
   */
  virtual void HoldUntilTransaction(TextureClient* aClient)
  {
    if (aClient) {
      mTexturesToRemove.AppendElement(aClient);
    }
  }

  /**
   * Forcibly remove texture data from TextureClient
   * This function needs to be called after a tansaction with Compositor.
   */
  virtual void RemoveTexturesIfNecessary()
  {
    mTexturesToRemove.Clear();
  }

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

  virtual void SendPendingAsyncMessges() = 0;

  void IdentifyTextureHost(const TextureFactoryIdentifier& aIdentifier);

  virtual int32_t GetMaxTextureSize() const override
  {
    return mTextureFactoryIdentifier.mMaxTextureSize;
  }

  bool IsOnCompositorSide() const override { return false; }

  /**
   * Returns the type of backend that is used off the main thread.
   * We only don't allow changing the backend type at runtime so this value can
   * be queried once and will not change until Gecko is restarted.
   */
  LayersBackend GetCompositorBackendType() const
  {
    return mTextureFactoryIdentifier.mParentBackend;
  }

  bool SupportsTextureBlitting() const
  {
    return mTextureFactoryIdentifier.mSupportsTextureBlitting;
  }

  bool SupportsPartialUploads() const
  {
    return mTextureFactoryIdentifier.mSupportsPartialUploads;
  }

  const TextureFactoryIdentifier& GetTextureFactoryIdentifier() const
  {
    return mTextureFactoryIdentifier;
  }

  int32_t GetSerial() { return mSerial; }

  SyncObject* GetSyncObject() { return mSyncObject; }

protected:
  TextureFactoryIdentifier mTextureFactoryIdentifier;
  nsTArray<RefPtr<TextureClient> > mTexturesToRemove;
  RefPtr<SyncObject> mSyncObject;
  const int32_t mSerial;
  static mozilla::Atomic<int32_t> sSerialCounter;
};

} // namespace layers
} // namespace mozilla

#endif
