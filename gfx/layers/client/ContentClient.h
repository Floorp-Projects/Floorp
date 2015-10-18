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
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr, already_AddRefed
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/LayersTypes.h"  // for TextureDumpMode
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "mozilla/mozalloc.h"           // for operator delete
#include "ReadbackProcessor.h"          // For ReadbackProcessor::Update
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray

namespace mozilla {
namespace gfx {
class DrawTarget;
} // namespace gfx

namespace layers {

class PaintedLayer;

/**
 * A compositable client for PaintedLayers. These are different to Image/Canvas
 * clients due to sending a valid region across IPC and because we do a lot more
 * optimisation work, encapsualted in RotatedContentBuffers.
 *
 * We use content clients for OMTC and non-OMTC, basic rendering so that
 * BasicPaintedLayer has only one interface to deal with. We support single and
 * double buffered flavours. For tiled layers, we do not use a ContentClient
 * although we do have a ContentHost, and we do use texture clients and texture
 * hosts.
 *
 * The interface presented by ContentClient is used by the BasicPaintedLayer
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
 * call BeginPaintBuffer on the content client. That will initialise the buffer
 * for painting, by calling RotatedContentBuffer::BeginPaint (usually) which
 * will call back to ContentClient::FinalizeFrame to finalize update of the
 * buffers before drawing (i.e., it finalizes the previous frame). Then call
 * BorrowDrawTargetForPainting to get a DrawTarget to paint into. Then paint.
 * Then return that DrawTarget using ReturnDrawTarget.
 * Call EndPaint on the content client;
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
  static already_AddRefed<ContentClient> CreateContentClient(CompositableForwarder* aFwd);

  explicit ContentClient(CompositableForwarder* aForwarder)
  : CompositableClient(aForwarder)
  {}
  virtual ~ContentClient()
  {}

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  virtual void Clear() = 0;
  virtual RotatedContentBuffer::PaintState BeginPaintBuffer(PaintedLayer* aLayer,
                                                            uint32_t aFlags) = 0;
  virtual gfx::DrawTarget* BorrowDrawTargetForPainting(RotatedContentBuffer::PaintState& aPaintState,
                                                       RotatedContentBuffer::DrawIterator* aIter = nullptr) = 0;
  virtual void ReturnDrawTargetToBuffer(gfx::DrawTarget*& aReturned) = 0;

  // Called as part of the layers transation reply. Conveys data about our
  // buffer(s) from the compositor. If appropriate we should swap references
  // to our buffers.
  virtual void SwapBuffers(const nsIntRegion& aFrontUpdatedRegion) {}

  // call before and after painting into this content client
  virtual void BeginPaint() {}
  virtual void EndPaint(nsTArray<ReadbackProcessor::Update>* aReadbackUpdates = nullptr);
};

/**
 * A ContentClient for use with OMTC.
 */
class ContentClientRemote : public ContentClient
{
public:
  explicit ContentClientRemote(CompositableForwarder* aForwarder)
    : ContentClient(aForwarder)
  {}

  virtual void Updated(const nsIntRegion& aRegionToDraw,
                       const nsIntRegion& aVisibleRegion,
                       bool aDidSelfCopy) = 0;
};

// thin wrapper around RotatedContentBuffer, for on-mtc
class ContentClientBasic final : public ContentClient
                               , protected RotatedContentBuffer
{
public:
  ContentClientBasic();

  typedef RotatedContentBuffer::PaintState PaintState;
  typedef RotatedContentBuffer::ContentType ContentType;

  virtual void Clear() override { RotatedContentBuffer::Clear(); }
  virtual PaintState BeginPaintBuffer(PaintedLayer* aLayer,
                                      uint32_t aFlags) override
  {
    return RotatedContentBuffer::BeginPaint(aLayer, aFlags);
  }
  virtual gfx::DrawTarget* BorrowDrawTargetForPainting(PaintState& aPaintState,
                                                       RotatedContentBuffer::DrawIterator* aIter = nullptr) override
  {
    return RotatedContentBuffer::BorrowDrawTargetForPainting(aPaintState, aIter);
  }
  virtual void ReturnDrawTargetToBuffer(gfx::DrawTarget*& aReturned) override
  {
    BorrowDrawTarget::ReturnDrawTarget(aReturned);
  }

  void DrawTo(PaintedLayer* aLayer,
              gfx::DrawTarget* aTarget,
              float aOpacity,
              gfx::CompositionOp aOp,
              gfx::SourceSurface* aMask,
              const gfx::Matrix* aMaskTransform)
  {
    RotatedContentBuffer::DrawTo(aLayer, aTarget, aOpacity, aOp,
                                 aMask, aMaskTransform);
  }

  virtual void CreateBuffer(ContentType aType, const gfx::IntRect& aRect, uint32_t aFlags,
                            RefPtr<gfx::DrawTarget>* aBlackDT, RefPtr<gfx::DrawTarget>* aWhiteDT) override;

  virtual TextureInfo GetTextureInfo() const override
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
  explicit ContentClientRemoteBuffer(CompositableForwarder* aForwarder)
    : ContentClientRemote(aForwarder)
    , RotatedContentBuffer(ContainsVisibleBounds)
    , mIsNewBuffer(false)
    , mFrontAndBackBufferDiffer(false)
    , mSurfaceFormat(gfx::SurfaceFormat::B8G8R8A8)
  {}

  typedef RotatedContentBuffer::PaintState PaintState;
  typedef RotatedContentBuffer::ContentType ContentType;

  virtual void Clear() override
  {
    RotatedContentBuffer::Clear();
    mTextureClient = nullptr;
    mTextureClientOnWhite = nullptr;
  }

  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix="",
                    bool aDumpHtml=false,
                    TextureDumpMode aCompress=TextureDumpMode::Compress) override;

