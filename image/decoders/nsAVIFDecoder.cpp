/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLogging.h"  // Must appear first

#include "nsAVIFDecoder.h"

#include "aom/aomdx.h"

#include "mozilla/gfx/Types.h"
#include "YCbCrUtils.h"

#include "SurfacePipeFactory.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace image {

static LazyLogModule sAVIFLog("AVIFDecoder");

// Wrapper to allow rust to call our read adaptor.
intptr_t nsAVIFDecoder::read_source(uint8_t* aDestBuf, uintptr_t aDestBufSize,
                                    void* aUserData) {
  MOZ_ASSERT(aDestBuf);
  MOZ_ASSERT(aUserData);

  MOZ_LOG(sAVIFLog, LogLevel::Verbose,
          ("AVIF read_source, aDestBufSize: %zu", aDestBufSize));

  auto* decoder = reinterpret_cast<nsAVIFDecoder*>(aUserData);

  MOZ_ASSERT(decoder->mReadCursor);

  size_t bufferLength = decoder->mBufferedData.end() - decoder->mReadCursor;
  size_t n_bytes = std::min(aDestBufSize, bufferLength);

  MOZ_LOG(sAVIFLog, LogLevel::Verbose,
          ("AVIF read_source, %zu bytes ready, copying %zu", bufferLength,
           n_bytes));

  memcpy(aDestBuf, decoder->mReadCursor, n_bytes);
  decoder->mReadCursor += n_bytes;

  return n_bytes;
}

nsAVIFDecoder::nsAVIFDecoder(RasterImage* aImage)
    : Decoder(aImage), mParser(nullptr) {
  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] nsAVIFDecoder::nsAVIFDecoder", this));
}

nsAVIFDecoder::~nsAVIFDecoder() {
  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] nsAVIFDecoder::~nsAVIFDecoder", this));

  if (mParser) {
    mp4parse_avif_free(mParser);
  }

  if (mCodecContext) {
    aom_codec_err_t res = aom_codec_destroy(mCodecContext.ptr());

    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] aom_codec_destroy -> %d", this, res));
  }
}

