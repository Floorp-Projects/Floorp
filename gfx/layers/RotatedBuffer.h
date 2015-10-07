/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ROTATEDBUFFER_H_
#define ROTATEDBUFFER_H_

#include "gfxTypes.h"
#include <stdint.h>                     // for uint32_t
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr, already_AddRefed
#include "mozilla/gfx/2D.h"             // for DrawTarget, etc
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsRegion.h"                   // for nsIntRegion
#include "LayersTypes.h"

namespace mozilla {
namespace gfx {
class Matrix;
} // namespace gfx

namespace layers {

class TextureClient;
class PaintedLayer;

/**
 * This is a cairo/Thebes surface, but with a literal twist. Scrolling
 * causes the layer's visible region to move. We want to keep
 * reusing the same surface if the region size hasn't changed, but we don't
 * want to keep moving the contents of the surface around in memory. So
 * we use a trick.
 * Consider just the vertical case, and suppose the buffer is H pixels
 * high and we're scrolling down by N pixels. Instead of copying the
 * buffer contents up by N pixels, we leave the buffer contents in place,
 * and paint content rows H to H+N-1 into rows 0 to N-1 of the buffer.
 * Then we can refresh the screen by painting rows N to H-1 of the buffer
 * at row 0 on the screen, and then painting rows 0 to N-1 of the buffer
 * at row H-N on the screen.
 * mBufferRotation.y would be N in this example.
 */
class RotatedBuffer {
public:
  typedef gfxContentType ContentType;

  RotatedBuffer(const gfx::IntRect& aBufferRect,
                const gfx::IntPoint& aBufferRotation)
    : mBufferRect(aBufferRect)
    , mBufferRotation(aBufferRotation)
    , mDidSelfCopy(false)
  { }
  RotatedBuffer()
    : mDidSelfCopy(false)
  { }

  /*
   * Which buffer should be drawn to/read from.
   */
  enum ContextSource {
    BUFFER_BLACK, // The normal buffer, or buffer with black background when using component alpha.
    BUFFER_WHITE, // The buffer with white background, only valid with component alpha.
    BUFFER_BOTH // The combined black/white buffers, only valid for writing operations, not reading.
  };
  // It is the callers repsonsibility to ensure aTarget is flushed after calling
  // this method.
  void DrawBufferWithRotation(gfx::DrawTarget* aTarget, ContextSource aSource,
                              float aOpacity = 1.0,
                              gfx::CompositionOp aOperator = gfx::CompositionOp::OP_OVER,
                              gfx::SourceSurface* aMask = nullptr,
                              const gfx::Matrix* aMaskTransform = nullptr) const;

  /**
   * |BufferRect()| is the rect of device pixels that this
   * RotatedBuffer covers.  That is what DrawBufferWithRotation()
   * will paint when it's called.
   */
  const gfx::IntRect& BufferRect() const { return mBufferRect; }
  const gfx::IntPoint& BufferRotation() const { return mBufferRotation; }

  virtual bool HaveBuffer() const = 0;
  virtual bool HaveBufferOnWhite() const = 0;

  virtual already_AddRefed<gfx::SourceSurface> GetSourceSurface(ContextSource aSource) const = 0;

protected:

  enum XSide {
    LEFT, RIGHT
  };
  enum YSide {
    TOP, BOTTOM
  };
  gfx::IntRect GetQuadrantRectangle(XSide aXSide, YSide aYSide) const;

  gfx::Rect GetSourceRectangle(XSide aXSide, YSide aYSide) const;

  /*
   * If aMask is non-null, then it is used as an alpha mask for rendering this
   * buffer. aMaskTransform must be non-null if aMask is non-null, and is used
   * to adjust the coordinate space of the mask.
   */
  void DrawBufferQuadrant(gfx::DrawTarget* aTarget, XSide aXSide, YSide aYSide,
                          ContextSource aSource,
                          float aOpacity,
                          gfx::CompositionOp aOperator,
                          gfx::SourceSurface* aMask,
                          const gfx::Matrix* aMaskTransform) const;