  virtual PaintState BeginPaintBuffer(PaintedLayer* aLayer,
                                      uint32_t aFlags) override
  {
    return RotatedContentBuffer::BeginPaint(aLayer, aFlags);
  }
  virtual gfx::DrawTarget* BorrowDrawTargetForPainting(PaintState& aPaintState,
                                                       RotatedContentBuffer::DrawIterator* aIter = nullptr) override
  {
    return RotatedContentBuffer::BorrowDrawTargetForPainting(aPaintState, aIter);
  }
  virtual void ReturnDrawTargetToBuffer(gfx::DrawTarget*& aReturned) override
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
  virtual void BeginPaint() override;
  virtual void EndPaint(nsTArray<ReadbackProcessor::Update>* aReadbackUpdates = nullptr) override;

  virtual void Updated(const nsIntRegion& aRegionToDraw,
                       const nsIntRegion& aVisibleRegion,
                       bool aDidSelfCopy) override;

  virtual void SwapBuffers(const nsIntRegion& aFrontUpdatedRegion) override;

  // Expose these protected methods from the superclass.
  virtual const gfx::IntRect& BufferRect() const
  {
    return RotatedContentBuffer::BufferRect();
  }
  virtual const nsIntPoint& BufferRotation() const
  {
    return RotatedContentBuffer::BufferRotation();
  }

  virtual void CreateBuffer(ContentType aType, const gfx::IntRect& aRect, uint32_t aFlags,
                            RefPtr<gfx::DrawTarget>* aBlackDT, RefPtr<gfx::DrawTarget>* aWhiteDT) override;

  virtual TextureFlags ExtraTextureFlags() const
  {
    return TextureFlags::NO_FLAGS;
  }

protected:
  void DestroyBuffers();

  virtual nsIntRegion GetUpdatedRegion(const nsIntRegion& aRegionToDraw,
                                       const nsIntRegion& aVisibleRegion,
                                       bool aDidSelfCopy);

  void BuildTextureClients(gfx::SurfaceFormat aFormat,
                           const gfx::IntRect& aRect,
                           uint32_t aFlags);

  void CreateBackBuffer(const gfx::IntRect& aBufferRect);

  // Ensure we have a valid back buffer if we have a valid front buffer (i.e.
  // if a backbuffer has been created.)
  virtual void EnsureBackBufferIfFrontBuffer() {}

  // Create the front buffer for the ContentClient/Host pair if necessary
  // and notify the compositor that we have created the buffer(s).
  virtual void DestroyFrontBuffer() {}

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
  explicit ContentClientDoubleBuffered(CompositableForwarder* aFwd)
    : ContentClientRemoteBuffer(aFwd)
  {}

  virtual ~ContentClientDoubleBuffered() {}

  virtual void Clear() override
  {
    ContentClientRemoteBuffer::Clear();
    mFrontClient = nullptr;
    mFrontClientOnWhite = nullptr;
  }

  virtual void Updated(const nsIntRegion& aRegionToDraw,
                       const nsIntRegion& aVisibleRegion,
                       bool aDidSelfCopy) override;

  virtual void SwapBuffers(const nsIntRegion& aFrontUpdatedRegion) override;

  virtual void BeginPaint() override;

  virtual void FinalizeFrame(const nsIntRegion& aRegionToDraw) override;

  virtual void EnsureBackBufferIfFrontBuffer() override;

  virtual TextureInfo GetTextureInfo() const override
  {
    return TextureInfo(CompositableType::CONTENT_DOUBLE, mTextureFlags);
  }

  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix="",
                    bool aDumpHtml=false,
                    TextureDumpMode aCompress=TextureDumpMode::Compress) override;
protected:
  virtual void DestroyFrontBuffer() override;

private:
  void UpdateDestinationFrom(const RotatedBuffer& aSource,
                             const nsIntRegion& aUpdateRegion);

  virtual void AbortTextureClientCreation() override
  {
    mTextureClient = nullptr;
    mTextureClientOnWhite = nullptr;
    mFrontClient = nullptr;
    mFrontClientOnWhite = nullptr;
  }

  RefPtr<TextureClient> mFrontClient;
  RefPtr<TextureClient> mFrontClientOnWhite;
  nsIntRegion mFrontUpdatedRegion;
  gfx::IntRect mFrontBufferRect;
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
  explicit ContentClientSingleBuffered(CompositableForwarder* aFwd)
    : ContentClientRemoteBuffer(aFwd)
  {
  }
  virtual ~ContentClientSingleBuffered() {}

  virtual void FinalizeFrame(const nsIntRegion& aRegionToDraw) override;

  virtual TextureInfo GetTextureInfo() const override
  {
    return TextureInfo(CompositableType::CONTENT_SINGLE, mTextureFlags | ExtraTextureFlags());
  }

  virtual TextureFlags ExtraTextureFlags() const override
  {
    return TextureFlags::IMMEDIATE_UPLOAD;
  }
};

} // namespace layers
} // namespace mozilla

#endif