LexerResult nsAVIFDecoder::DoDecode(SourceBufferIterator& aIterator,
                                    IResumable* aOnResume) {
  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] nsAVIFDecoder::DoDecode", this));

  // Since the SourceBufferIterator doesn't guarantee a contiguous buffer,
  // but the current mp4parse-rust implementation requires it, always buffer
  // locally. This keeps the code simpler at the cost of some performance, but
  // this implementation is only experimental, so we don't want to spend time
  // optimizing it prematurely.
  while (!mReadCursor) {
    SourceBufferIterator::State state =
        aIterator.AdvanceOrScheduleResume(SIZE_MAX, aOnResume);

    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] After advance, iterator state is %d", this, state));

    switch (state) {
      case SourceBufferIterator::WAITING:
        return LexerResult(Yield::NEED_MORE_DATA);

      case SourceBufferIterator::COMPLETE:
        mReadCursor = mBufferedData.begin();
        break;

      case SourceBufferIterator::READY: {  // copy new data to buffer
        MOZ_LOG(sAVIFLog, LogLevel::Debug,
                ("[this=%p] SourceBufferIterator ready, %zu bytes available",
                 this, aIterator.Length()));

        bool appendSuccess =
            mBufferedData.append(aIterator.Data(), aIterator.Length());

        if (!appendSuccess) {
          MOZ_LOG(sAVIFLog, LogLevel::Error,
                  ("[this=%p] Failed to append %zu bytes to buffer", this,
                   aIterator.Length()));
        }

        break;
      }

      default:
        MOZ_ASSERT_UNREACHABLE("unexpected SourceBufferIterator state");
    }
  }

  Mp4parseIo io = {nsAVIFDecoder::read_source, this};
  if (!mParser) {
    Mp4parseStatus status = mp4parse_avif_new(&io, &mParser);

    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] mp4parse_avif_new status: %d", this, status));
  }

  if (!mParser) {
    return LexerResult(TerminalState::FAILURE);
  }

  Mp4parseByteData mdat = {};  // change the name to 'primary_item' or something
  Mp4parseStatus status = mp4parse_avif_get_primary_item(mParser, &mdat);

  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] mp4parse_avif_get_primary_item -> %d", this, status));

  if (status != MP4PARSE_STATUS_OK) {
    return LexerResult(TerminalState::FAILURE);
  }

  aom_codec_iface_t* iface = aom_codec_av1_dx();
  aom_codec_ctx_t ctx;
  aom_codec_err_t res =
      aom_codec_dec_init(&ctx, iface, /* cfg = */ nullptr, /* flags = */ 0);

  MOZ_LOG(
      sAVIFLog, LogLevel::Error,
      ("[this=%p] aom_codec_dec_init -> %d, name = %s", this, res, ctx.name));

  if (res == AOM_CODEC_OK) {
    mCodecContext = Some(ctx);
  } else {
    return LexerResult(TerminalState::FAILURE);
  }

  res = aom_codec_decode(mCodecContext.ptr(), mdat.data, mdat.length, nullptr);

  if (res != AOM_CODEC_OK) {
    MOZ_LOG(sAVIFLog, LogLevel::Error,
            ("[this=%p] aom_codec_decode -> %d", this, res));
    return LexerResult(TerminalState::FAILURE);
  }

  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] aom_codec_decode -> %d", this, res));

  aom_codec_iter_t iter = nullptr;
  const aom_image_t* img = aom_codec_get_frame(mCodecContext.ptr(), &iter);

  if (img == nullptr) {
    MOZ_LOG(sAVIFLog, LogLevel::Error,
            ("[this=%p] aom_codec_get_frame -> %p", this, img));
    return LexerResult(TerminalState::FAILURE);
  }

  const CheckedInt<int> decoded_width = img->d_w;
  const CheckedInt<int> decoded_height = img->d_h;

  if (!decoded_height.isValid() || !decoded_width.isValid()) {
    MOZ_LOG(
        sAVIFLog, LogLevel::Debug,
        ("[this=%p] image dimensions can't be stored in int: d_w: %u, d_h: %u",
         this, img->d_w, img->d_h));
    return LexerResult(TerminalState::FAILURE);
  }

  PostSize(decoded_width.value(), decoded_height.value());

  // TODO: This doesn't account for the alpha plane in a separate frame
  const bool hasAlpha = false;
  if (hasAlpha) {
    PostHasTransparency();
  }

  if (IsMetadataDecode()) {
    return LexerResult(TerminalState::SUCCESS);
  }

  MOZ_ASSERT(img->stride[AOM_PLANE_Y] == img->stride[AOM_PLANE_ALPHA]);
  MOZ_ASSERT(img->stride[AOM_PLANE_Y] >= aom_img_plane_width(img, AOM_PLANE_Y));
  MOZ_ASSERT(img->stride[AOM_PLANE_U] == img->stride[AOM_PLANE_V]);
  MOZ_ASSERT(img->stride[AOM_PLANE_U] >= aom_img_plane_width(img, AOM_PLANE_U));
  MOZ_ASSERT(img->stride[AOM_PLANE_V] >= aom_img_plane_width(img, AOM_PLANE_V));
  MOZ_ASSERT(aom_img_plane_width(img, AOM_PLANE_U) ==
             aom_img_plane_width(img, AOM_PLANE_V));
  MOZ_ASSERT(aom_img_plane_height(img, AOM_PLANE_U) ==
             aom_img_plane_height(img, AOM_PLANE_V));

  layers::PlanarYCbCrData data;
  data.mYChannel = img->planes[AOM_PLANE_Y];
  data.mYStride = img->stride[AOM_PLANE_Y];
  data.mYSize = gfx::IntSize(aom_img_plane_width(img, AOM_PLANE_Y),
                             aom_img_plane_height(img, AOM_PLANE_Y));
  data.mYSkip =
      img->stride[AOM_PLANE_Y] - aom_img_plane_width(img, AOM_PLANE_Y);
  data.mCbChannel = img->planes[AOM_PLANE_U];
  data.mCrChannel = img->planes[AOM_PLANE_V];
  data.mCbCrStride = img->stride[AOM_PLANE_U];
  data.mCbCrSize = gfx::IntSize(aom_img_plane_width(img, AOM_PLANE_U),
                                aom_img_plane_height(img, AOM_PLANE_U));
  data.mCbSkip =
      img->stride[AOM_PLANE_U] - aom_img_plane_width(img, AOM_PLANE_U);
  data.mCrSkip =
      img->stride[AOM_PLANE_V] - aom_img_plane_width(img, AOM_PLANE_V);
  data.mPicX = 0;
  data.mPicY = 0;
  data.mPicSize = gfx::IntSize(decoded_width.value(), decoded_height.value());
  data.mStereoMode = StereoMode::MONO;
  data.mColorDepth = ColorDepthForBitDepth(img->bit_depth);

  switch (img->cp) {
    case AOM_CICP_CP_BT_601:
      data.mYUVColorSpace = gfx::YUVColorSpace::BT601;
      break;
    case AOM_CICP_CP_BT_709:
      data.mYUVColorSpace = gfx::YUVColorSpace::BT709;
      break;
    case AOM_CICP_CP_BT_2020:
      data.mYUVColorSpace = gfx::YUVColorSpace::BT2020;
      break;
    default:
      MOZ_LOG(sAVIFLog, LogLevel::Debug,
              ("[this=%p] unsupported aom_color_primaries value: %u", this,
               img->cp));
      data.mYUVColorSpace = gfx::YUVColorSpace::UNKNOWN;
  }

  switch (img->range) {
    case AOM_CR_STUDIO_RANGE:
      data.mColorRange = gfx::ColorRange::LIMITED;
      break;
    case AOM_CR_FULL_RANGE:
      data.mColorRange = gfx::ColorRange::FULL;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unknown color range");
  }

  gfx::SurfaceFormat format =
      hasAlpha ? SurfaceFormat::OS_RGBA : SurfaceFormat::OS_RGBX;
  const IntSize intrinsicSize = Size();
  IntSize rgbSize = intrinsicSize;

  gfx::GetYCbCrToRGBDestFormatAndSize(data, format, rgbSize);
  const int bytesPerPixel = BytesPerPixel(format);

  const CheckedInt rgbStride = CheckedInt<int>(rgbSize.width) * bytesPerPixel;
  const CheckedInt rgbBufLength = rgbStride * rgbSize.height;

  if (!rgbStride.isValid() || !rgbBufLength.isValid()) {
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] overflow calculating rgbBufLength: rbgSize.width: %d, "
             "rgbSize.height: %d, "
             "bytesPerPixel: %u",
             this, rgbSize.width, rgbSize.height, bytesPerPixel));
    return LexerResult(TerminalState::FAILURE);
  }

  UniquePtr<uint8_t[]> rgbBuf = MakeUnique<uint8_t[]>(rgbBufLength.value());
  const uint8_t* endOfRgbBuf = {rgbBuf.get() + rgbBufLength.value()};

  if (!rgbBuf) {
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] allocation of %u-byte rgbBuf failed", this,
             rgbBufLength.value()));
    return LexerResult(TerminalState::FAILURE);
  }

  gfx::ConvertYCbCrToRGB(data, format, rgbSize, rgbBuf.get(),
                         rgbStride.value());

  Maybe<SurfacePipe> pipe = SurfacePipeFactory::CreateSurfacePipe(
      this, rgbSize, OutputSize(), FullFrame(), format, format, Nothing(),
      nullptr, SurfacePipeFlags());

  if (!pipe) {
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] could not initialize surface pipe", this));
    return LexerResult(TerminalState::FAILURE);
  }

  WriteState writeBufferResult = WriteState::NEED_MORE_DATA;
  for (uint8_t* rowPtr = rgbBuf.get(); rowPtr < endOfRgbBuf;
       rowPtr += rgbStride.value()) {
    writeBufferResult = pipe->WriteBuffer(reinterpret_cast<uint32_t*>(rowPtr));

    Maybe<SurfaceInvalidRect> invalidRect = pipe->TakeInvalidRect();
    if (invalidRect) {
      PostInvalidation(invalidRect->mInputSpaceRect,
                       Some(invalidRect->mOutputSpaceRect));
    }

    if (writeBufferResult == WriteState::FAILURE) {
      MOZ_LOG(sAVIFLog, LogLevel::Debug,
              ("[this=%p] error writing rowPtr to surface pipe", this));

    } else if (writeBufferResult == WriteState::FINISHED) {
      MOZ_ASSERT(rowPtr + rgbStride.value() == endOfRgbBuf);
    }
  }

  // We don't support image sequences yet
  DebugOnly<aom_image_t*> next_img =
      aom_codec_get_frame(mCodecContext.ptr(), &iter);
  MOZ_ASSERT(next_img == nullptr);

  if (writeBufferResult == WriteState::FINISHED) {
    PostFrameStop(hasAlpha ? Opacity::SOME_TRANSPARENCY
                           : Opacity::FULLY_OPAQUE);
    PostDecodeDone();
    return LexerResult(TerminalState::SUCCESS);
  }

  return LexerResult(TerminalState::FAILURE);
}

}  // namespace image
}  // namespace mozilla
