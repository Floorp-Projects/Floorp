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

  nsresult InitInternal() override;
  LexerResult DoDecode(SourceBufferIterator& aIterator,
                       IResumable* aOnResume) override;
  virtual Telemetry::ID SpeedHistogram() override;

  /// @return true if this PNG is a valid ICO resource.
  bool IsValidICO() const;

private:
  friend class DecoderFactory;

  // Decoders should only be instantiated via DecoderFactory.
  explicit nsPNGDecoder(RasterImage* aImage);

  /// The information necessary to create a frame.
  struct FrameInfo
  {
    gfx::SurfaceFormat mFormat;
    gfx::IntRect mFrameRect;
    bool mIsInterlaced;
  };

  nsresult CreateFrame(const FrameInfo& aFrameInfo);
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

  // Convenience methods to make interacting with StreamingLexer from inside
  // a libpng callback easier.
  void Terminate(png_structp aPNGStruct, TerminalState aState);
  void Yield(png_structp aPNGStruct);

  enum class State
  {
    PNG_DATA,
    FINISHED_PNG_DATA
  };

  LexerTransition<State> ReadPNGData(const char* aData, size_t aLength);
  LexerTransition<State> FinishedPNGData();

  StreamingLexer<State> mLexer;

  // The next lexer state transition. We need to store it here because we can't
  // directly return arbitrary values from libpng callbacks.
  LexerTransition<State> mNextTransition;

  // We yield to the caller every time we finish decoding a frame. When this
  // happens, we need to allocate the next frame after returning from the yield.
  // |mNextFrameInfo| is used to store the information needed to allocate the
  // next frame.
  Maybe<FrameInfo> mNextFrameInfo;

  // The length of the last chunk of data passed to ReadPNGData(). We use this
  // to arrange to arrive back at the correct spot in the data after yielding.
  size_t mLastChunkLength;

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
