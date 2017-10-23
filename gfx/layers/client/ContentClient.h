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
#include "mozilla/Maybe.h"              // for Maybe
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
class CapturedPaintState;

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
  typedef gfxContentType ContentType;

  /**
   * Creates, configures, and returns a new content client. If necessary, a
   * message will be sent to the compositor to create a corresponding content
   * host.
   */
  static already_AddRefed<ContentClient> CreateContentClient(CompositableForwarder* aFwd);

  /**
   * Controls the size of the backing buffer of this.
   * - SizedToVisibleBounds: the backing buffer is exactly the same
   *   size as the bounds of PaintedLayer's visible region
   * - ContainsVisibleBounds: the backing buffer is large enough to
   *   fit visible bounds.  May be larger.
   */
  enum BufferSizePolicy {
    SizedToVisibleBounds,
    ContainsVisibleBounds
  };

  explicit ContentClient(CompositableForwarder* aForwarder,
                         BufferSizePolicy aBufferSizePolicy)
  : CompositableClient(aForwarder)
  , mBufferSizePolicy(aBufferSizePolicy)
  , mInAsyncPaint(false)
  {}
  virtual ~ContentClient()
  {}

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  virtual void Clear();

  /**
   * This is returned by BeginPaint. The caller should draw into mTarget.
   * mRegionToDraw must be drawn. mRegionToInvalidate has been invalidated
   * by RotatedContentBuffer and must be redrawn on the screen.
   * mRegionToInvalidate is set when the buffer has changed from
   * opaque to transparent or vice versa, since the details of rendering can
   * depend on the buffer type.
   */
  struct PaintState {
    PaintState()
      : mRegionToDraw()
      , mRegionToInvalidate()
      , mMode(SurfaceMode::SURFACE_NONE)
      , mClip(DrawRegionClip::NONE)
      , mContentType(gfxContentType::SENTINEL)
    {}

    nsIntRegion mRegionToDraw;
    nsIntRegion mRegionToInvalidate;
    SurfaceMode mMode;
    DrawRegionClip mClip;
    gfxContentType mContentType;
  };

  enum {
    PAINT_WILL_RESAMPLE = 0x01,
    PAINT_NO_ROTATION = 0x02,
    PAINT_CAN_DRAW_ROTATED = 0x04
  };

  /**
   * Start a drawing operation. This returns a PaintState describing what
   * needs to be drawn to bring the buffer up to date in the visible region.
   * This queries aLayer to get the currently valid and visible regions.
   * The returned mTarget may be null if mRegionToDraw is empty.
   * Otherwise it must not be null.
   * mRegionToInvalidate will contain mRegionToDraw.
   * @param aFlags when PAINT_WILL_RESAMPLE is passed, this indicates that
   * buffer will be resampled when rendering (i.e the effective transform
   * combined with the scale for the resolution is not just an integer
   * translation). This will disable buffer rotation (since we don't want
   * to resample across the rotation boundary) and will ensure that we
   * make the entire buffer contents valid (since we don't want to sample
   * invalid pixels outside the visible region, if the visible region doesn't
   * fill the buffer bounds).
   * PAINT_CAN_DRAW_ROTATED can be passed if the caller supports drawing
   * rotated content that crosses the physical buffer boundary. The caller
   * will need to call BorrowDrawTargetForPainting multiple times to achieve
   * this.
   */
  PaintState BeginPaintBuffer(PaintedLayer* aLayer, uint32_t aFlags);

  /**
   * Fetch a DrawTarget for rendering. The DrawTarget remains owned by
   * this. See notes on BorrowDrawTargetForQuadrantUpdate.
   * May return null. If the return value is non-null, it must be
   * 'un-borrowed' using ReturnDrawTarget.
   *
   * If PAINT_CAN_DRAW_ROTATED was specified for BeginPaint, then the caller
   * must call this function repeatedly (with an iterator) until it returns
   * nullptr. The caller should draw the mDrawRegion of the iterator instead
   * of mRegionToDraw in the PaintState.
   *
   * @param aPaintState Paint state data returned by a call to BeginPaint
   * @param aIter Paint state iterator. Only required if PAINT_CAN_DRAW_ROTATED
   * was specified to BeginPaint.
   */
  virtual gfx::DrawTarget* BorrowDrawTargetForPainting(
    PaintState& aPaintState,
    RotatedBuffer::DrawIterator* aIter = nullptr);

  /**
   * Borrow a draw target for recording. The required transform for correct painting
   * is not applied to the returned DrawTarget by default, BUT it is
   * required to be whenever drawing does happen.
   */
  virtual RefPtr<CapturedPaintState> BorrowDrawTargetForRecording(
    PaintState& aPaintState,
    RotatedBuffer::DrawIterator* aIter,
    bool aSetTransform = false);

  virtual void ReturnDrawTargetToBuffer(gfx::DrawTarget*& aReturned);

  // Called as part of the layers transation reply. Conveys data about our
  // buffer(s) from the compositor. If appropriate we should swap references
  // to our buffers.
  virtual void SwapBuffers(const nsIntRegion& aFrontUpdatedRegion) {}

  // Call before and after painting into this content client
  virtual void BeginPaint() {}
  virtual void BeginAsyncPaint();
  virtual void EndPaint(nsTArray<ReadbackProcessor::Update>* aReadbackUpdates = nullptr);

  nsIntRegion ExpandDrawRegion(PaintState& aPaintState,
                               RotatedBuffer::DrawIterator* aIter,
                               gfx::BackendType aBackendType);

  static bool PrepareDrawTargetForPainting(CapturedPaintState*);

  enum {
    BUFFER_COMPONENT_ALPHA = 0x02 // Dual buffers should be created for drawing with
                                  // component alpha.
  };

