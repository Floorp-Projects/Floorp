/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef THEBESLAYERBUFFER_H_
#define THEBESLAYERBUFFER_H_

#include <stdint.h>                     // for uint32_t
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxContext.h"                 // for gfxContext
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr, TemporaryRef
#include "mozilla/gfx/2D.h"             // for DrawTarget, etc
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsISupportsImpl.h"            // for gfxContext::AddRef, etc
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc

struct gfxMatrix;
struct nsIntSize;

namespace mozilla {
namespace gfx {
class Matrix;
}

namespace layers {

class DeprecatedTextureClient;
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
  typedef gfxASurface::gfxContentType ContentType;

  RotatedBuffer(gfxASurface* aBuffer, gfxASurface* aBufferOnWhite,
                const nsIntRect& aBufferRect,
                const nsIntPoint& aBufferRotation)
    : mBuffer(aBuffer)
    , mBufferOnWhite(aBufferOnWhite)
    , mBufferRect(aBufferRect)
    , mBufferRotation(aBufferRotation)
  { }
  RotatedBuffer(gfx::DrawTarget* aDTBuffer, gfx::DrawTarget* aDTBufferOnWhite,
                const nsIntRect& aBufferRect,
                const nsIntPoint& aBufferRotation)
    : mDTBuffer(aDTBuffer)
    , mDTBufferOnWhite(aDTBufferOnWhite)
    , mBufferRect(aBufferRect)
    , mBufferRotation(aBufferRotation)
  { }
  RotatedBuffer() { }

  /*
   * Which buffer should be drawn to/read from.
   */
  enum ContextSource {
    BUFFER_BLACK, // The normal buffer, or buffer with black background when using component alpha.
    BUFFER_WHITE, // The buffer with white background, only valid with component alpha.
    BUFFER_BOTH // The combined black/white buffers, only valid for writing operations, not reading.
  };
  void DrawBufferWithRotation(gfxContext* aTarget, ContextSource aSource,
                              float aOpacity = 1.0,
                              gfxASurface* aMask = nullptr,
                              const gfxMatrix* aMaskTransform = nullptr) const;

  void DrawBufferWithRotation(gfx::DrawTarget* aTarget, ContextSource aSource,
                              float aOpacity = 1.0,
                              gfx::SourceSurface* aMask = nullptr,
                              const gfx::Matrix* aMaskTransform = nullptr) const;

  /**
   * |BufferRect()| is the rect of device pixels that this
   * ThebesLayerBuffer covers.  That is what DrawBufferWithRotation()
   * will paint when it's called.
   */
  const nsIntRect& BufferRect() const { return mBufferRect; }
  const nsIntPoint& BufferRotation() const { return mBufferRotation; }

  virtual bool HaveBuffer() const { return mBuffer || mDTBuffer; }
  virtual bool HaveBufferOnWhite() const { return mBufferOnWhite || mDTBufferOnWhite; }

protected:

  enum XSide {
    LEFT, RIGHT
  };
  enum YSide {
    TOP, BOTTOM
  };
  nsIntRect GetQuadrantRectangle(XSide aXSide, YSide aYSide) const;

  /*
   * If aMask is non-null, then it is used as an alpha mask for rendering this
   * buffer. aMaskTransform must be non-null if aMask is non-null, and is used
   * to adjust the coordinate space of the mask.
   */
  void DrawBufferQuadrant(gfxContext* aTarget, XSide aXSide, YSide aYSide,
                          ContextSource aSource,
                          float aOpacity,
                          gfxASurface* aMask,
                          const gfxMatrix* aMaskTransform) const;
  void DrawBufferQuadrant(gfx::DrawTarget* aTarget, XSide aXSide, YSide aYSide,
                          ContextSource aSource,
                          float aOpacity,
                          gfx::SourceSurface* aMask,
                          const gfx::Matrix* aMaskTransform) const;

  nsRefPtr<gfxASurface> mBuffer;
  nsRefPtr<gfxASurface> mBufferOnWhite;
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
};

/**
 * This class encapsulates the buffer used to retain ThebesLayer contents,
 * i.e., the contents of the layer's GetVisibleRegion().
 */
class ThebesLayerBuffer : public RotatedBuffer {
public:
  typedef gfxASurface::gfxContentType ContentType;

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

