/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ROTATEDBUFFER_H_
#define ROTATEDBUFFER_H_

#include <stdint.h>                     // for uint32_t
#include "gfxASurface.h"                // for gfxASurface, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr, TemporaryRef
#include "mozilla/gfx/2D.h"             // for DrawTarget, etc
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "LayersTypes.h"

struct nsIntSize;

namespace mozilla {
namespace gfx {
class Matrix;
}

namespace layers {

class DeprecatedTextureClient;
class TextureClient;
class ThebesLayer;

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

  RotatedBuffer(gfx::DrawTarget* aDTBuffer, gfx::DrawTarget* aDTBufferOnWhite,
                const nsIntRect& aBufferRect,
                const nsIntPoint& aBufferRotation)
    : mDTBuffer(aDTBuffer)
    , mDTBufferOnWhite(aDTBufferOnWhite)
    , mBufferRect(aBufferRect)
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
  const nsIntRect& BufferRect() const { return mBufferRect; }
  const nsIntPoint& BufferRotation() const { return mBufferRotation; }

  virtual bool HaveBuffer() const { return mDTBuffer; }
  virtual bool HaveBufferOnWhite() const { return mDTBufferOnWhite; }

protected:

  enum XSide {
    LEFT, RIGHT
  };
  enum YSide {
    TOP, BOTTOM
  };
  nsIntRect GetQuadrantRectangle(XSide aXSide, YSide aYSide) const;

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

  RefPtr<gfx::DrawTarget> mDTBuffer;
  RefPtr<gfx::DrawTarget> mDTBufferOnWhite;
  /** The area of the ThebesLayer that is covered by the buffer as a whole */
  nsIntRect             mBufferRect;
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
  nsIntPoint            mBufferRotation;
  // When this is true it means that all pixels have moved inside the buffer.
  // It's not possible to sync with another buffer without a full copy.
  bool                  mDidSelfCopy;
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
 * This class encapsulates the buffer used to retain ThebesLayer contents,
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
   *   size as the bounds of ThebesLayer's visible region
   * - ContainsVisibleBounds: the backing buffer is large enough to
   *   fit visible bounds.  May be larger.
   */
  enum BufferSizePolicy {
    SizedToVisibleBounds,
    ContainsVisibleBounds
  };

  RotatedContentBuffer(BufferSizePolicy aBufferSizePolicy)
    : mDeprecatedBufferProvider(nullptr)
    , mDeprecatedBufferProviderOnWhite(nullptr)
    , mBufferProvider(nullptr)
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
    mDeprecatedBufferProvider = nullptr;
    mDeprecatedBufferProviderOnWhite = nullptr;
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
      : mMode(SurfaceMode::SURFACE_NONE)
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
    PAINT_NO_ROTATION = 0x02
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
   */
  PaintState BeginPaint(ThebesLayer* aLayer,
                        uint32_t aFlags);

  /**
   * Fetch a DrawTarget for rendering. The DrawTarget remains owned by
   * this. See notes on BorrowDrawTargetForQuadrantUpdate.
   * May return null. If the return value is non-null, it must be
   * 'un-borrowed' using ReturnDrawTarget.
   */
  gfx::DrawTarget* BorrowDrawTargetForPainting(ThebesLayer* aLayer,
                                               const PaintState& aPaintState);

  enum {
    ALLOW_REPEAT = 0x01,
    BUFFER_COMPONENT_ALPHA = 0x02 // Dual buffers should be created for drawing with
                                  // component alpha.
  };
  /**
   * Return a new surface of |aSize| and |aType|.
   * @param aFlags if ALLOW_REPEAT is set, then the buffer should be configured
   * to allow repeat-mode, otherwise it should be in pad (clamp) mode
   * If the created buffer supports azure content, then the result(s) will
   * be returned in aBlackDT/aWhiteDT, otherwise aBlackSurface/aWhiteSurface
   * will be used.
   */
  virtual void
  CreateBuffer(ContentType aType, const nsIntRect& aRect, uint32_t aFlags,
               RefPtr<gfx::DrawTarget>* aBlackDT, RefPtr<gfx::DrawTarget>* aWhiteDT) = 0;

  /**
   * Get the underlying buffer, if any. This is useful because we can pass
   * in the buffer as the default "reference surface" if there is one.
   * Don't use it for anything else!
   */
  gfx::DrawTarget* GetDTBuffer() { return mDTBuffer; }
  gfx::DrawTarget* GetDTBufferOnWhite() { return mDTBufferOnWhite; }

