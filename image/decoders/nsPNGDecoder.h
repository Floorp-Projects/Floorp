/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsPNGDecoder_h
#define mozilla_image_decoders_nsPNGDecoder_h

#include "Decoder.h"
#include "png.h"
#include "qcms.h"
#include "StreamingLexer.h"
#include "SurfacePipe.h"

namespace mozilla {
namespace image {
class RasterImage;

class nsPNGDecoder : public Decoder
{
public:
  virtual ~nsPNGDecoder();

  virtual void InitInternal() override;
  virtual void WriteInternal(const char* aBuffer, uint32_t aCount) override;
  virtual Telemetry::ID SpeedHistogram() override;

private:
  friend class DecoderFactory;
  friend class nsICODecoder;

  // Decoders should only be instantiated via DecoderFactory.
  // XXX(seth): nsICODecoder is temporarily an exception to this rule.
  explicit nsPNGDecoder(RasterImage* aImage);

  nsresult CreateFrame(gfx::SurfaceFormat aFormat,
                       const gfx::IntRect& aFrameRect,
                       bool aIsInterlaced);
  void EndImageFrame();

  enum class TransparencyType
  {
    eNone,
    eAlpha,
    eFrameRect
  };

  TransparencyType GetTransparencyType(gfx::SurfaceFormat aFormat,
                                       const gfx::IntRect& aFrameRect);
  void PostHasTransparencyIfNeeded(TransparencyType aTransparencyType);

  void PostInvalidationIfNeeded();

  void WriteRow(uint8_t* aRow);

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
                     &png_color_type, nullptr, nullptr, nullptr)) {

      return ((png_color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
               png_color_type == PNG_COLOR_TYPE_RGB) &&
              png_bit_depth == 8);
    } else {
      return false;
    }
  }

  enum class State
  {
    PNG_DATA,
    FINISHED_PNG_DATA
  };

  LexerTransition<State> ReadPNGData(const char* aData, size_t aLength);
  LexerTransition<State> FinishedPNGData();

  StreamingLexer<State> mLexer;

public:
  png_structp mPNG;
  png_infop mInfo;
  nsIntRect mFrameRect;
  uint8_t* mCMSLine;
  uint8_t* interlacebuf;
  qcms_profile* mInProfile;
  qcms_transform* mTransform;

  gfx::SurfaceFormat format;

  // whether CMS or premultiplied alpha are forced off
  uint32_t mCMSMode;

  uint8_t mChannels;
  uint8_t mPass;
  bool mFrameIsHidden;
  bool mDisablePremultipliedAlpha;

  struct AnimFrameInfo
  {
    AnimFrameInfo();
#ifdef PNG_APNG_SUPPORTED
    AnimFrameInfo(png_structp aPNG, png_infop aInfo);
#endif

    DisposalMethod mDispose;
    BlendMethod mBlend;
    int32_t mTimeout;
  };

  AnimFrameInfo mAnimInfo;

  SurfacePipe mPipe;  /// The SurfacePipe used to write to the output surface.

  // The number of frames we've finished.
  uint32_t mNumFrames;

  // libpng callbacks
  // We put these in the class so that they can access protected members.
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

#endif // mozilla_image_decoders_nsPNGDecoder_h
