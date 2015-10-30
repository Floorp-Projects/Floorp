/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BUFFERCLIENT_H
#define MOZILLA_GFX_BUFFERCLIENT_H

#include <stdint.h>                     // for uint64_t
#include <vector>                       // for vector
#include <map>                          // for map
#include "mozilla/Assertions.h"         // for MOZ_CRASH
#include "mozilla/RefPtr.h"             // for already_AddRefed, RefCounted
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "mozilla/layers/AsyncTransactionTracker.h" // for AsyncTransactionTracker
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "mozilla/layers/TextureClientRecycleAllocator.h" // for TextureClientRecycleAllocator
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc

namespace mozilla {
namespace layers {

class CompositableClient;
class BufferTextureClient;
class ImageBridgeChild;
class ImageContainer;
class CompositableForwarder;
class CompositableChild;
class PCompositableChild;

/**
 * Handle RemoveTextureFromCompositableAsync() transaction.
 */
class RemoveTextureFromCompositableTracker : public AsyncTransactionTracker {
public:
  explicit RemoveTextureFromCompositableTracker(AsyncTransactionWaiter* aWaiter = nullptr)
    : AsyncTransactionTracker(aWaiter)
  {
    MOZ_COUNT_CTOR(RemoveTextureFromCompositableTracker);
  }

protected:
  ~RemoveTextureFromCompositableTracker()
  {
    MOZ_COUNT_DTOR(RemoveTextureFromCompositableTracker);
    ReleaseTextureClient();
  }

public:
  virtual void Complete() override
  {
    ReleaseTextureClient();
  }

  virtual void Cancel() override
  {
    ReleaseTextureClient();
  }

  virtual void SetTextureClient(TextureClient* aTextureClient) override
  {
    ReleaseTextureClient();
    mTextureClient = aTextureClient;
  }

  virtual void SetReleaseFenceHandle(FenceHandle& aReleaseFenceHandle) override
  {
    if (mTextureClient) {
      mTextureClient->SetReleaseFenceHandle(aReleaseFenceHandle);
    }
  }

protected:
  void ReleaseTextureClient();

private:
  RefPtr<TextureClient> mTextureClient;
};

/**
 * CompositableClient manages the texture-specific logic for composite layers,
 * independently of the layer. It is the content side of a CompositableClient/
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
 * will let the LayerComposite on the compositor side know which CompositableHost
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
 * Subclasses: Painted layers use ContentClients, ImageLayers use ImageClients,
 * Canvas layers use CanvasClients (but ImageHosts). We have a different subclass
 * where we have a different way of interfacing with the textures - in terms of
 * drawing into the compositable and/or passing its contents to the compostior.
 */
class CompositableClient
{
protected:
  virtual ~CompositableClient();

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositableClient)

  explicit CompositableClient(CompositableForwarder* aForwarder, TextureFlags aFlags = TextureFlags::NO_FLAGS);

  virtual TextureInfo GetTextureInfo() const = 0;

  LayersBackend GetCompositorBackendType() const;

  already_AddRefed<BufferTextureClient>
  CreateBufferTextureClient(gfx::SurfaceFormat aFormat,
                            gfx::IntSize aSize,
                            gfx::BackendType aMoz2dBackend = gfx::BackendType::NONE,
                            TextureFlags aFlags = TextureFlags::DEFAULT);

  already_AddRefed<TextureClient>
  CreateTextureClientForDrawing(gfx::SurfaceFormat aFormat,
                                gfx::IntSize aSize,
                                BackendSelector aSelector,
                                TextureFlags aTextureFlags,
                                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT);

  /**
   * Establishes the connection with compositor side through IPDL
   */
  virtual bool Connect(ImageContainer* aImageContainer = nullptr);

  void Destroy();

  bool IsDestroyed() { return mDestroyed; }

  PCompositableChild* GetIPDLActor() const;

  // should only be called by a CompositableForwarder
  virtual void SetIPDLActor(CompositableChild* aChild);

  CompositableForwarder* GetForwarder() const
  {
    return mForwarder;
  }

  /**
   * This identifier is what lets us attach async compositables with a shadow
   * layer. It is not used if the compositable is used with the regular shadow
   * layer forwarder.
   *
   * If this returns zero, it means the compositable is not async (it is used
   * on the main thread).
   */
  uint64_t GetAsyncID() const;

  /**
   * Tells the Compositor to create a TextureHost for this TextureClient.
   */
  virtual bool AddTextureClient(TextureClient* aClient);

  /**
   * A hook for the when the Compositable is detached from it's layer.
   */
  virtual void OnDetach() {}

  /**
   * Clear any resources that are not immediately necessary. This may be called
   * in low-memory conditions.
   */
  virtual void ClearCachedResources();

  /**
   * Should be called when deataching a TextureClient from a Compositable, because
   * some platforms need to do some extra book keeping when this happens (for
   * example to properly keep track of fences on Gonk).
   *
   * See AutoRemoveTexture to automatically invoke this at the end of a scope.
   */
  virtual void RemoveTexture(TextureClient* aTexture);

  static CompositableClient* FromIPDLActor(PCompositableChild* aActor);

  /**
   * Allocate and deallocate a CompositableChild actor.
   *
   * CompositableChild is an implementation detail of CompositableClient that is not
   * exposed to the rest of the code base. CreateIPDLActor and DestroyIPDLActor
   * are for use with the managing IPDL protocols only (so that they can
   * implement AllocCompositableChild and DeallocPCompositableChild).
   */
  static PCompositableChild* CreateIPDLActor();

  static bool DestroyIPDLActor(PCompositableChild* actor);

  void InitIPDLActor(PCompositableChild* aActor, uint64_t aAsyncID = 0);

  static void TransactionCompleteted(PCompositableChild* aActor, uint64_t aTransactionId);

  static void HoldUntilComplete(PCompositableChild* aActor, AsyncTransactionTracker* aTracker);

  static uint64_t GetTrackersHolderId(PCompositableChild* aActor);

  TextureFlags GetTextureFlags() const { return mTextureFlags; }

  TextureClientRecycleAllocator* GetTextureClientRecycler();

  static void DumpTextureClient(std::stringstream& aStream, TextureClient* aTexture);
protected:
  CompositableChild* mCompositableChild;
  RefPtr<CompositableForwarder> mForwarder;
  // Some layers may want to enforce some flags to all their textures
  // (like disallowing tiling)
  TextureFlags mTextureFlags;
  bool mDestroyed;
  RefPtr<TextureClientRecycleAllocator> mTextureClientRecycler;

  friend class CompositableChild;
};

/**
 * Helper to call RemoveTexture at the end of a scope.
 */
struct AutoRemoveTexture
{
  explicit AutoRemoveTexture(CompositableClient* aCompositable,
                             TextureClient* aTexture = nullptr)
    : mTexture(aTexture)
    , mCompositable(aCompositable)
  {}

  ~AutoRemoveTexture();

  RefPtr<TextureClient> mTexture;
private:
  CompositableClient* mCompositable;
};

} // namespace layers
} // namespace mozilla

#endif
