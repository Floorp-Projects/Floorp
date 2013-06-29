/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CONTENTCLIENT_H
#define MOZILLA_GFX_CONTENTCLIENT_H

#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/CompositableClient.h"
#include "gfxReusableSurfaceWrapper.h"
#include "mozilla/layers/TextureClient.h"
#include "ThebesLayerBuffer.h"
#include "ipc/AutoOpenSurface.h"
#include "ipc/ShadowLayerChild.h"
#include "gfxPlatform.h"

namespace mozilla {
namespace layers {

class BasicLayerManager;

/**
 * A compositable client for Thebes layers. These are different to Image/Canvas
 * clients due to sending a valid region across IPC and because we do a lot more
 * optimisation work, encapsualted in ThebesLayerBuffers.
 *
 * We use content clients for OMTC and non-OMTC, basic rendering so that
 * BasicThebesLayer has only one interface to deal with. We support single and
 * double buffered flavours. For tiled layers, we do not use a ContentClient
 * although we do have a ContentHost, and we do use texture clients and texture
 * hosts.
 *
 * The interface presented by ContentClient is used by the BasicThebesLayer
 * methods - PaintThebes, which is the same for MT and OMTC, and PaintBuffer
 * which is different (the OMTC one does a little more). The 'buffer' in the
 * names of a lot of these method is actually the TextureClient. But, 'buffer'
 * for the ThebesLayerBuffer (as in SetBuffer) means a gfxSurface. See the
 * comments for SetBuffer and SetBufferProvider in ThebesLayerBuffer. To keep
 * these mapped buffers alive, we store a pointer in mOldTextures if the
 * ThebesLayerBuffer's surface is not the one from our texture client, once we
 * are done painting we unmap the surface/texture client and don't need to keep
 * it alive anymore, so we clear mOldTextures.
 *
 * The sequence for painting is: BeginPaint (lock our texture client into the
 * buffer), Paint the layer which calls SyncFrontBufferToBackBuffer (which gets
 * the surface back from the buffer and puts it back in again with the buffer
 * attributes), call BeginPaint on the buffer, call PaintBuffer on the layer
 * (which does the actual painting via the callback, then calls Updated on the
 * ContentClient, finally calling EndPaint on the ContentClient (which unlocks
 * the surface from the buffer)).
 *
 * Updated() is called when we are done painting and packages up the change in
 * the appropriate way to be passed to the compositor in the layers transation.
 *
 * SwapBuffers is called in repsonse to the transaction reply from the compositor.
 */
class ContentClient : public CompositableClient
{
public:
  /**
   * Creates, configures, and returns a new content client. If necessary, a
   * message will be sent to the compositor to create a corresponding content
   * host.
   */
  static TemporaryRef<ContentClient> CreateContentClient(CompositableForwarder* aFwd);

  ContentClient(CompositableForwarder* aForwarder)
  : CompositableClient(aForwarder)
  {}
  virtual ~ContentClient()
  {}


  virtual void Clear() = 0;
  virtual ThebesLayerBuffer::PaintState BeginPaintBuffer(ThebesLayer* aLayer,
                                                         ThebesLayerBuffer::ContentType aContentType,
                                                         uint32_t aFlags) = 0;

  // Sync front/back buffers content
  // After executing, the new back buffer has the same (interesting) pixels as
  // the new front buffer, and mValidRegion et al. are correct wrt the new
  // back buffer (i.e. as they were for the old back buffer)
  virtual void SyncFrontBufferToBackBuffer() {}

  // Called as part of the layers transation reply. Conveys data about our
  // buffer(s) from the compositor. If appropriate we should swap references
  // to our buffers.
  virtual void SwapBuffers(const nsIntRegion& aFrontUpdatedRegion) {}

  // call before and after painting into this content client
  virtual void BeginPaint() {}
  virtual void EndPaint() {}
};

/**
 * A ContentClient for use with OMTC.
 */
class ContentClientRemote : public ContentClient
{
public:
  ContentClientRemote(CompositableForwarder* aForwarder)
    : ContentClient(aForwarder)
  {}