  /** The area of the PaintedLayer that is covered by the buffer as a whole */
  gfx::IntRect             mBufferRect;
  /**
   * The x and y rotation of the buffer. Conceptually the buffer
   * has its origin translated to mBufferRect.TopLeft() - mBufferRotation,
   * is tiled to fill the plane, and the result is clipped to mBufferRect.
   * So the pixel at mBufferRotation within the buffer is what gets painted at
   * mBufferRect.TopLeft().
   * This is "rotation" in the sense of rotating items in a linear buffer,
   * where items falling off the end of the buffer are returned to the
   * buffer at the other end, not 2D rotation!
   */
  gfx::IntPoint            mBufferRotation;
  // When this is true it means that all pixels have moved inside the buffer.
  // It's not possible to sync with another buffer without a full copy.
  bool                  mDidSelfCopy;
};

class SourceRotatedBuffer : public RotatedBuffer
{
public:
  SourceRotatedBuffer(gfx::SourceSurface* aSource, gfx::SourceSurface* aSourceOnWhite,
                      const gfx::IntRect& aBufferRect,
                      const gfx::IntPoint& aBufferRotation)
    : RotatedBuffer(aBufferRect, aBufferRotation)
    , mSource(aSource)
    , mSourceOnWhite(aSourceOnWhite)
  { }

  virtual already_AddRefed<gfx::SourceSurface> GetSourceSurface(ContextSource aSource) const;

  virtual bool HaveBuffer() const { return !!mSource; }
  virtual bool HaveBufferOnWhite() const { return !!mSourceOnWhite; }

private:
  RefPtr<gfx::SourceSurface> mSource;
  RefPtr<gfx::SourceSurface> mSourceOnWhite;
};

// Mixin class for classes which need logic for loaning out a draw target.
// See comments on BorrowDrawTargetForQuadrantUpdate.
class BorrowDrawTarget
{
protected:
  void ReturnDrawTarget(gfx::DrawTarget*& aReturned);