  ThebesLayerBuffer(BufferSizePolicy aBufferSizePolicy)
    : mBufferProvider(nullptr)
    , mBufferProviderOnWhite(nullptr)
    , mBufferSizePolicy(aBufferSizePolicy)
  {
    MOZ_COUNT_CTOR(ThebesLayerBuffer);
  }
  virtual ~ThebesLayerBuffer()
  {
    MOZ_COUNT_DTOR(ThebesLayerBuffer);
  }

  /**
   * Wipe out all retained contents. Call this when the entire
   * buffer becomes invalid.
   */
  void Clear()
  {
    mBuffer = nullptr;
    mBufferOnWhite = nullptr;
    mDTBuffer = nullptr;
    mDTBufferOnWhite = nullptr;
    mBufferProvider = nullptr;
    mBufferProviderOnWhite = nullptr;
    mBufferRect.SetEmpty();
  }

  /**
   * This is returned by BeginPaint. The caller should draw into mContext.
   * mRegionToDraw must be drawn. mRegionToInvalidate has been invalidated
   * by ThebesLayerBuffer and must be redrawn on the screen.
   * mRegionToInvalidate is set when the buffer has changed from
   * opaque to transparent or vice versa, since the details of rendering can
   * depend on the buffer type.  mDidSelfCopy is true if we kept our buffer
   * but used MovePixels() to shift its content.
   */
  struct PaintState {
    PaintState()
      : mDidSelfCopy(false)
    {}

    nsRefPtr<gfxContext> mContext;
    nsIntRegion mRegionToDraw;
    nsIntRegion mRegionToInvalidate;
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
   * The returned mContext may be null if mRegionToDraw is empty.
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
  PaintState BeginPaint(ThebesLayer* aLayer, ContentType aContentType,
                        uint32_t aFlags);

  enum {
    ALLOW_REPEAT = 0x01,
    BUFFER_COMPONENT_ALPHA = 0x02 // Dual buffers should be created for drawing with
                                  // component alpha.
  };
  /**
   * Return a new surface of |aSize| and |aType|.
   * @param aFlags if ALLOW_REPEAT is set, then the buffer should be configured
   * to allow repeat-mode, otherwise it should be in pad (clamp) mode
   */
  virtual already_AddRefed<gfxASurface>
  CreateBuffer(ContentType aType, const nsIntRect& aRect, uint32_t aFlags, gfxASurface** aWhiteSurface) = 0;
  virtual TemporaryRef<gfx::DrawTarget>
  CreateDTBuffer(ContentType aType, const nsIntRect& aRect, uint32_t aFlags, RefPtr<gfx::DrawTarget>* aWhiteDT)
  { NS_RUNTIMEABORT("CreateDTBuffer not implemented on this platform!"); return nullptr; }
  virtual bool SupportsAzureContent() const 
  { return false; }

  /**
   * Get the underlying buffer, if any. This is useful because we can pass
   * in the buffer as the default "reference surface" if there is one.
   * Don't use it for anything else!
   */
  gfxASurface* GetBuffer() { return mBuffer; }
  gfxASurface* GetBufferOnWhite() { return mBufferOnWhite; }
  gfx::DrawTarget* GetDTBuffer() { return mDTBuffer; }
  gfx::DrawTarget* GetDTBufferOnWhite() { return mDTBufferOnWhite; }

  /**
   * Complete the drawing operation. The region to draw must have been
   * drawn before this is called. The contents of the buffer are drawn
   * to aTarget.
   */
  void DrawTo(ThebesLayer* aLayer, gfxContext* aTarget, float aOpacity,
              gfxASurface* aMask, const gfxMatrix* aMaskTransform);

protected:
  // If this buffer is currently using Azure.
  bool IsAzureBuffer();

