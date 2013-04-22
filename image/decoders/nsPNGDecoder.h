/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPNGDecoder_h__
#define nsPNGDecoder_h__

#include "Decoder.h"

#include "gfxASurface.h"

#include "nsCOMPtr.h"

#include "png.h"

#include "qcms.h"

namespace mozilla {
namespace image {
class RasterImage;

class nsPNGDecoder : public Decoder
{
public:
  nsPNGDecoder(RasterImage &aImage);
  virtual ~nsPNGDecoder();

  virtual void InitInternal();
  virtual void WriteInternal(const char* aBuffer, uint32_t aCount);
  virtual Telemetry::ID SpeedHistogram();

  void CreateFrame(png_uint_32 x_offset, png_uint_32 y_offset,
                   int32_t width, int32_t height,
                   gfxASurface::gfxImageFormat format);
  void EndImageFrame();

  // Check if PNG is valid ICO (32bpp RGBA)
  // http://blogs.msdn.com/b/oldnewthing/archive/2010/10/22/10079192.aspx
  bool IsValidICO() const
  {
    // If there are errors in the call to png_get_IHDR, the error_callback in
    // nsPNGDecoder.cpp is called.  In this error callback we do a longjmp, so
    // we need to save the jump buffer here. Oterwise we'll end up without a
    // proper callstack.
    if (setjmp(png_jmpbuf(mPNG))) {
      // We got here from a longjmp call indirectly from png_get_IHDR
      return false;
    }

    png_uint_32
        png_width,  // Unused
        png_height; // Unused

    int png_bit_depth,
        png_color_type;

    if (png_get_IHDR(mPNG, mInfo, &png_width, &png_height, &png_bit_depth,
                     &png_color_type, NULL, NULL, NULL)) {

      return ((png_color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
               png_color_type == PNG_COLOR_TYPE_RGB) &&
              png_bit_depth == 8);
    } else {
      return false;
    }
  }

public:
  png_structp mPNG;
  png_infop mInfo;
  nsIntRect mFrameRect;
  uint8_t *mCMSLine;
  uint8_t *interlacebuf;
  qcms_profile *mInProfile;
  qcms_transform *mTransform;

  gfxASurface::gfxImageFormat format;

  // For size decodes
  uint8_t *mHeaderBuf;
  uint32_t mHeaderBytesRead;

  uint8_t mChannels;
  bool mFrameHasNoAlpha;
  bool mFrameIsHidden;

  // whether CMS or premultiplied alpha are forced off
  uint32_t mCMSMode;
  bool mDisablePremultipliedAlpha;

  struct AnimFrameInfo
  {
    AnimFrameInfo();
#ifdef PNG_APNG_SUPPORTED
    AnimFrameInfo(png_structp aPNG, png_infop aInfo);
#endif

    RasterImage::FrameDisposalMethod mDispose;
    RasterImage::FrameBlendMethod mBlend;
    int32_t mTimeout;
  };

  AnimFrameInfo mAnimInfo;

  // The number of frames we've finished.
  uint32_t mNumFrames;
  
  /*
   * libpng callbacks
   *
   * We put these in the class so that they can access protected members.
   */
  static void PNGAPI info_callback(png_structp png_ptr, png_infop info_ptr);
  static void PNGAPI row_callback(png_structp png_ptr, png_bytep new_row,
                                  png_uint_32 row_num, int pass);
#ifdef PNG_APNG_SUPPORTED
  static void PNGAPI frame_info_callback(png_structp png_ptr,
                                         png_uint_32 frame_num);
#endif
  static void PNGAPI end_callback(png_structp png_ptr, png_infop info_ptr);
  static void PNGAPI error_callback(png_structp png_ptr,
                                    png_const_charp error_msg);
  static void PNGAPI warning_callback(png_structp png_ptr,
                                      png_const_charp warning_msg);

  // This is defined in the PNG spec as an invariant. We use it to
  // do manual validation without libpng.
  static const uint8_t pngSignatureBytes[];
};

} // namespace image
} // namespace mozilla

#endif // nsPNGDecoder_h__