protected:
  struct BufferDecision {
    nsIntRegion mNeededRegion;
    nsIntRegion mValidRegion;
    gfx::IntRect mBufferRect;
    SurfaceMode mBufferMode;
    gfxContentType mBufferContentType;
    bool mCanReuseBuffer;
    bool mCanKeepBufferContents;
  };

  BufferDecision CalculateBufferForPaint(PaintedLayer* aLayer,
                                         uint32_t aFlags);

  /**
   * Return the buffer's content type.  Requires a valid buffer or
   * buffer provider.
   */
  gfxContentType BufferContentType();
  bool BufferSizeOkFor(const gfx::IntSize& aSize);

  OpenMode LockMode() const;

  /**
   * Any actions that should be performed at the last moment before we begin
   * rendering the next frame. I.e., after we calculate what we will draw,
   * but before we rotate the buffer and possibly create new buffers.
   * aRegionToDraw is the region which is guaranteed to be overwritten when
   * drawing the next frame.
   */
  virtual void FinalizeFrame(const nsIntRegion& aRegionToDraw) {}

  virtual RefPtr<RotatedBuffer> CreateBuffer(gfxContentType aType,
                                             const gfx::IntRect& aRect,
                                             uint32_t aFlags) = 0;

  RefPtr<RotatedBuffer> mBuffer;
  BufferSizePolicy      mBufferSizePolicy;
  bool mInAsyncPaint;
};

// Thin wrapper around DrawTargetRotatedBuffer, for on-mtc
class ContentClientBasic final : public ContentClient
{
public:
  explicit ContentClientBasic(gfx::BackendType aBackend);

  virtual RefPtr<CapturedPaintState> BorrowDrawTargetForRecording(PaintState& aPaintState,
                                                                  RotatedBuffer::DrawIterator* aIter,
                                                                  bool aSetTransform) override;

  void DrawTo(PaintedLayer* aLayer,
              gfx::DrawTarget* aTarget,
              float aOpacity,
              gfx::CompositionOp aOp,
              gfx::SourceSurface* aMask,
              const gfx::Matrix* aMaskTransform);

  virtual TextureInfo GetTextureInfo() const override
  {
    MOZ_CRASH("GFX: Should not be called on non-remote ContentClient");
  }

protected:
  virtual RefPtr<RotatedBuffer> CreateBuffer(gfxContentType aType,
                                             const gfx::IntRect& aRect,
                                             uint32_t aFlags) override;

private:
  gfx::BackendType mBackend;
};

