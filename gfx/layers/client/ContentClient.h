/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CONTENTCLIENT_H
#define MOZILLA_GFX_CONTENTCLIENT_H

#include <stdint.h>                     // for uint32_t
#include "RotatedBuffer.h"              // for RotatedContentBuffer, etc
#include "gfxTypes.h"
#include "gfxPlatform.h"                // for gfxPlatform
#include "mozilla/Assertions.h"         // for MOZ_CRASH
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr, TemporaryRef
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray

class gfxContext;
class gfxASurface;

namespace mozilla {
namespace gfx {
class DrawTarget;
}

namespace layers {

class BasicLayerManager;
class ThebesLayer;

/**
 * A compositable client for Thebes layers. These are different to Image/Canvas
 * clients due to sending a valid region across IPC and because we do a lot more
 * optimisation work, encapsualted in RotatedContentBuffers.
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
 * for the RotatedContentBuffer (as in SetBuffer) means a gfxSurface. See the
 * comments for SetBuffer and SetBufferProvider in RotatedContentBuffer. To keep
 * these mapped buffers alive, we store a pointer in mOldTextures if the
 * RotatedContentBuffer's surface is not the one from our texture client, once we
 * are done painting we unmap the surface/texture client and don't need to keep
 * it alive anymore, so we clear mOldTextures.
 *
 * The sequence for painting is: call BeginPaint on the content client;
 * call PrepareFrame on the content client;
 * call BeginPaintBuffer on the content client. That will initialise the buffer
 * for painting, by calling RotatedContentBuffer::BeginPaint (usually) which
 * will call back to ContentClient::FinalizeFrame to finalize update of the
 * buffers before drawing (i.e., it finalizes the previous frame). Then call
 * BorrowDrawTargetForPainting to get a DrawTarget to paint into. Then paint.
 * Then return that DrawTarget using ReturnDrawTarget.
 * Call EndPaint on the content client;
 * call OnTransaction on the content client.
 *
 * SwapBuffers is called in response to the transaction reply from the compositor.
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
  virtual RotatedContentBuffer::PaintState BeginPaintBuffer(ThebesLayer* aLayer,
                                                            uint32_t aFlags) = 0;
  virtual gfx::DrawTarget* BorrowDrawTargetForPainting(ThebesLayer* aLayer,
                                                       const RotatedContentBuffer::PaintState& aPaintState) = 0;
  virtual void ReturnDrawTargetToBuffer(gfx::DrawTarget*& aReturned) = 0;

  virtual void PrepareFrame() {}

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

// thin wrapper around RotatedContentBuffer, for on-mtc
class ContentClientBasic : public ContentClient
                         , protected RotatedContentBuffer
{
public:
  ContentClientBasic();

  typedef RotatedContentBuffer::PaintState PaintState;
  typedef RotatedContentBuffer::ContentType ContentType;

  virtual void Clear() { RotatedContentBuffer::Clear(); }
  virtual PaintState BeginPaintBuffer(ThebesLayer* aLayer,
                                      uint32_t aFlags) MOZ_OVERRIDE
  {
    return RotatedContentBuffer::BeginPaint(aLayer, aFlags);
  }
  virtual gfx::DrawTarget* BorrowDrawTargetForPainting(ThebesLayer* aLayer,
                                                       const PaintState& aPaintState) MOZ_OVERRIDE
  {
    return RotatedContentBuffer::BorrowDrawTargetForPainting(aLayer, aPaintState);
  }
  virtual void ReturnDrawTargetToBuffer(gfx::DrawTarget*& aReturned) MOZ_OVERRIDE
  {
    BorrowDrawTarget::ReturnDrawTarget(aReturned);
  }

  void DrawTo(ThebesLayer* aLayer,
              gfx::DrawTarget* aTarget,
              float aOpacity,
              gfx::CompositionOp aOp,
              gfxASurface* aMask,
              const gfx::Matrix* aMaskTransform)
  {
    RotatedContentBuffer::DrawTo(aLayer, aTarget, aOpacity, aOp,
                                 aMask, aMaskTransform);
  }

  virtual void CreateBuffer(ContentType aType, const nsIntRect& aRect, uint32_t aFlags,
                            RefPtr<gfx::DrawTarget>* aBlackDT, RefPtr<gfx::DrawTarget>* aWhiteDT) MOZ_OVERRIDE;

  virtual TextureInfo GetTextureInfo() const MOZ_OVERRIDE
  {
    MOZ_CRASH("Should not be called on non-remote ContentClient");
  }
};

/**
 * A ContentClientRemote backed by a RotatedContentBuffer.
 *
 * When using a ContentClientRemote, SurfaceDescriptors are created on
 * the rendering side and destroyed on the compositing side. They are only
 * passed from one side to the other when the TextureClient/Hosts are created.
 * *Ownership* of the SurfaceDescriptor moves from the rendering side to the
 * compositing side with the create message (send from CreateBuffer) which
 * tells the compositor that TextureClients have been created and that the
 * compositor should assign the corresponding TextureHosts to our corresponding
 * ContentHost.
 *
 * If the size or type of our buffer(s) change(s), then we simply destroy and
 * create them.
 */
// Version using new texture clients
class ContentClientRemoteBuffer : public ContentClientRemote
                                , protected RotatedContentBuffer
{
  using RotatedContentBuffer::BufferRect;
  using RotatedContentBuffer::BufferRotation;
public:
  ContentClientRemoteBuffer(CompositableForwarder* aForwarder)
    : ContentClientRemote(aForwarder)
    , RotatedContentBuffer(ContainsVisibleBounds)
    , mIsNewBuffer(false)
    , mFrontAndBackBufferDiffer(false)
    , mSurfaceFormat(gfx::SurfaceFormat::B8G8R8A8)
  {}

  typedef RotatedContentBuffer::PaintState PaintState;
  typedef RotatedContentBuffer::ContentType ContentType;

  virtual void Clear()
  {
    RotatedContentBuffer::Clear();
    mTextureClient = nullptr;
    mTextureClientOnWhite = nullptr;
  }

  virtual PaintState BeginPaintBuffer(ThebesLayer* aLayer,
                                      uint32_t aFlags) MOZ_OVERRIDE
  {
    return RotatedContentBuffer::BeginPaint(aLayer, aFlags);
  }
  virtual gfx::DrawTarget* BorrowDrawTargetForPainting(ThebesLayer* aLayer,
                                                       const PaintState& aPaintState) MOZ_OVERRIDE
  {
    return RotatedContentBuffer::BorrowDrawTargetForPainting(aLayer, aPaintState);
  }
  virtual void ReturnDrawTargetToBuffer(gfx::DrawTarget*& aReturned) MOZ_OVERRIDE
  {
    BorrowDrawTarget::ReturnDrawTarget(aReturned);
  }

  /**
   * Begin/End Paint map a gfxASurface from the texture client
   * into the buffer of RotatedBuffer. The surface is only
   * valid when the texture client is locked, so is mapped out
   * of RotatedContentBuffer when we are done painting.
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
    return RotatedContentBuffer::BufferRect();
  }
  virtual const nsIntPoint& BufferRotation() const
  {
    return RotatedContentBuffer::BufferRotation();
  }

  virtual void CreateBuffer(ContentType aType, const nsIntRect& aRect, uint32_t aFlags,
                            RefPtr<gfx::DrawTarget>* aBlackDT, RefPtr<gfx::DrawTarget>* aWhiteDT) MOZ_OVERRIDE;

  virtual TextureInfo GetTextureInfo() const MOZ_OVERRIDE
  {
    return mTextureInfo;
  }

protected:
  void DestroyBuffers();

  virtual nsIntRegion GetUpdatedRegion(const nsIntRegion& aRegionToDraw,
                                       const nsIntRegion& aVisibleRegion,
                                       bool aDidSelfCopy);

  void BuildTextureClients(gfx::SurfaceFormat aFormat,
                           const nsIntRect& aRect,
                           uint32_t aFlags);

  // Create the front buffer for the ContentClient/Host pair if necessary
  // and notify the compositor that we have created the buffer(s).
  virtual void CreateFrontBuffer(const nsIntRect& aBufferRect) = 0;
  virtual void DestroyFrontBuffer() {}

  bool CreateAndAllocateTextureClient(RefPtr<TextureClient>& aClient,
                                      TextureFlags aFlags = 0);

  virtual void AbortTextureClientCreation()
  {
    mTextureClient = nullptr;
    mTextureClientOnWhite = nullptr;
    mIsNewBuffer = false;
  }

  RefPtr<TextureClient> mTextureClient;
  RefPtr<TextureClient> mTextureClientOnWhite;
  // keep a record of texture clients we have created and need to keep around
  // (for RotatedBuffer to access), then unlock and remove them when we are done
  // painting.
  nsTArray<RefPtr<TextureClient> > mOldTextures;

  TextureInfo mTextureInfo;
  bool mIsNewBuffer;
  bool mFrontAndBackBufferDiffer;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mSurfaceFormat;
};

/**
 * A double buffered ContentClient. mTextureClient is the back buffer, which
 * we draw into. mFrontClient is the front buffer which we may read from, but
 * not write to, when the compositor does not have the 'soft' lock. We can write
 * into mTextureClient at any time.
 *
 * The ContentHost keeps a reference to both corresponding texture hosts, in
 * response to our UpdateTextureRegion message, the compositor swaps its
 * references. In response to the compositor's reply we swap our references
 * (in SwapBuffers).
 */
class ContentClientDoubleBuffered : public ContentClientRemoteBuffer
{
public:
  ContentClientDoubleBuffered(CompositableForwarder* aFwd)
    : ContentClientRemoteBuffer(aFwd)
  {
    mTextureInfo.mCompositableType = COMPOSITABLE_CONTENT_DOUBLE;
  }
  virtual ~ContentClientDoubleBuffered() {}

  virtual void Clear() MOZ_OVERRIDE
  {
    ContentClientRemoteBuffer::Clear();
    mFrontClient = nullptr;
    mFrontClientOnWhite = nullptr;
  }

  virtual void SwapBuffers(const nsIntRegion& aFrontUpdatedRegion) MOZ_OVERRIDE;

  virtual void PrepareFrame() MOZ_OVERRIDE;

  virtual void FinalizeFrame(const nsIntRegion& aRegionToDraw) MOZ_OVERRIDE;

protected:
  virtual void CreateFrontBuffer(const nsIntRect& aBufferRect) MOZ_OVERRIDE;
  virtual void DestroyFrontBuffer() MOZ_OVERRIDE;

private:
  void UpdateDestinationFrom(const RotatedBuffer& aSource,
                             const nsIntRegion& aUpdateRegion);

  virtual void AbortTextureClientCreation() MOZ_OVERRIDE
  {
    mTextureClient = nullptr;
    mTextureClientOnWhite = nullptr;
    mFrontClient = nullptr;
    mFrontClientOnWhite = nullptr;
  }

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
 * TextureHost's memory directly, it must upload it to video memory of some
 * kind. We are free to modify the TextureClient once we receive reply from
 * the compositor.
 */
class ContentClientSingleBuffered : public ContentClientRemoteBuffer
{
public:
  ContentClientSingleBuffered(CompositableForwarder* aFwd)
    : ContentClientRemoteBuffer(aFwd)
  {
    mTextureInfo.mCompositableType = COMPOSITABLE_CONTENT_SINGLE;
  }
  virtual ~ContentClientSingleBuffered() {}

  virtual void PrepareFrame() MOZ_OVERRIDE;

protected:
  virtual void CreateFrontBuffer(const nsIntRect& aBufferRect) MOZ_OVERRIDE {}
};

/**
 * A single buffered ContentClient that creates temporary buffers which are
 * used to update the host-side texture. The ownership of the buffers is
 * passed to the host side during the transaction, and we need to create
 * new ones each frame.
 */
class ContentClientIncremental : public ContentClientRemote
                               , public BorrowDrawTarget
{
public:
  ContentClientIncremental(CompositableForwarder* aFwd)
    : ContentClientRemote(aFwd)
    , mContentType(gfxContentType::COLOR_ALPHA)
    , mHasBuffer(false)
    , mHasBufferOnWhite(false)
  {
    mTextureInfo.mCompositableType = BUFFER_CONTENT_INC;
  }

  typedef RotatedContentBuffer::PaintState PaintState;
  typedef RotatedContentBuffer::ContentType ContentType;

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

  virtual PaintState BeginPaintBuffer(ThebesLayer* aLayer,
                                      uint32_t aFlags) MOZ_OVERRIDE;
  virtual gfx::DrawTarget* BorrowDrawTargetForPainting(ThebesLayer* aLayer,
                                                       const PaintState& aPaintState) MOZ_OVERRIDE;
  virtual void ReturnDrawTargetToBuffer(gfx::DrawTarget*& aReturned) MOZ_OVERRIDE
  {
    BorrowDrawTarget::ReturnDrawTarget(aReturned);
  }

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
    mTextureInfo.mTextureFlags = aFlags & ~TEXTURE_DEALLOCATE_CLIENT;
    mContentType = aType;

    mForwarder->CreatedIncrementalBuffer(this,
                                         mTextureInfo,
                                         mBufferRect);

  }

  TemporaryRef<gfx::DrawTarget> GetUpdateSurface(BufferType aType,
                                                 const nsIntRegion& aUpdateRegion);

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