  virtual void Updated(const nsIntRegion& aRegionToDraw,
                       const nsIntRegion& aVisibleRegion,
                       bool aDidSelfCopy) = 0;
};

// thin wrapper around BasicThebesLayerBuffer, for on-mtc
class ContentClientBasic : public ContentClient
                         , protected ThebesLayerBuffer
{
public:
  ContentClientBasic(CompositableForwarder* aForwarder,
                     BasicLayerManager* aManager);

  typedef ThebesLayerBuffer::PaintState PaintState;
  typedef ThebesLayerBuffer::ContentType ContentType;

  virtual void Clear() { ThebesLayerBuffer::Clear(); }
  PaintState BeginPaintBuffer(ThebesLayer* aLayer, ContentType aContentType,
                              uint32_t aFlags)
  {
    return ThebesLayerBuffer::BeginPaint(aLayer, aContentType, aFlags);
  }

  void DrawTo(ThebesLayer* aLayer, gfxContext* aTarget, float aOpacity,
              gfxASurface* aMask, const gfxMatrix* aMaskTransform)
  {
    ThebesLayerBuffer::DrawTo(aLayer, aTarget, aOpacity, aMask, aMaskTransform);
  }

  virtual already_AddRefed<gfxASurface> CreateBuffer(ContentType aType,
                                                     const nsIntRect& aRect,
                                                     uint32_t aFlags,
                                                     gfxASurface**) MOZ_OVERRIDE;
  virtual TemporaryRef<gfx::DrawTarget>
    CreateDTBuffer(ContentType aType, const nsIntRect& aRect, uint32_t aFlags);

  virtual TextureInfo GetTextureInfo() const MOZ_OVERRIDE
  {
    MOZ_CRASH("Should not be called on non-remote ContentClient");
    return TextureInfo();
  }

private:
  BasicLayerManager* mManager;
};

/**
 * A ContentClientRemote backed by a ThebesLayerBuffer.
 *
 * When using a ContentClientRemote, SurfaceDescriptors are created on
 * the rendering side and destroyed on the compositing side. They are only
 * passed from one side to the other when the TextureClient/Hosts are created.
 * *Ownership* of the SurfaceDescriptor moves from the rendering side to the
 * comnpositing side with the create message (send from CreateBuffer) which
 * tells the compositor that TextureClients have been created and that the
 * compositor should assign the corresponding TextureHosts to our corresponding
 * ContentHost.
 *
 * If the size or type of our buffer(s) change(s), then we simply destroy and
 * create them.
 */
class ContentClientRemoteBuffer : public ContentClientRemote
                                , protected ThebesLayerBuffer
{
  using ThebesLayerBuffer::BufferRect;
  using ThebesLayerBuffer::BufferRotation;
public:
  ContentClientRemoteBuffer(CompositableForwarder* aForwarder)
    : ContentClientRemote(aForwarder)
    , ThebesLayerBuffer(ContainsVisibleBounds)
    , mTextureClient(nullptr)
    , mIsNewBuffer(false)
    , mFrontAndBackBufferDiffer(false)
    , mContentType(gfxASurface::CONTENT_COLOR_ALPHA)
  {}

  typedef ThebesLayerBuffer::PaintState PaintState;
  typedef ThebesLayerBuffer::ContentType ContentType;

  virtual void Clear() { ThebesLayerBuffer::Clear(); }
  PaintState BeginPaintBuffer(ThebesLayer* aLayer, ContentType aContentType,
                              uint32_t aFlags)
  {
    return ThebesLayerBuffer::BeginPaint(aLayer, aContentType, aFlags);
  }

  /**
   * Begin/End Paint map a gfxASurface from the texture client
   * into the buffer of ThebesLayerBuffer. The surface is only
   * valid when the texture client is locked, so is mapped out
   * of ThebesLayerBuffer when we are done painting.
   * None of the underlying buffer attributes (rect, rotation)
   * are affected by mapping/unmapping.
   */
  virtual void BeginPaint() MOZ_OVERRIDE;
  virtual void EndPaint() MOZ_OVERRIDE;

  virtual void Updated(const nsIntRegion& aRegionToDraw,
                       const nsIntRegion& aVisibleRegion,
                       bool aDidSelfCopy);

  virtual void SwapBuffers(const nsIntRegion& aFrontUpdatedRegion) MOZ_OVERRIDE;

  // Expose these protected methods from the superclass.
  virtual const nsIntRect& BufferRect() const
  {
    return ThebesLayerBuffer::BufferRect();
  }
  virtual const nsIntPoint& BufferRotation() const
  {
    return ThebesLayerBuffer::BufferRotation();
  }

  virtual already_AddRefed<gfxASurface> CreateBuffer(ContentType aType,
                                                     const nsIntRect& aRect,
                                                     uint32_t aFlags,
                                                     gfxASurface** aWhiteSurface) MOZ_OVERRIDE;
  virtual TemporaryRef<gfx::DrawTarget> CreateDTBuffer(ContentType aType,
                                                       const nsIntRect& aRect,
                                                       uint32_t aFlags) MOZ_OVERRIDE;

  virtual bool SupportsAzureContent() const MOZ_OVERRIDE
  {
    return gfxPlatform::GetPlatform()->SupportsAzureContent();
  }

  void DestroyBuffers();

  virtual TextureInfo GetTextureInfo() const MOZ_OVERRIDE
  {
    return mTextureInfo;
  }

protected:
  virtual nsIntRegion GetUpdatedRegion(const nsIntRegion& aRegionToDraw,
                                       const nsIntRegion& aVisibleRegion,
                                       bool aDidSelfCopy);

  // create and configure mTextureClient
  void BuildTextureClients(ContentType aType,
                           const nsIntRect& aRect,
                           uint32_t aFlags);

  // Create the front buffer for the ContentClient/Host pair if necessary
  // and notify the compositor that we have created the buffer(s).
  virtual void CreateFrontBufferAndNotify(const nsIntRect& aBufferRect) = 0;
  virtual void DestroyFrontBuffer() {}
  // We're about to hand off to the compositor, if you've got a back buffer,
  // lock it now.
  virtual void LockFrontBuffer() {}

  RefPtr<TextureClient> mTextureClient;
  RefPtr<TextureClient> mTextureClientOnWhite;
  // keep a record of texture clients we have created and need to keep
  // around, then unlock when we are done painting
  nsTArray<RefPtr<TextureClient> > mOldTextures;

  TextureInfo mTextureInfo;
  bool mIsNewBuffer;
  bool mFrontAndBackBufferDiffer;
  gfx::IntSize mSize;
  ContentType mContentType;
};

/**
 * A double buffered ContentClient. mTextureClient is the back buffer, which
 * we draw into. mFrontClient is the front buffer which we may read from, but
 * not write to, when the compositor does not have the 'soft' lock. We can write
 * into mTextureClient at any time.
 *
 * The ContentHost keeps a reference to both corresponding texture hosts, in
 * repsonse to our UpdateTextureRegion message, the compositor swaps its
 * references. In repsonse to the compositor's reply we swap our references
 * (in SwapBuffers).
 */
class ContentClientDoubleBuffered : public ContentClientRemoteBuffer
{
public:
  ContentClientDoubleBuffered(CompositableForwarder* aFwd)
    : ContentClientRemoteBuffer(aFwd)
  {
    mTextureInfo.mCompositableType = BUFFER_CONTENT_DIRECT;
  }
  ~ContentClientDoubleBuffered();