  /**
   * Complete the drawing operation. The region to draw must have been
   * drawn before this is called. The contents of the buffer are drawn
   * to aTarget.
   */
  void DrawTo(ThebesLayer* aLayer,
              gfx::DrawTarget* aTarget,
              float aOpacity,
              gfx::CompositionOp aOp,
              gfxASurface* aMask,
              const gfx::Matrix* aMaskTransform);

protected:
  TemporaryRef<gfx::DrawTarget>
  SetDTBuffer(gfx::DrawTarget* aBuffer,
              const nsIntRect& aBufferRect,
              const nsIntPoint& aBufferRotation)
  {
    RefPtr<gfx::DrawTarget> tmp = mDTBuffer.forget();
    mDTBuffer = aBuffer;
    mBufferRect = aBufferRect;
    mBufferRotation = aBufferRotation;
    return tmp.forget();
  }

  TemporaryRef<gfx::DrawTarget>
  SetDTBufferOnWhite(gfx::DrawTarget* aBuffer)
  {
    RefPtr<gfx::DrawTarget> tmp = mDTBufferOnWhite.forget();
    mDTBufferOnWhite = aBuffer;
    return tmp.forget();
  }

  /**
   * Set the texture client only.  This is used with surfaces that
   * require explicit lock/unlock, which |aClient| is used to do on
   * demand in this code.
   *
   * It's the caller's responsibility to ensure |aClient| is valid
   * for the duration of operations it requests of this
   * RotatedContentBuffer.  It's also the caller's responsibility to
   * unset the provider when inactive, by calling
   * SetBufferProvider(nullptr).
   */
  void SetDeprecatedBufferProvider(DeprecatedTextureClient* aClient)
  {
    // Only this buffer provider can give us a buffer.  If we
    // already have one, something has gone wrong.
    MOZ_ASSERT((!aClient || !mDTBuffer) && !mBufferProvider);

    mDeprecatedBufferProvider = aClient;
    if (!mDeprecatedBufferProvider) {
      mDTBuffer = nullptr;
    } 
  }
  
  void SetDeprecatedBufferProviderOnWhite(DeprecatedTextureClient* aClient)
  {
    // Only this buffer provider can give us a buffer.  If we
    // already have one, something has gone wrong.
    MOZ_ASSERT((!aClient || !mDTBufferOnWhite) && !mBufferProviderOnWhite);

    mDeprecatedBufferProviderOnWhite = aClient;
    if (!mDeprecatedBufferProviderOnWhite) {
      mDTBufferOnWhite = nullptr;
    }
  }

  // new texture client versions
  void SetBufferProvider(TextureClient* aClient)
  {
    // Only this buffer provider can give us a buffer.  If we
    // already have one, something has gone wrong.
    MOZ_ASSERT((!aClient || !mDTBuffer) && !mDeprecatedBufferProvider);

    mBufferProvider = aClient;
    if (!mBufferProvider) {
      mDTBuffer = nullptr;
    }
  }

  void SetBufferProviderOnWhite(TextureClient* aClient)
  {
    // Only this buffer provider can give us a buffer.  If we
    // already have one, something has gone wrong.
    MOZ_ASSERT((!aClient || !mDTBufferOnWhite) && !mDeprecatedBufferProviderOnWhite);

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
  BorrowDrawTargetForQuadrantUpdate(const nsIntRect& aBounds,
                                    ContextSource aSource);

  static bool IsClippingCheap(gfx::DrawTarget* aTarget, const nsIntRegion& aRegion);

protected:
  /**
   * Return the buffer's content type.  Requires a valid buffer or
   * buffer provider.
   */
  gfxContentType BufferContentType();
  bool BufferSizeOkFor(const nsIntSize& aSize);
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

  /**
   * These members are only set transiently.  They're used to map mDTBuffer
   * when we're using surfaces that require explicit map/unmap. Only one
   * may be used at a time.
   */
  DeprecatedTextureClient* mDeprecatedBufferProvider;
  DeprecatedTextureClient* mDeprecatedBufferProviderOnWhite;
  TextureClient* mBufferProvider;
  TextureClient* mBufferProviderOnWhite;

  BufferSizePolicy      mBufferSizePolicy;
};

}
}

#endif /* ROTATEDBUFFER_H_ */
