/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef THEBESLAYERBUFFER_H_
#define THEBESLAYERBUFFER_H_

#include "gfxContext.h"
#include "gfxASurface.h"
#include "nsRegion.h"

namespace mozilla {
namespace layers {

class ThebesLayer;

/**
 * This class encapsulates the buffer used to retain ThebesLayer contents,
 * i.e., the contents of the layer's GetVisibleRegion().
 * 
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
class ThebesLayerBuffer {
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
    : mBufferDims(0,0)
    , mBufferRotation(0,0)
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
    mBuffer = nsnull;
    mBufferDims.SizeTo(0, 0);
    mBufferRect.Empty();
  }

  /**
   * This is returned by BeginPaint. The caller should draw into mContext.
   * mRegionToDraw must be drawn. mRegionToInvalidate has been invalidated
   * by ThebesLayerBuffer and must be redrawn on the screen.
   * mRegionToInvalidate is set when the buffer has changed from
   * opaque to transparent or vice versa, since the details of rendering can
   * depend on the buffer type.
   */
  struct PaintState {
    nsRefPtr<gfxContext> mContext;
    nsIntRegion mRegionToDraw;
    nsIntRegion mRegionToInvalidate;
  };

  /**
   * Start a drawing operation. This returns a PaintState describing what
   * needs to be drawn to bring the buffer up to date in the visible region.
   * This queries aLayer to get the currently valid and visible regions.
   * The returned mContext may be null if mRegionToDraw is empty.
   * Otherwise it must not be null.
   * mRegionToInvalidate will contain mRegionToDraw.
   */
  PaintState BeginPaint(ThebesLayer* aLayer, ContentType aContentType,
                        float aXResolution, float aYResolution);

  /**
   * Return a new surface of |aSize| and |aType|.
   */
  virtual already_AddRefed<gfxASurface>
  CreateBuffer(ContentType aType, const nsIntSize& aSize) = 0;

  /**
   * Get the underlying buffer, if any. This is useful because we can pass
   * in the buffer as the default "reference surface" if there is one.
   * Don't use it for anything else!
   */
  gfxASurface* GetBuffer() { return mBuffer; }

protected:
  enum XSide {
    LEFT, RIGHT
  };
  enum YSide {
    TOP, BOTTOM
  };
  nsIntRect GetQuadrantRectangle(XSide aXSide, YSide aYSide);
  void DrawBufferQuadrant(gfxContext* aTarget, XSide aXSide, YSide aYSide,
                          float aOpacity, float aXRes, float aYRes);
  void DrawBufferWithRotation(gfxContext* aTarget, float aOpacity,
                              float aXRes, float aYRes);

  /**
   * |BufferRect()| is the rect of device pixels that this
   * ThebesLayerBuffer covers.  That is what DrawBufferWithRotation()
   * will paint when it's called.
   *
   * |BufferDims()| is the actual dimensions of the underlying surface
   * maintained by this, also in device pixels.  It is *not*
   * necessarily true that |BufferRect().Size() == BufferDims()|.
   * They may differ if a ThebesLayer is drawn at a non-1.0
   * resolution.
   */
  const nsIntSize& BufferDims() const { return mBufferDims; }
  const nsIntRect& BufferRect() const { return mBufferRect; }
  const nsIntPoint& BufferRotation() const { return mBufferRotation; }

  already_AddRefed<gfxASurface>
  SetBuffer(gfxASurface* aBuffer, const nsIntSize& aBufferDims,
            const nsIntRect& aBufferRect, const nsIntPoint& aBufferRotation)
  {
    nsRefPtr<gfxASurface> tmp = mBuffer.forget();
    mBuffer = aBuffer;
    mBufferDims = aBufferDims;
    mBufferRect = aBufferRect;
    mBufferRotation = aBufferRotation;
    return tmp.forget();
  }

private:
  PRBool BufferSizeOkFor(const nsIntSize& aSize)
  {
    return (aSize == mBufferDims ||
            (SizedToVisibleBounds != mBufferSizePolicy &&
             aSize < mBufferDims));
  }

  nsRefPtr<gfxASurface> mBuffer;
  /**
   * The actual dimensions of mBuffer.  For the ContainsVisibleBounds
   * policy or with resolution-scaled drawing, mBufferDims might be
   * different than mBufferRect.Size().
   */
  nsIntSize             mBufferDims;
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
  BufferSizePolicy      mBufferSizePolicy;
};

}
}

#endif /* THEBESLAYERBUFFER_H_ */