  virtual void SwapBuffers(const nsIntRegion& aFrontUpdatedRegion) MOZ_OVERRIDE;

  virtual void SyncFrontBufferToBackBuffer() MOZ_OVERRIDE;

protected:
  virtual void CreateFrontBufferAndNotify(const nsIntRect& aBufferRect) MOZ_OVERRIDE;
  virtual void DestroyFrontBuffer() MOZ_OVERRIDE;
  virtual void LockFrontBuffer() MOZ_OVERRIDE;

private:
  void UpdateDestinationFrom(const RotatedBuffer& aSource,
                             const nsIntRegion& aUpdateRegion);

  RefPtr<TextureClient> mFrontClient;
  RefPtr<TextureClient> mFrontClientOnWhite;
  nsIntRegion mFrontUpdatedRegion;
  nsIntRect mFrontBufferRect;
  nsIntPoint mFrontBufferRotation;
};

/**
 * A single buffered ContentClient. We have a single TextureClient/Host
 * which we update and then send a message to the compositor that we are
 * done updating. It is not safe for the compositor to use the corresponding
 * TextureHost's memory directly, it most upload it to video memory of some
 * kind. We are free to modify the TextureClient once we receive reply from
 * the compositor.
 */
class ContentClientSingleBuffered : public ContentClientRemoteBuffer
{
public:
  ContentClientSingleBuffered(CompositableForwarder* aFwd)
    : ContentClientRemoteBuffer(aFwd)
  {
    mTextureInfo.mCompositableType = BUFFER_CONTENT;    
  }
  ~ContentClientSingleBuffered();