/**
 * A ContentClient backed by a RotatedContentBuffer.
 *
 * When using a ContentClientRemoteBuffer, SurfaceDescriptors are created on
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
class ContentClientRemoteBuffer : public ContentClient
{
public:
  explicit ContentClientRemoteBuffer(CompositableForwarder* aForwarder)
    : ContentClient(aForwarder, ContainsVisibleBounds)
    , mIsNewBuffer(false)
  {}

  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix="",
                    bool aDumpHtml=false,
                    TextureDumpMode aCompress=TextureDumpMode::Compress) override;

  virtual RefPtr<CapturedPaintState> BorrowDrawTargetForRecording(PaintState& aPaintState,
                                                                  RotatedBuffer::DrawIterator* aIter,
                                                                  bool aSetTransform) override;

  /**
   * Begin/End Paint map a gfxASurface from the texture client
   * into the buffer of RotatedBuffer. The surface is only
   * valid when the texture client is locked, so is mapped out
   * of RotatedContentBuffer when we are done painting.
   * None of the underlying buffer attributes (rect, rotation)
   * are affected by mapping/unmapping.
   */
  virtual void BeginPaint() override;
  virtual void BeginAsyncPaint() override;
  virtual void EndPaint(nsTArray<ReadbackProcessor::Update>* aReadbackUpdates = nullptr) override;

  virtual void Updated(const nsIntRegion& aRegionToDraw,
                       const nsIntRegion& aVisibleRegion);

  virtual TextureFlags ExtraTextureFlags() const
  {
    return TextureFlags::IMMEDIATE_UPLOAD;
  }

protected:
  virtual nsIntRegion GetUpdatedRegion(const nsIntRegion& aRegionToDraw,
                                       const nsIntRegion& aVisibleRegion);

  // Ensure we have a valid back buffer if we have a valid front buffer (i.e.
  // if a backbuffer has been created.)
  virtual void EnsureBackBufferIfFrontBuffer() {}

  virtual RefPtr<RotatedBuffer> CreateBuffer(gfxContentType aType,
                                             const gfx::IntRect& aRect,
                                             uint32_t aFlags) override;

  RefPtr<RotatedBuffer> CreateBufferInternal(const gfx::IntRect& aRect,
                                             gfx::SurfaceFormat aFormat,
                                             TextureFlags aFlags);

  RemoteRotatedBuffer* GetRemoteBuffer() const
  {
    return static_cast<RemoteRotatedBuffer*>(mBuffer.get());
  }

  bool mIsNewBuffer;
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
    , mFrontAndBackBufferDiffer(false)
  {}

  virtual ~ContentClientDoubleBuffered() {}

  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix="",
                    bool aDumpHtml=false,
                    TextureDumpMode aCompress=TextureDumpMode::Compress) override;

  virtual void Clear() override
  {
    ContentClient::Clear();
    mFrontBuffer = nullptr;
  }

  virtual void SwapBuffers(const nsIntRegion& aFrontUpdatedRegion) override;

  virtual void BeginPaint() override;
  virtual void BeginAsyncPaint() override;

  virtual void FinalizeFrame(const nsIntRegion& aRegionToDraw) override;

  virtual void EnsureBackBufferIfFrontBuffer() override;

  virtual TextureInfo GetTextureInfo() const override
  {
    return TextureInfo(CompositableType::CONTENT_DOUBLE, mTextureFlags);
  }

private:
  RefPtr<RemoteRotatedBuffer> mFrontBuffer;
  nsIntRegion mFrontUpdatedRegion;
  bool mFrontAndBackBufferDiffer;
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

  virtual TextureInfo GetTextureInfo() const override
  {
    return TextureInfo(CompositableType::CONTENT_SINGLE, mTextureFlags | ExtraTextureFlags());
  }
};

} // namespace layers
} // namespace mozilla

#endif
