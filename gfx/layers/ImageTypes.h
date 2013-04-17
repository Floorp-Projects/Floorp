/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGETYPES_H
#define GFX_IMAGETYPES_H

namespace mozilla {

enum ImageFormat {
  /**
   * The PLANAR_YCBCR format creates a PlanarYCbCrImage. All backends should
   * support this format, because the Ogg video decoder depends on it.
   * The maximum image width and height is 16384.
   */
  PLANAR_YCBCR,

  /**
   * The GRALLOC_PLANAR_YCBCR format creates a GrallocPlanarYCbCrImage, a
   * subtype of PlanarYCbCrImage. It takes a PlanarYCbCrImage data and can be
   * used as a texture by Gonk backend directly.
   */
  GRALLOC_PLANAR_YCBCR,

  /**
   * The SHARED_RGB format creates a SharedRGBImage, which stores RGB data in
   * shared memory. Some Android hardware video decoders require this format.
   * Currently only used on Android.
   */
  SHARED_RGB,

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
  CAIRO_SURFACE,

  /**
   * The GONK_IO_SURFACE format creates a GonkIOSurfaceImage.
   *
   * It wraps an GraphicBuffer object and binds it directly to a GL texture.
   */
  GONK_IO_SURFACE,

  /**
   * An bitmap image that can be shared with a remote process.
   */
  REMOTE_IMAGE_BITMAP,

  /**
   * A OpenGL texture that can be shared across threads or processes
   */
  SHARED_TEXTURE,

  /**
   * An DXGI shared surface handle that can be shared with a remote process.
   */
  REMOTE_IMAGE_DXGI_TEXTURE
};


enum StereoMode {
  STEREO_MODE_MONO,
  STEREO_MODE_LEFT_RIGHT,
  STEREO_MODE_RIGHT_LEFT,
  STEREO_MODE_BOTTOM_TOP,
  STEREO_MODE_TOP_BOTTOM
};


} // namespace

#endif
