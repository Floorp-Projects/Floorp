/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPNGDecoder_h__
#define nsPNGDecoder_h__

#include "Decoder.h"

#include "imgIDecoderObserver.h"
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
  nsPNGDecoder(RasterImage &aImage, imgIDecoderObserver* aObserver);
  virtual ~nsPNGDecoder();

  virtual void InitInternal();
  virtual void WriteInternal(const char* aBuffer, PRUint32 aCount);
  virtual Telemetry::ID SpeedHistogram();

  void CreateFrame(png_uint_32 x_offset, png_uint_32 y_offset,
                   PRInt32 width, PRInt32 height,
                   gfxASurface::gfxImageFormat format);
  void SetAnimFrameInfo();

  void EndImageFrame();

  // Check if PNG is valid ICO (32bpp RGBA)
  // http://blogs.msdn.com/b/oldnewthing/archive/2010/10/22/10079192.aspx
  bool IsValidICO() const
  {
    png_uint_32
        png_width,  // Unused
        png_height; // Unused

    int png_bit_depth,
        png_color_type;

    if (png_get_IHDR(mPNG, mInfo, &png_width, &png_height, &png_bit_depth,
                     &png_color_type, NULL, NULL, NULL)) {

      return (png_color_type == PNG_COLOR_TYPE_RGB_ALPHA &&
              png_bit_depth == 8);
    } else {
      return false;
    }
  }

public:
  png_structp mPNG;
  png_infop mInfo;
  nsIntRect mFrameRect;
  PRUint8 *mCMSLine;
  PRUint8 *interlacebuf;
  PRUint8 *mImageData;
  qcms_profile *mInProfile;
  qcms_transform *mTransform;

  gfxASurface::gfxImageFormat format;

  // For size decodes
  PRUint8 *mHeaderBuf;
  PRUint32 mHeaderBytesRead;

  PRUint8 mChannels;
  bool mFrameHasNoAlpha;
  bool mFrameIsHidden;

  // whether CMS or premultiplied alpha are forced off
  PRUint32 mCMSMode;
  bool mDisablePremultipliedAlpha;
  
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
  static const PRUint8 pngSignatureBytes[];
};

} // namespace image
} // namespace mozilla

#endif // nsPNGDecoder_h__