  already_AddRefed<gfxASurface>
  SetBuffer(gfxASurface* aBuffer,
            const nsIntRect& aBufferRect, const nsIntPoint& aBufferRotation)
  {
    MOZ_ASSERT(!SupportsAzureContent());
    nsRefPtr<gfxASurface> tmp = mBuffer.forget();
    mBuffer = aBuffer;
    mBufferRect = aBufferRect;
    mBufferRotation = aBufferRotation;
    return tmp.forget();
  }

  already_AddRefed<gfxASurface>
  SetBufferOnWhite(gfxASurface* aBuffer)
  {
    MOZ_ASSERT(!SupportsAzureContent());
    nsRefPtr<gfxASurface> tmp = mBufferOnWhite.forget();
    mBufferOnWhite = aBuffer;
    return tmp.forget();
  }

  TemporaryRef<gfx::DrawTarget>
  SetDTBuffer(gfx::DrawTarget* aBuffer,
            const nsIntRect& aBufferRect, const nsIntPoint& aBufferRotation)
  {
    MOZ_ASSERT(SupportsAzureContent());
    RefPtr<gfx::DrawTarget> tmp = mDTBuffer.forget();
    mDTBuffer = aBuffer;
    mBufferRect = aBufferRect;
    mBufferRotation = aBufferRotation;
    return tmp.forget();
  }

  TemporaryRef<gfx::DrawTarget>
  SetDTBufferOnWhite(gfx::DrawTarget* aBuffer)
  {
    MOZ_ASSERT(SupportsAzureContent());
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
   * ThebesLayerBuffer.  It's also the caller's responsibility to
   * unset the provider when inactive, by calling
   * SetBufferProvider(nullptr).
   */
  void SetBufferProvider(DeprecatedTextureClient* aClient)
  {
    // Only this buffer provider can give us a buffer.  If we
    // already have one, something has gone wrong.
    MOZ_ASSERT(!aClient || (!mBuffer && !mDTBuffer));

    mBufferProvider = aClient;
    if (!mBufferProvider) {
      mBuffer = nullptr;
      mDTBuffer = nullptr;
    } 
  }
  
  void SetBufferProviderOnWhite(DeprecatedTextureClient* aClient)
  {
    // Only this buffer provider can give us a buffer.  If we
    // already have one, something has gone wrong.
    MOZ_ASSERT(!aClient || (!mBufferOnWhite && !mDTBufferOnWhite));

    mBufferProviderOnWhite = aClient;
    if (!mBufferProviderOnWhite) {
      mBufferOnWhite = nullptr;
      mDTBufferOnWhite = nullptr;
    } 
  }

  /**
   * Get a context at the specified resolution for updating |aBounds|,
   * which must be contained within a single quadrant.
   *
   * Optionally returns the TopLeft coordinate of the quadrant being drawn to.
   */
  already_AddRefed<gfxContext>
  GetContextForQuadrantUpdate(const nsIntRect& aBounds, ContextSource aSource, nsIntPoint* aTopLeft = nullptr);

  static bool IsClippingCheap(gfxContext* aTarget, const nsIntRegion& aRegion);

protected:
  // Buffer helpers.  Don't use mBuffer directly; instead use one of
  // these helpers.

  /**
   * Return the buffer's content type.  Requires a valid buffer or
   * buffer provider.
   */
  gfxASurface::gfxContentType BufferContentType();
  bool BufferSizeOkFor(const nsIntSize& aSize);
  /**
   * If the buffer hasn't been mapped, map it.
   */
  void EnsureBuffer();
  void EnsureBufferOnWhite();
  /**
   * True if we have a buffer where we can get it (but not necessarily
   * mapped currently).
   */
  virtual bool HaveBuffer() const;
  virtual bool HaveBufferOnWhite() const;

  /**
   * These members are only set transiently.  They're used to map mBuffer
   * when we're using surfaces that require explicit map/unmap. Only one
   * may be used at a time.
   */
  DeprecatedTextureClient* mBufferProvider;
  DeprecatedTextureClient* mBufferProviderOnWhite;

  BufferSizePolicy      mBufferSizePolicy;
};

}
}

#endif /* THEBESLAYERBUFFER_H_ */