  virtual void SyncFrontBufferToBackBuffer() MOZ_OVERRIDE;

protected:
  virtual void CreateFrontBufferAndNotify(const nsIntRect& aBufferRect) MOZ_OVERRIDE;
};

/**
 * A single buffered ContentClient that creates temporary buffers which are 
 * used to update the host-side texture. The ownership of the buffers is
 * passed to the host side during the transaction, and we need to create
 * new ones each frame.
 */
class ContentClientIncremental : public ContentClientRemote
{
public:
  ContentClientIncremental(CompositableForwarder* aFwd)
    : ContentClientRemote(aFwd)
    , mContentType(gfxASurface::CONTENT_COLOR_ALPHA)
    , mHasBuffer(false)
    , mHasBufferOnWhite(false)
  {
    mTextureInfo.mCompositableType = BUFFER_CONTENT_INC;
  }

  typedef ThebesLayerBuffer::PaintState PaintState;
  typedef ThebesLayerBuffer::ContentType ContentType;

  virtual TextureInfo GetTextureInfo() const
  {
    return mTextureInfo;
  }

  virtual void Clear()
  {
    mBufferRect.SetEmpty();
    mHasBuffer = false;
    mHasBufferOnWhite = false;
  }
  virtual ThebesLayerBuffer::PaintState BeginPaintBuffer(ThebesLayer* aLayer,
                                                         ThebesLayerBuffer::ContentType aContentType,
                                                         uint32_t aFlags);

  virtual void Updated(const nsIntRegion& aRegionToDraw,
                       const nsIntRegion& aVisibleRegion,
                       bool aDidSelfCopy);

  virtual void EndPaint()
  {
    if (IsSurfaceDescriptorValid(mUpdateDescriptor)) {
      mForwarder->DestroySharedSurface(&mUpdateDescriptor);
    }
    if (IsSurfaceDescriptorValid(mUpdateDescriptorOnWhite)) {
      mForwarder->DestroySharedSurface(&mUpdateDescriptorOnWhite);
    }
  }

private:

  enum BufferType{
    BUFFER_BLACK,
    BUFFER_WHITE
  };

  void NotifyBufferCreated(ContentType aType, uint32_t aFlags)
  {
    mTextureInfo.mTextureFlags = aFlags | HostRelease;
    mContentType = aType;

    mForwarder->CreatedIncrementalBuffer(this,
                                         mTextureInfo,
                                         mBufferRect);

  }

  already_AddRefed<gfxASurface> GetUpdateSurface(BufferType aType, nsIntRegion& aUpdateRegion);

  TextureInfo mTextureInfo;
  nsIntRect mBufferRect;
  nsIntPoint mBufferRotation;

  SurfaceDescriptor mUpdateDescriptor;
  SurfaceDescriptor mUpdateDescriptorOnWhite;

  ContentType mContentType;

  bool mHasBuffer;
  bool mHasBufferOnWhite;
};

}
}

#endif
