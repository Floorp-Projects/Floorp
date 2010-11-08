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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#ifndef GFX_IMAGELAYER_H
#define GFX_IMAGELAYER_H

#include "Layers.h"

#include "gfxPattern.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

/**
 * A class representing a buffer of pixel data. The data can be in one
 * of various formats including YCbCr.
 * 
 * Create an image using an ImageContainer. Fill the image with data, and
 * then call ImageContainer::SetImage to display it. An image must not be
 * modified after calling SetImage. Image implementations do not need to
 * perform locking; when filling an Image, the Image client is responsible
 * for ensuring only one thread accesses the Image at a time, and after
 * SetImage the image is immutable.
 * 
 * When resampling an Image, only pixels within the buffer should be
 * sampled. For example, cairo images should be sampled in EXTEND_PAD mode.
 */
class THEBES_API Image {
  THEBES_INLINE_DECL_THREADSAFE_REFCOUNTING(Image)

public:
  virtual ~Image() {}

  enum Format {
    /**
     * The PLANAR_YCBCR format creates a PlanarYCbCrImage. All backends should
     * support this format, because the Ogg video decoder depends on it.
     * The maximum image width and height is 16384.
     */
    PLANAR_YCBCR,

    /**
     * The CAIRO_SURFACE format creates a CairoImage. All backends should
     * support this format, because video rendering sometimes requires it.
     * 
     * This format is useful even though a ThebesLayer could be used.
     * It makes it easy to render a cairo surface when another Image format
     * could be used. It can also avoid copying the surface data in some
     * cases.
     * 
     * Images in CAIRO_SURFACE format should only be created and
     * manipulated on the main thread, since the underlying cairo surface
     * is main-thread-only.
     */
    CAIRO_SURFACE
  };

  Format GetFormat() { return mFormat; }
  void* GetImplData() { return mImplData; }

protected:
  Image(void* aImplData, Format aFormat) :
    mImplData(aImplData),
    mFormat(aFormat)
  {}

  void* mImplData;
  Format mFormat;
};

/**
 * A class that manages Images for an ImageLayer. The only reason
 * we need a separate class here is that ImageLayers aren't threadsafe
 * (because layers can only be used on the main thread) and we want to
 * be able to set the current Image from any thread, to facilitate
 * video playback without involving the main thread, for example.
 */
class THEBES_API ImageContainer {
  THEBES_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageContainer)

public:
  virtual ~ImageContainer() {}

  /**
   * Create an Image in one of the given formats.
   * Picks the "best" format from the list and creates an Image of that
   * format.
   * Returns null if this backend does not support any of the formats.
   */
  virtual already_AddRefed<Image> CreateImage(const Image::Format* aFormats,
                                              PRUint32 aNumFormats) = 0;

  /**
   * Set an Image as the current image to display. The Image must have
   * been created by this ImageContainer.
   * 
   * The Image data must not be modified after this method is called!
   */
  virtual void SetCurrentImage(Image* aImage) = 0;

  /**
   * Get the current Image.
   * This has to add a reference since otherwise there are race conditions
   * where the current image is destroyed before the caller can add
   * a reference.
   */
  virtual already_AddRefed<Image> GetCurrentImage() = 0;

  /**
   * Get the current image as a gfxASurface. This is useful for fallback
   * rendering.
   * This can only be called from the main thread, since cairo objects
   * can only be used from the main thread.
   * This is defined here and not on Image because it's possible (likely)
   * that some backends will make an Image "ready to draw" only when it
   * becomes the current image for an image container.
   * Returns null if there is no current image.
   * Returns the size in aSize.
   * The returned surface will never be modified. The caller must not
   * modify it.
   */
  virtual already_AddRefed<gfxASurface> GetCurrentAsSurface(gfxIntSize* aSizeResult) = 0;

  /**
   * Returns the layer manager for this container. This can only
   * be used on the main thread, since layer managers should only be
   * accessed on the main thread.
   */
  LayerManager* Manager()
  {
    NS_PRECONDITION(NS_IsMainThread(), "Must be called on main thread");
    return mManager;
  }

  /**
   * Returns the size of the image in pixels.
   */
  virtual gfxIntSize GetCurrentSize() = 0;

  /**
   * Set a new layer manager for this image container.  It must be
   * either of the same type as the container's current layer manager,
   * or null.  TRUE is returned on success.
   */
  virtual PRBool SetLayerManager(LayerManager *aManager) = 0;