  // The draw target loaned by BorrowDrawTargetForQuadrantUpdate. It should not
  // be used, we just keep a reference to ensure it is kept alive and so we can
  // correctly restore state when it is returned.
  RefPtr<gfx::DrawTarget> mLoanedDrawTarget;
  gfx::Matrix mLoanedTransform;
};

/**
 * This class encapsulates the buffer used to retain PaintedLayer contents,
 * i.e., the contents of the layer's GetVisibleRegion().
 */
class RotatedContentBuffer : public RotatedBuffer
                           , public BorrowDrawTarget
{
public:
  typedef gfxContentType ContentType;

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

  explicit RotatedContentBuffer(BufferSizePolicy aBufferSizePolicy)
    : mBufferProvider(nullptr)
    , mBufferProviderOnWhite(nullptr)
    , mBufferSizePolicy(aBufferSizePolicy)
  {
    MOZ_COUNT_CTOR(RotatedContentBuffer);
  }
  virtual ~RotatedContentBuffer()
  {
    MOZ_COUNT_DTOR(RotatedContentBuffer);
  }

  /**
   * Wipe out all retained contents. Call this when the entire
   * buffer becomes invalid.
   */
  void Clear()
  {
    mDTBuffer = nullptr;
    mDTBufferOnWhite = nullptr;
    mBufferProvider = nullptr;
    mBufferProviderOnWhite = nullptr;
    mBufferRect.SetEmpty();
  }

  /**
   * This is returned by BeginPaint. The caller should draw into mTarget.
   * mRegionToDraw must be drawn. mRegionToInvalidate has been invalidated
   * by RotatedContentBuffer and must be redrawn on the screen.
   * mRegionToInvalidate is set when the buffer has changed from
   * opaque to transparent or vice versa, since the details of rendering can
   * depend on the buffer type.  mDidSelfCopy is true if we kept our buffer
   * but used MovePixels() to shift its content.
   */
  struct PaintState {
    PaintState()
      : mRegionToDraw()
      , mRegionToInvalidate()
      , mMode(SurfaceMode::SURFACE_NONE)
      , mClip(DrawRegionClip::NONE)
      , mContentType(gfxContentType::SENTINEL)
      , mDidSelfCopy(false)
    {}

    nsIntRegion mRegionToDraw;
    nsIntRegion mRegionToInvalidate;
    SurfaceMode mMode;
    DrawRegionClip mClip;
    ContentType mContentType;
    bool mDidSelfCopy;
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
  PaintState BeginPaint(PaintedLayer* aLayer,
                        uint32_t aFlags);

  struct DrawIterator {
    friend class RotatedContentBuffer;
    DrawIterator()
      : mCount(0)
    {}

    nsIntRegion mDrawRegion;

  private:
    uint32_t mCount;
  };

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
  gfx::DrawTarget* BorrowDrawTargetForPainting(PaintState& aPaintState,
                                               DrawIterator* aIter = nullptr);

  enum {
    BUFFER_COMPONENT_ALPHA = 0x02 // Dual buffers should be created for drawing with
                                  // component alpha.
  };
  /**
   * Return a new surface of |aSize| and |aType|.
   *
   * If the created buffer supports azure content, then the result(s) will
   * be returned in aBlackDT/aWhiteDT, otherwise aBlackSurface/aWhiteSurface
   * will be used.
   */
  virtual void
  CreateBuffer(ContentType aType, const gfx::IntRect& aRect, uint32_t aFlags,
               RefPtr<gfx::DrawTarget>* aBlackDT, RefPtr<gfx::DrawTarget>* aWhiteDT) = 0;

  /**
   * Get the underlying buffer, if any. This is useful because we can pass
   * in the buffer as the default "reference surface" if there is one.
   * Don't use it for anything else!
   */
  gfx::DrawTarget* GetDTBuffer() { return mDTBuffer; }
  gfx::DrawTarget* GetDTBufferOnWhite() { return mDTBufferOnWhite; }

  virtual already_AddRefed<gfx::SourceSurface> GetSourceSurface(ContextSource aSource) const;

  /**
   * Complete the drawing operation. The region to draw must have been
   * drawn before this is called. The contents of the buffer are drawn
   * to aTarget.
   */
  void DrawTo(PaintedLayer* aLayer,
              gfx::DrawTarget* aTarget,
              float aOpacity,
              gfx::CompositionOp aOp,
              gfx::SourceSurface* aMask,
              const gfx::Matrix* aMaskTransform);

protected:
  // new texture client versions
  void SetBufferProvider(TextureClient* aClient)
  {
    // Only this buffer provider can give us a buffer.  If we
    // already have one, something has gone wrong.
    MOZ_ASSERT(!aClient || !mDTBuffer);

    mBufferProvider = aClient;
    if (!mBufferProvider) {
      mDTBuffer = nullptr;
    }
  }

  void SetBufferProviderOnWhite(TextureClient* aClient)
  {
    // Only this buffer provider can give us a buffer.  If we
    // already have one, something has gone wrong.
    MOZ_ASSERT(!aClient || !mDTBufferOnWhite);

    mBufferProviderOnWhite = aClient;
    if (!mBufferProviderOnWhite) {
      mDTBufferOnWhite = nullptr;
    }
  }

  /**
   * Get a draw target at the specified resolution for updating |aBounds|,
   * which must be contained within a single quadrant.
   *
   * The result should only be held temporarily by the caller (it will be kept
   * alive by this). Once used it should be returned using ReturnDrawTarget.
   * BorrowDrawTargetForQuadrantUpdate may not be called more than once without
   * first calling ReturnDrawTarget.
   *
   * ReturnDrawTarget will restore the transform on the draw target. But it is
   * the callers responsibility to restore the clip. The caller should flush the
   * draw target, if necessary.
   */
  gfx::DrawTarget*
  BorrowDrawTargetForQuadrantUpdate(const gfx::IntRect& aBounds,
                                    ContextSource aSource,
                                    DrawIterator* aIter);

  static bool IsClippingCheap(gfx::DrawTarget* aTarget, const nsIntRegion& aRegion);

protected:
  /**
   * Return the buffer's content type.  Requires a valid buffer or
   * buffer provider.
   */
  gfxContentType BufferContentType();
  bool BufferSizeOkFor(const gfx::IntSize& aSize);
  /**
   * If the buffer hasn't been mapped, map it.
   */
  bool EnsureBuffer();
  bool EnsureBufferOnWhite();

  // Flush our buffers if they are mapped.
  void FlushBuffers();

  /**
   * True if we have a buffer where we can get it (but not necessarily
   * mapped currently).
   */
  virtual bool HaveBuffer() const;
  virtual bool HaveBufferOnWhite() const;

  /**
   * Any actions that should be performed at the last moment before we begin
   * rendering the next frame. I.e., after we calculate what we will draw,
   * but before we rotate the buffer and possibly create new buffers.
   * aRegionToDraw is the region which is guaranteed to be overwritten when
   * drawing the next frame.
   */
  virtual void FinalizeFrame(const nsIntRegion& aRegionToDraw) {}

  RefPtr<gfx::DrawTarget> mDTBuffer;
  RefPtr<gfx::DrawTarget> mDTBufferOnWhite;

  /**
   * These members are only set transiently.  They're used to map mDTBuffer
   * when we're using surfaces that require explicit map/unmap. Only one
   * may be used at a time.
   */
  TextureClient* mBufferProvider;
  TextureClient* mBufferProviderOnWhite;

  BufferSizePolicy      mBufferSizePolicy;
};

} // namespace layers
} // namespace mozilla

#endif /* ROTATEDBUFFER_H_ */
