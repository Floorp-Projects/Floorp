/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BUFFERCLIENT_H
#define MOZILLA_GFX_BUFFERCLIENT_H

#include "mozilla/layers/PCompositableChild.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace layers {

class CompositableChild;
class CompositableClient;
class TextureClient;
class ShadowLayersChild;
class ImageBridgeChild;
class ShadowableLayer;
class CompositableForwarder;
class CompositableChild;

/**
 * CompositableClient manages the texture-specific logic for composite layers,
 * independently of the layer. It is the content side of a ConmpositableClient/
 * CompositableHost pair.
 *
 * CompositableClient's purpose is to send texture data to the compositor side
 * along with any extra information about how the texture is to be composited.
 * Things like opacity or transformation belong to layer and not compositable.
 *
 * Since Compositables are independent of layers it is possible to create one,
 * connect it to the compositor side, and start sending images to it. This alone
 * is arguably not very useful, but it means that as long as a shadow layer can
 * do the proper magic to find a reference to the right CompositableHost on the
 * Compositor side, a Compositable client can be used outside of the main
 * shadow layer forwarder machinery that is used on the main thread.
 *
 * The first step is to create a Compositable client and call Connect().
 * Connect() creates the underlying IPDL actor (see CompositableChild) and the
 * corresponding CompositableHost on the other side.
 *
 * To do in-transaction texture transfer (the default), call
 * ShadowLayerForwarder::Attach(CompositableClient*, ShadowableLayer*). This
 * will let the ShadowLayer on the compositor side know which CompositableHost
 * to use for compositing.
 *
 * To do async texture transfer (like async-video), the CompositableClient
 * should be created with a different CompositableForwarder (like
 * ImageBridgeChild) and attachment is done with
 * CompositableForwarder::AttachAsyncCompositable that takes an identifier
 * instead of a CompositableChild, since the CompositableClient is not managed
 * by this layer forwarder (the matching uses a global map on the compositor side,
 * see CompositableMap in ImageBridgeParent.cpp)
 *
 * Subclasses: Thebes layers use ContentClients, ImageLayers use ImageClients,
 * Canvas layers use CanvasClients (but ImageHosts). We have a different subclass
 * where we have a different way of interfacing with the textures - in terms of
 * drawing into the compositable and/or passing its contents to the compostior.
 */
class CompositableClient : public RefCounted<CompositableClient>
{
public:
  CompositableClient(CompositableForwarder* aForwarder)
  : mCompositableChild(nullptr), mForwarder(aForwarder)
  {
    MOZ_COUNT_CTOR(CompositableClient);
  }

  virtual ~CompositableClient();

  virtual TextureInfo GetTextureInfo() const
  {
    MOZ_NOT_REACHED("This method should be overridden");
    return TextureInfo();
  }

  LayersBackend GetCompositorBackendType() const;

  TemporaryRef<TextureClient>
  CreateTextureClient(TextureClientType aTextureClientType);

  virtual void SetDescriptorFromReply(TextureIdentifier aTextureId,
                                      const SurfaceDescriptor& aDescriptor)
  {
    MOZ_NOT_REACHED("If you want to call this, you should have implemented it");
  }

  /**
   * Establishes the connection with compositor side through IPDL
   */
  virtual bool Connect();

  void Destroy();

  CompositableChild* GetIPDLActor() const;

  // should only be called by a CompositableForwarder
  void SetIPDLActor(CompositableChild* aChild);

  CompositableForwarder* GetForwarder() const
  {
    return mForwarder;
  }

  /**
   * This identifier is what lets us attach async compositables with a shadow
   * layer. It is not used if the compositable is used with the regulat shadow
   * layer forwarder.
   */
  uint64_t GetAsyncID() const;

protected:
  CompositableChild* mCompositableChild;
  CompositableForwarder* mForwarder;
};

/**
 * IPDL actor used by CompositableClient to match with its corresponding
 * CompositableHost on the compositor side.
 *
 * CompositableChild is owned by a CompositableClient.
 */
class CompositableChild : public PCompositableChild
{
public:
  CompositableChild()
  : mCompositableClient(nullptr), mID(0)
  {
    MOZ_COUNT_CTOR(CompositableChild);
  }
  ~CompositableChild()
  {
    MOZ_COUNT_DTOR(CompositableChild);
  }

  void Destroy();

  void SetClient(CompositableClient* aClient)
  {
    mCompositableClient = aClient;
  }

  CompositableClient* GetCompositableClient() const
  {
    return mCompositableClient;
  }

  void SetAsyncID(uint64_t aID) { mID = aID; }
  uint64_t GetAsyncID() const
  {
    return mID;
  }
private:
  CompositableClient* mCompositableClient;
  uint64_t mID;
};

} // namespace
} // namespace

#endif