protected:
  LayerManager* mManager;

  ImageContainer(LayerManager* aManager) : mManager(aManager) {}
};

/**
 * A Layer which renders an Image.
 */
class THEBES_API ImageLayer : public Layer {
public:
  /**
   * CONSTRUCTION PHASE ONLY
   * Set the ImageContainer. aContainer must have the same layer manager
   * as this layer.
   */
  void SetContainer(ImageContainer* aContainer) { mContainer = aContainer; }
  /**
   * CONSTRUCTION PHASE ONLY
   * Set the filter used to resample this image if necessary.
   */
  void SetFilter(gfxPattern::GraphicsFilter aFilter) { mFilter = aFilter; }

  ImageContainer* GetContainer() { return mContainer; }
  gfxPattern::GraphicsFilter GetFilter() { return mFilter; }

  MOZ_LAYER_DECL_NAME("ImageLayer", TYPE_IMAGE)

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    // Snap image edges to pixel boundaries
    gfxRect snap(0, 0, 0, 0);
    if (mContainer) {
      gfxIntSize size = mContainer->GetCurrentSize();
      snap.size = gfxSize(size.width, size.height);
    }
    // Snap our local transform first, and snap the inherited transform as well.
    // This makes our snapping equivalent to what would happen if our content
    // was drawn into a ThebesLayer (gfxContext would snap using the local
    // transform, then we'd snap again when compositing the ThebesLayer).
    mEffectiveTransform =
        SnapTransform(GetLocalTransform(), snap, nsnull)*
        SnapTransform(aTransformToSurface, gfxRect(0, 0, 0, 0), nsnull);
  }

protected:
  ImageLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData), mFilter(gfxPattern::FILTER_GOOD) {}

  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix);

  nsRefPtr<ImageContainer> mContainer;
  gfxPattern::GraphicsFilter mFilter;
};

/****** Image subtypes for the different formats ******/

/**
 * We assume that the image data is in the REC 470M color space (see
 * Theora specification, section 4.3.1).
 *
 * The YCbCr format can be:
 *
 * 4:4:4 - CbCr width/height are the same as Y.
 * 4:2:2 - CbCr width is half that of Y. Height is the same.
 * 4:2:0 - CbCr width and height is half that of Y.
 *
 * The color format is detected based on the height/width ratios
 * defined above.
 * 
 * The Image that is rendered is the picture region defined by
 * mPicX, mPicY and mPicSize. The size of the rendered image is
 * mPicSize, not mYSize or mCbCrSize.
 */
class THEBES_API PlanarYCbCrImage : public Image {
public:
  struct Data {
    // Luminance buffer
    PRUint8* mYChannel;
    PRInt32 mYStride;
    gfxIntSize mYSize;
    // Chroma buffers
    PRUint8* mCbChannel;
    PRUint8* mCrChannel;
    PRInt32 mCbCrStride;
    gfxIntSize mCbCrSize;
    // Picture region
    PRUint32 mPicX;
    PRUint32 mPicY;
    gfxIntSize mPicSize;
  };

  enum {
    MAX_DIMENSION = 16384
  };

  /**
   * This makes a copy of the data buffers.
   * XXX Eventually we will change this to not make a copy of the data,
   * Right now it doesn't matter because the BasicLayer implementation
   * does YCbCr conversion here anyway.
   */
  virtual void SetData(const Data& aData) = 0;

protected:
  PlanarYCbCrImage(void* aImplData) : Image(aImplData, PLANAR_YCBCR) {}
};

/**
 * Currently, the data in a CairoImage surface is treated as being in the
 * device output color space.
 */
class THEBES_API CairoImage : public Image {
public:
  struct Data {
    gfxASurface* mSurface;
    gfxIntSize mSize;
  };

  /**
   * This can only be called on the main thread. It may add a reference
   * to the surface (which will eventually be released on the main thread).
   * The surface must not be modified after this call!!!
   */
  virtual void SetData(const Data& aData) = 0;

protected:
  CairoImage(void* aImplData) : Image(aImplData, CAIRO_SURFACE) {}
};

}
}

#endif /* GFX_IMAGELAYER_H */
