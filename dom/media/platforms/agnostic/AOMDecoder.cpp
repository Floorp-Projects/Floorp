/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AOMDecoder.h"

#include <algorithm>

#include "BitWriter.h"
#include "BitReader.h"
#include "ImageContainer.h"
#include "MediaResult.h"
#include "TimeUnits.h"
#include <aom/aom_image.h>
#include <aom/aomdx.h>
#include "gfx2DGlue.h"
#include "gfxUtils.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TaskQueue.h"
#include "nsError.h"
#include "nsThreadUtils.h"
#include "prsystem.h"
#include "VideoUtils.h"

#undef LOG
#define LOG(arg, ...)                                                  \
  DDMOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, "::%s: " arg, __func__, \
            ##__VA_ARGS__)
#define LOG_RESULT(code, message, ...)                                        \
  DDMOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, "::%s: %s (code %d) " message, \
            __func__, aom_codec_err_to_string(code), (int)code, ##__VA_ARGS__)
#define LOGEX_RESULT(_this, code, message, ...)         \
  DDMOZ_LOGEX(_this, sPDMLog, mozilla::LogLevel::Debug, \
              "::%s: %s (code %d) " message, __func__,  \
              aom_codec_err_to_string(code), (int)code, ##__VA_ARGS__)
#define LOG_STATIC_RESULT(code, message, ...)                 \
  MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug,                  \
          ("AOMDecoder::%s: %s (code %d) " message, __func__, \
           aom_codec_err_to_string(code), (int)code, ##__VA_ARGS__))

#define ASSERT_BYTE_ALIGNED(bitIO) MOZ_ASSERT((bitIO).BitCount() % 8 == 0)

namespace mozilla {

using namespace gfx;
using namespace layers;
using gfx::CICP::ColourPrimaries;
using gfx::CICP::MatrixCoefficients;
using gfx::CICP::TransferCharacteristics;

static MediaResult InitContext(AOMDecoder& aAOMDecoder, aom_codec_ctx_t* aCtx,
                               const VideoInfo& aInfo) {
  aom_codec_iface_t* dx = aom_codec_av1_dx();
  if (!dx) {
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("Couldn't get AV1 decoder interface."));
  }

  size_t decode_threads = 2;
  if (aInfo.mDisplay.width >= 2048) {
    decode_threads = 8;
  } else if (aInfo.mDisplay.width >= 1024) {
    decode_threads = 4;
  }
  decode_threads = std::min(decode_threads, GetNumberOfProcessors());

  aom_codec_dec_cfg_t config;
  PodZero(&config);
  config.threads = static_cast<unsigned int>(decode_threads);
  config.w = config.h = 0;  // set after decode
  config.allow_lowbitdepth = true;

  aom_codec_flags_t flags = 0;

  auto res = aom_codec_dec_init(aCtx, dx, &config, flags);
  if (res != AOM_CODEC_OK) {
    LOGEX_RESULT(&aAOMDecoder, res, "Codec initialization failed, res=%d",
                 int(res));
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("AOM error initializing AV1 decoder: %s",
                                     aom_codec_err_to_string(res)));
  }
  return NS_OK;
}

AOMDecoder::AOMDecoder(const CreateDecoderParams& aParams)
    : mImageContainer(aParams.mImageContainer),
      mTaskQueue(TaskQueue::Create(
          GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER), "AOMDecoder")),
      mInfo(aParams.VideoConfig()),
      mTrackingId(aParams.mTrackingId) {
  PodZero(&mCodec);
}

AOMDecoder::~AOMDecoder() = default;

RefPtr<ShutdownPromise> AOMDecoder::Shutdown() {
  RefPtr<AOMDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self]() {
    auto res = aom_codec_destroy(&self->mCodec);
    if (res != AOM_CODEC_OK) {
      LOGEX_RESULT(self.get(), res, "aom_codec_destroy");
    }
    return self->mTaskQueue->BeginShutdown();
  });
}

RefPtr<MediaDataDecoder::InitPromise> AOMDecoder::Init() {
  MediaResult rv = InitContext(*this, &mCodec, mInfo);
  if (NS_FAILED(rv)) {
    return AOMDecoder::InitPromise::CreateAndReject(rv, __func__);
  }
  return AOMDecoder::InitPromise::CreateAndResolve(TrackInfo::kVideoTrack,
                                                   __func__);
}

RefPtr<MediaDataDecoder::FlushPromise> AOMDecoder::Flush() {
  return InvokeAsync(mTaskQueue, __func__, [this, self = RefPtr(this)]() {
    mPerformanceRecorder.Record(std::numeric_limits<int64_t>::max());
    return FlushPromise::CreateAndResolve(true, __func__);
  });
}

// UniquePtr dtor wrapper for aom_image_t.
struct AomImageFree {
  void operator()(aom_image_t* img) { aom_img_free(img); }
};

RefPtr<MediaDataDecoder::DecodePromise> AOMDecoder::ProcessDecode(
    MediaRawData* aSample) {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

#if defined(DEBUG)
  NS_ASSERTION(
      IsKeyframe(*aSample) == aSample->mKeyframe,
      "AOM Decode Keyframe error sample->mKeyframe and si.si_kf out of sync");
#endif

  MediaInfoFlag flag = MediaInfoFlag::None;
  flag |= (aSample->mKeyframe ? MediaInfoFlag::KeyFrame
                              : MediaInfoFlag::NonKeyFrame);
  flag |= MediaInfoFlag::SoftwareDecoding;
  flag |= MediaInfoFlag::VIDEO_AV1;

  mTrackingId.apply([&](const auto& aId) {
    mPerformanceRecorder.Start(aSample->mTimecode.ToMicroseconds(),
                               "AOMDecoder"_ns, aId, flag);
  });

  if (aom_codec_err_t r = aom_codec_decode(&mCodec, aSample->Data(),
                                           aSample->Size(), nullptr)) {
    LOG_RESULT(r, "Decode error!");
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                    RESULT_DETAIL("AOM error decoding AV1 sample: %s",
                                  aom_codec_err_to_string(r))),
        __func__);
  }

  aom_codec_iter_t iter = nullptr;
  aom_image_t* img;
  UniquePtr<aom_image_t, AomImageFree> img8;
  DecodedData results;

  while ((img = aom_codec_get_frame(&mCodec, &iter))) {
    NS_ASSERTION(
        img->fmt == AOM_IMG_FMT_I420 || img->fmt == AOM_IMG_FMT_I42016 ||
            img->fmt == AOM_IMG_FMT_I444 || img->fmt == AOM_IMG_FMT_I44416,
        "AV1 image format not I420 or I444");

    // Chroma shifts are rounded down as per the decoding examples in the SDK
    VideoData::YCbCrBuffer b;
    b.mPlanes[0].mData = img->planes[0];
    b.mPlanes[0].mStride = img->stride[0];
    b.mPlanes[0].mHeight = img->d_h;
    b.mPlanes[0].mWidth = img->d_w;
    b.mPlanes[0].mSkip = 0;

    b.mPlanes[1].mData = img->planes[1];
    b.mPlanes[1].mStride = img->stride[1];
    b.mPlanes[1].mSkip = 0;

    b.mPlanes[2].mData = img->planes[2];
    b.mPlanes[2].mStride = img->stride[2];
    b.mPlanes[2].mSkip = 0;

    if (img->fmt == AOM_IMG_FMT_I420 || img->fmt == AOM_IMG_FMT_I42016) {
      b.mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;

      b.mPlanes[1].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[1].mWidth = (img->d_w + 1) >> img->x_chroma_shift;

      b.mPlanes[2].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[2].mWidth = (img->d_w + 1) >> img->x_chroma_shift;
    } else if (img->fmt == AOM_IMG_FMT_I444 || img->fmt == AOM_IMG_FMT_I44416) {
      b.mPlanes[1].mHeight = img->d_h;
      b.mPlanes[1].mWidth = img->d_w;

      b.mPlanes[2].mHeight = img->d_h;
      b.mPlanes[2].mWidth = img->d_w;
    } else {
      LOG("AOM Unknown image format");
      return DecodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                      RESULT_DETAIL("AOM Unknown image format")),
          __func__);
    }

    if (img->bit_depth == 10) {
      b.mColorDepth = ColorDepth::COLOR_10;
    } else if (img->bit_depth == 12) {
      b.mColorDepth = ColorDepth::COLOR_12;
    }

    switch (img->mc) {
      case AOM_CICP_MC_BT_601:
        b.mYUVColorSpace = YUVColorSpace::BT601;
        break;
      case AOM_CICP_MC_BT_2020_NCL:
      case AOM_CICP_MC_BT_2020_CL:
        b.mYUVColorSpace = YUVColorSpace::BT2020;
        break;
      case AOM_CICP_MC_BT_709:
        b.mYUVColorSpace = YUVColorSpace::BT709;
        break;
      default:
        b.mYUVColorSpace = DefaultColorSpace({img->d_w, img->d_h});
        break;
    }
    b.mColorRange = img->range == AOM_CR_FULL_RANGE ? ColorRange::FULL
                                                    : ColorRange::LIMITED;

    switch (img->cp) {
      case AOM_CICP_CP_BT_709:
        b.mColorPrimaries = ColorSpace2::BT709;
        break;
      case AOM_CICP_CP_BT_2020:
        b.mColorPrimaries = ColorSpace2::BT2020;
        break;
      default:
        b.mColorPrimaries = ColorSpace2::BT709;
        break;
    }

    RefPtr<VideoData> v;
    v = VideoData::CreateAndCopyData(
        mInfo, mImageContainer, aSample->mOffset, aSample->mTime,
        aSample->mDuration, b, aSample->mKeyframe, aSample->mTimecode,
        mInfo.ScaledImageRect(img->d_w, img->d_h), nullptr);

    if (!v) {
      LOG("Image allocation error source %ux%u display %ux%u picture %ux%u",
          img->d_w, img->d_h, mInfo.mDisplay.width, mInfo.mDisplay.height,
          mInfo.mImage.width, mInfo.mImage.height);
      return DecodePromise::CreateAndReject(
          MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__), __func__);
    }
    mPerformanceRecorder.Record(
        aSample->mTimecode.ToMicroseconds(), [&](DecodeStage& aStage) {
          aStage.SetResolution(mInfo.mImage.width, mInfo.mImage.height);
          auto format = [&]() -> Maybe<DecodeStage::ImageFormat> {
            switch (img->fmt) {
              case AOM_IMG_FMT_I420:
              case AOM_IMG_FMT_I42016:
                return Some(DecodeStage::YUV420P);
              case AOM_IMG_FMT_I444:
              case AOM_IMG_FMT_I44416:
                return Some(DecodeStage::YUV444P);
              default:
                return Nothing();
            }
          }();
          format.apply([&](auto& aFmt) { aStage.SetImageFormat(aFmt); });
          aStage.SetYUVColorSpace(b.mYUVColorSpace);
          aStage.SetColorRange(b.mColorRange);
          aStage.SetColorDepth(b.mColorDepth);
        });
    results.AppendElement(std::move(v));
  }
  return DecodePromise::CreateAndResolve(std::move(results), __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> AOMDecoder::Decode(
    MediaRawData* aSample) {
  return InvokeAsync<MediaRawData*>(mTaskQueue, this, __func__,
                                    &AOMDecoder::ProcessDecode, aSample);
}

RefPtr<MediaDataDecoder::DecodePromise> AOMDecoder::Drain() {
  return InvokeAsync(mTaskQueue, __func__, [] {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  });
}

/* static */
bool AOMDecoder::IsAV1(const nsACString& aMimeType) {
  return aMimeType.EqualsLiteral("video/av1");
}

/* static */
bool AOMDecoder::IsKeyframe(Span<const uint8_t> aBuffer) {
  aom_codec_stream_info_t info;
  PodZero(&info);

  auto res = aom_codec_peek_stream_info(aom_codec_av1_dx(), aBuffer.Elements(),
                                        aBuffer.Length(), &info);
  if (res != AOM_CODEC_OK) {
    LOG_STATIC_RESULT(
        res, "couldn't get keyframe flag with aom_codec_peek_stream_info");
    return false;
  }

  return bool(info.is_kf);
}

/* static */
gfx::IntSize AOMDecoder::GetFrameSize(Span<const uint8_t> aBuffer) {
  aom_codec_stream_info_t info;
  PodZero(&info);

  auto res = aom_codec_peek_stream_info(aom_codec_av1_dx(), aBuffer.Elements(),
                                        aBuffer.Length(), &info);
  if (res != AOM_CODEC_OK) {
    LOG_STATIC_RESULT(
        res, "couldn't get frame size with aom_codec_peek_stream_info");
  }

  return gfx::IntSize(info.w, info.h);
}

/* static */
AOMDecoder::OBUIterator AOMDecoder::ReadOBUs(const Span<const uint8_t>& aData) {
  return OBUIterator(aData);
}

void AOMDecoder::OBUIterator::UpdateNext() {
  // If mGoNext is not set, we don't need to load a new OBU.
  if (!mGoNext) {
    return;
  }
  // Check if we've reached the end of the data. Allow mGoNext to stay true so
  // that HasNext() will return false.
  if (mPosition >= mData.Length()) {
    return;
  }
  mGoNext = false;

  // If retrieving the next OBU fails, reset the current OBU and set the
  // position past the end of the data so that HasNext() returns false.
  auto resetExit = MakeScopeExit([&]() {
    mCurrent = OBUInfo();
    mPosition = mData.Length();
  });

  auto subspan = mData.Subspan(mPosition, mData.Length() - mPosition);
  BitReader br(subspan.Elements(), subspan.Length() * 8);
  OBUInfo temp;

  // AV1 spec available at:
  // https://aomediacodec.github.io/av1-spec/
  // or https://aomediacodec.github.io/av1-spec/av1-spec.pdf

  // begin open_bitstream_unit( )
  // https://aomediacodec.github.io/av1-spec/#general-obu-syntax

  // begin obu_header( )
  // https://aomediacodec.github.io/av1-spec/#obu-header-syntax
  br.ReadBit();  // obu_forbidden_bit
  temp.mType = static_cast<OBUType>(br.ReadBits(4));
  if (!temp.IsValid()) {
    // Non-fatal error, unknown OBUs can be skipped as long as the size field
    // is properly specified.
    NS_WARNING(nsPrintfCString("Encountered unknown OBU type (%" PRIu8
                               ", OBU may be invalid",
                               static_cast<uint8_t>(temp.mType))
                   .get());
  }
  temp.mExtensionFlag = br.ReadBit();
  bool hasSizeField = br.ReadBit();
  br.ReadBit();  // obu_reserved_1bit

  // begin obu_extension_header( ) (5.3.3)
  if (temp.mExtensionFlag) {
    if (br.BitsLeft() < 8) {
      mResult = MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                            "Not enough bits left for an OBU extension header");
      return;
    }
    br.ReadBits(3);  // temporal_id
    br.ReadBits(2);  // spatial_id
    br.ReadBits(3);  // extension_header_reserved_3bits
  }
  // end obu_extension_header( )
  // end obu_header( )

  // Get the size of the remaining OBU data attached to the header in
  // bytes.
  size_t size;
  if (hasSizeField) {
    if (br.BitsLeft() < 8) {
      mResult = MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                            "Not enough bits left for an OBU size field");
      return;
    }
    CheckedUint32 checkedSize = br.ReadULEB128().toChecked<uint32_t>();
    // Spec requires that the value ULEB128 reads is (1 << 32) - 1 or below.
    // See leb128(): https://aomediacodec.github.io/av1-spec/#leb128
    if (!checkedSize.isValid()) {
      mResult =
          MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, "OBU size was too large");
      return;
    }
    size = checkedSize.value();
  } else {
    // This case should rarely happen in practice. To support the Annex B
    // format in the specification, we would have to parse every header type
    // to skip over them, but this allows us to at least iterate once to
    // retrieve the first OBU in the data.
    size = mData.Length() - 1 - temp.mExtensionFlag;
  }

  if (br.BitsLeft() / 8 < size) {
    mResult = MediaResult(
        NS_ERROR_DOM_MEDIA_DECODE_ERR,
        nsPrintfCString("Size specified by the OBU header (%zu) is more "
                        "than the actual remaining OBU data (%zu)",
                        size, br.BitsLeft() / 8)
            .get());
    return;
  }

  ASSERT_BYTE_ALIGNED(br);

  size_t bytes = br.BitCount() / 8;
  temp.mContents = mData.Subspan(mPosition + bytes, size);
  mCurrent = temp;
  // end open_bitstream_unit( )

  mPosition += bytes + size;
  resetExit.release();
  mResult = NS_OK;
}

/* static */
already_AddRefed<MediaByteBuffer> AOMDecoder::CreateOBU(
    const OBUType aType, const Span<const uint8_t>& aContents) {
  RefPtr<MediaByteBuffer> buffer = new MediaByteBuffer();

  BitWriter bw(buffer);
  bw.WriteBits(0, 1);  // obu_forbidden_bit
  bw.WriteBits(static_cast<uint8_t>(aType), 4);
  bw.WriteBit(false);  // obu_extension_flag
  bw.WriteBit(true);   // obu_has_size_field
  bw.WriteBits(0, 1);  // obu_reserved_1bit
  ASSERT_BYTE_ALIGNED(bw);
  bw.WriteULEB128(aContents.Length());
  ASSERT_BYTE_ALIGNED(bw);

  buffer->AppendElements(aContents.Elements(), aContents.Length());
  return buffer.forget();
}

/* static */
MediaResult AOMDecoder::ReadSequenceHeaderInfo(
    const Span<const uint8_t>& aSample, AV1SequenceInfo& aDestInfo) {
  // We need to get the last sequence header OBU, the specification does not
  // limit a temporal unit to one sequence header.
  OBUIterator iter = ReadOBUs(aSample);
  OBUInfo seqOBU;

  while (true) {
    if (!iter.HasNext()) {
      // Pass along the error from parsing the OBU.
      MediaResult result = iter.GetResult();
      if (result.Code() != NS_OK) {
        return result;
      }
      break;
    }
    OBUInfo obu = iter.Next();
    if (obu.mType == OBUType::SequenceHeader) {
      seqOBU = obu;
    }
  }

  if (seqOBU.mType != OBUType::SequenceHeader) {
    return NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA;
  }

  // Sequence header syntax is specified here:
  // https://aomediacodec.github.io/av1-spec/#sequence-header-obu-syntax
  // Section 5.5: Sequence header OBU syntax

  // See also Section 6.4: Sequence header OBU semantics
  // https://aomediacodec.github.io/av1-spec/#sequence-header-obu-semantics
  // This section defines all the fields used in the sequence header.
  BitReader br(seqOBU.mContents.Elements(), seqOBU.mContents.Length() * 8);
  AV1SequenceInfo tempInfo;

  // begin sequence_header_obu( )
  // https://aomediacodec.github.io/av1-spec/#general-sequence-header-obu-syntax
  tempInfo.mProfile = br.ReadBits(3);
  const bool stillPicture = br.ReadBit();
  const bool reducedStillPicture = br.ReadBit();
  if (!stillPicture && reducedStillPicture) {
    return MediaResult(
        NS_ERROR_DOM_MEDIA_DECODE_ERR,
        "reduced_still_picture is true while still_picture is false");
  }

  if (reducedStillPicture) {
    OperatingPoint op;
    op.mLayers = 0;
    op.mLevel = br.ReadBits(5);  // seq_level_idx[0]
    op.mTier = 0;
    tempInfo.mOperatingPoints.SetCapacity(1);
    tempInfo.mOperatingPoints.AppendElement(op);
  } else {
    bool decoderModelInfoPresent;
    uint8_t operatingPointCountMinusOne;

    if (br.ReadBit()) {  // timing_info_present_flag
      // begin timing_info( )
      // https://aomediacodec.github.io/av1-spec/#timing-info-syntax
      br.ReadBits(32);     // num_units_in_display_tick
      br.ReadBits(32);     // time_scale
      if (br.ReadBit()) {  // equal_picture_interval
        br.ReadUE();       // num_ticks_per_picture_minus_1
      }
      // end timing_info( )

      decoderModelInfoPresent = br.ReadBit();
      if (decoderModelInfoPresent) {
        // begin decoder_model_info( )
        // https://aomediacodec.github.io/av1-spec/#decoder-model-info-syntax
        br.ReadBits(5);   // buffer_delay_length_minus_1
        br.ReadBits(32);  // num_units_in_decoding_tick
        br.ReadBits(5);   // buffer_removal_time_length_minus_1
        br.ReadBits(5);   // frame_presentation_time_length_minus_1
        // end decoder_model_info( )
      }
    } else {
      decoderModelInfoPresent = false;
    }

    bool initialDisplayDelayPresent = br.ReadBit();
    operatingPointCountMinusOne = br.ReadBits(5);
    tempInfo.mOperatingPoints.SetCapacity(operatingPointCountMinusOne + 1);
    for (uint8_t i = 0; i <= operatingPointCountMinusOne; i++) {
      OperatingPoint op;
      op.mLayers = br.ReadBits(12);  // operating_point_idc[ i ]
      op.mLevel = br.ReadBits(5);    // seq_level_idx[ i ]
      op.mTier = op.mLevel > 7 ? br.ReadBits(1) : 0;
      if (decoderModelInfoPresent) {
        br.ReadBit();  // decoder_model_present_for_this_op[ i ]
      }
      if (initialDisplayDelayPresent) {
        if (br.ReadBit()) {  // initial_display_delay_present_for_this_op[ i ]
          br.ReadBits(4);
        }
      }
      tempInfo.mOperatingPoints.AppendElement(op);
    }
  }

  uint8_t frameWidthBits = br.ReadBits(4) + 1;
  uint8_t frameHeightBits = br.ReadBits(4) + 1;
  uint32_t maxWidth = br.ReadBits(frameWidthBits) + 1;
  uint32_t maxHeight = br.ReadBits(frameHeightBits) + 1;
  tempInfo.mImage = gfx::IntSize(maxWidth, maxHeight);

  if (!reducedStillPicture) {
    if (br.ReadBit()) {  // frame_id_numbers_present_flag
      br.ReadBits(4);    // delta_frame_id_length_minus_2
      br.ReadBits(3);    // additional_frame_id_legnth_minus_1
    }
  }

  br.ReadBit();  // use_128x128_superblock
  br.ReadBit();  // enable_filter_intra
  br.ReadBit();  // enable_intra_edge_filter

  if (reducedStillPicture) {
    // enable_interintra_compound = 0
    // enable_masked_compound = 0
    // enable_warped_motion = 0
    // enable_dual_filter = 0
    // enable_order_hint = 0
    // enable_jnt_comp = 0
    // enable_ref_frame_mvs = 0
    // seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS
    // seq_force_integer_mv = SELECT_INTEGER_MV
    // OrderHintBits = 0
  } else {
    br.ReadBit();  // enable_interintra_compound
    br.ReadBit();  // enable_masked_compound
    br.ReadBit();  // enable_warped_motion
    br.ReadBit();  // enable_dual_filter

    const bool enableOrderHint = br.ReadBit();

    if (enableOrderHint) {
      br.ReadBit();  // enable_jnt_comp
      br.ReadBit();  // enable_ref_frame_mvs
    }

    uint8_t forceScreenContentTools;

    if (br.ReadBit()) {             // seq_choose_screen_content_tools
      forceScreenContentTools = 2;  // SELECT_SCREEN_CONTENT_TOOLS
    } else {
      forceScreenContentTools = br.ReadBits(1);
    }

    if (forceScreenContentTools > 0) {
      if (!br.ReadBit()) {  // seq_choose_integer_mv
        br.ReadBit();       // seq_force_integer_mv
      }
    }

    if (enableOrderHint) {
      br.ReadBits(3);  // order_hint_bits_minus_1
    }
  }

  br.ReadBit();  // enable_superres
  br.ReadBit();  // enable_cdef
  br.ReadBit();  // enable_restoration

  // begin color_config( )
  // https://aomediacodec.github.io/av1-spec/#color-config-syntax
  const bool highBitDepth = br.ReadBit();
  if (tempInfo.mProfile == 2 && highBitDepth) {
    const bool twelveBit = br.ReadBit();
    tempInfo.mBitDepth = twelveBit ? 12 : 10;
  } else {
    tempInfo.mBitDepth = highBitDepth ? 10 : 8;
  }

  tempInfo.mMonochrome = tempInfo.mProfile == 1 ? false : br.ReadBit();

  VideoColorSpace* colors = &tempInfo.mColorSpace;

  if (br.ReadBit()) {  // color_description_present_flag
    colors->mPrimaries = static_cast<ColourPrimaries>(br.ReadBits(8));
    colors->mTransfer = static_cast<TransferCharacteristics>(br.ReadBits(8));
    colors->mMatrix = static_cast<MatrixCoefficients>(br.ReadBits(8));
  } else {
    colors->mPrimaries = ColourPrimaries::CP_UNSPECIFIED;
    colors->mTransfer = TransferCharacteristics::TC_UNSPECIFIED;
    colors->mMatrix = MatrixCoefficients::MC_UNSPECIFIED;
  }

  if (tempInfo.mMonochrome) {
    colors->mRange = br.ReadBit() ? ColorRange::FULL : ColorRange::LIMITED;
    tempInfo.mSubsamplingX = true;
    tempInfo.mSubsamplingY = true;
    tempInfo.mChromaSamplePosition = ChromaSamplePosition::Unknown;
  } else if (colors->mPrimaries == ColourPrimaries::CP_BT709 &&
             colors->mTransfer == TransferCharacteristics::TC_SRGB &&
             colors->mMatrix == MatrixCoefficients::MC_IDENTITY) {
    colors->mRange = ColorRange::FULL;
    tempInfo.mSubsamplingX = false;
    tempInfo.mSubsamplingY = false;
  } else {
    colors->mRange = br.ReadBit() ? ColorRange::FULL : ColorRange::LIMITED;
    switch (tempInfo.mProfile) {
      case 0:
        tempInfo.mSubsamplingX = true;
        tempInfo.mSubsamplingY = true;
        break;
      case 1:
        tempInfo.mSubsamplingX = false;
        tempInfo.mSubsamplingY = false;
        break;
      case 2:
        if (tempInfo.mBitDepth == 12) {
          tempInfo.mSubsamplingX = br.ReadBit();
          tempInfo.mSubsamplingY =
              tempInfo.mSubsamplingX ? br.ReadBit() : false;
        } else {
          tempInfo.mSubsamplingX = true;
          tempInfo.mSubsamplingY = false;
        }
        break;
    }
    tempInfo.mChromaSamplePosition =
        tempInfo.mSubsamplingX && tempInfo.mSubsamplingY
            ? static_cast<ChromaSamplePosition>(br.ReadBits(2))
            : ChromaSamplePosition::Unknown;
  }

  br.ReadBit();  // separate_uv_delta_q
  // end color_config( )

  br.ReadBit();  // film_grain_params_present
  // end sequence_header_obu( )

  // begin trailing_bits( )
  // https://aomediacodec.github.io/av1-spec/#trailing-bits-syntax
  if (br.BitsLeft() > 8) {
    NS_WARNING(
        "AV1 sequence header finished reading with more than "
        "a byte of aligning bits, may indicate an error");
  }
  // Ensure that data is read correctly by checking trailing bits.
  bool correct = br.ReadBit();
  correct &= br.ReadBits(br.BitsLeft() % 8) == 0;
  while (br.BitsLeft() > 0) {
    correct &= br.ReadBits(8) == 0;
  }
  if (!correct) {
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       "AV1 sequence header was corrupted");
  }
  // end trailing_bits( )

  aDestInfo = tempInfo;
  return NS_OK;
}

/* static */
already_AddRefed<MediaByteBuffer> AOMDecoder::CreateSequenceHeader(
    const AV1SequenceInfo& aInfo, nsresult& aResult) {
  aResult = NS_ERROR_FAILURE;

  RefPtr<MediaByteBuffer> seqHdrBuffer = new MediaByteBuffer();
  BitWriter bw(seqHdrBuffer);

  // See 5.5.1: General sequence header OBU syntax
  // https://aomediacodec.github.io/av1-spec/#general-sequence-header-obu-syntax
  bw.WriteBits(aInfo.mProfile, 3);
  bw.WriteBit(false);  // still_picture
  bw.WriteBit(false);  // reduced_still_picture_header

  bw.WriteBit(false);  // timing_info_present_flag
  // if ( timing_info_present_flag ) {...}
  bw.WriteBit(false);  // initial_display_delay_present_flag

  size_t opCount = aInfo.mOperatingPoints.Length();
  bw.WriteBits(opCount - 1, 5);  // operating_points_cnt_minus_1
  for (size_t i = 0; i < opCount; i++) {
    OperatingPoint op = aInfo.mOperatingPoints[i];
    bw.WriteBits(op.mLayers, 12);  // operating_point_idc[ i ]
    bw.WriteBits(op.mLevel, 5);
    if (op.mLevel > 7) {
      bw.WriteBits(op.mTier, 1);
    } else {
      // seq_tier[ i ] = 0
      if (op.mTier != 0) {
        NS_WARNING("Operating points cannot specify tier for levels under 8.");
        return nullptr;
      }
    }
    // if ( decoder_model_info_present_flag ) {...}
    // else
    //   decoder_model_info_present_for_this_op[ i ] = 0
    // if ( initial_display_delay_present_flag ) {...}
  }

  if (aInfo.mImage.IsEmpty()) {
    NS_WARNING("Sequence header requires a valid image size");
    return nullptr;
  }
  auto getBits = [](int32_t value) {
    uint8_t bit = 0;
    do {
      value >>= 1;
      bit++;
    } while (value > 0);
    return bit;
  };
  uint8_t bitsW = getBits(aInfo.mImage.Width());
  uint8_t bitsH = getBits(aInfo.mImage.Height());
  bw.WriteBits(bitsW - 1, 4);
  bw.WriteBits(bitsH - 1, 4);
  bw.WriteBits(aInfo.mImage.Width() - 1, bitsW);
  bw.WriteBits(aInfo.mImage.Height() - 1, bitsH);

  // if ( !reduced_still_picture_header )
  bw.WriteBit(false);  // frame_id_numbers_present_flag
  // if ( frame_id_numbers_present_flag ) {...}
  // end if ( !reduced_still_picture_header )

  // Values below are derived from a 1080p YouTube AV1 stream.
  // The values are unused currently for determining the usable
  // decoder, and are only included to allow successful validation
  // of the generated sequence header.

  bw.WriteBit(true);  // use_128x128_superblock
  bw.WriteBit(true);  // enable_filter_intra
  bw.WriteBit(true);  // enable_intra_edge_filter

  // if ( !reduced_still_picture_header)
  bw.WriteBit(false);  // enable_interintra_compound
  bw.WriteBit(true);   // enable_masked_compound
  bw.WriteBit(true);   // enable_warped_motion
  bw.WriteBit(false);  // enable_dual_filter

  bw.WriteBit(true);  // enable_order_hint
  // if ( enable_order_hint )
  bw.WriteBit(false);  // enable_jnt_comp
  bw.WriteBit(true);   // enable_ref_frame_mvs
  // end if ( enable_order_hint )

  bw.WriteBit(true);  // seq_choose_screen_content_tools
  // if ( seq_choose_screen_content_tools )
  //   seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS (2)
  // else
  //   seq_force_screen_content_tools = f(1)

  // if ( seq_force_screen_content_tools > 0 )
  bw.WriteBit(true);  // seq_choose_integer_mv
  // if ( !seq_choose_integer_mv ) {...}
  // end if ( seq_force_screen_content_tools > 0 )

  // if ( enable_order_hint )
  bw.WriteBits(6, 3);  // order_hint_bits_minus_1
  // end if ( enable_order_hint )
  // end if ( !reduced_still_picture_header )

  bw.WriteBit(false);  // enable_superres
  bw.WriteBit(false);  // enable_cdef
  bw.WriteBit(true);   // enable_restoration

  // Begin color_config( )
  // https://aomediacodec.github.io/av1-spec/#color-config-syntax
  bool highBitDepth = aInfo.mBitDepth >= 10;
  bw.WriteBit(highBitDepth);

  if (aInfo.mBitDepth == 12 && aInfo.mProfile != 2) {
    NS_WARNING("Profile must be 2 for 12-bit");
    return nullptr;
  }
  if (aInfo.mProfile == 2 && highBitDepth) {
    bw.WriteBit(aInfo.mBitDepth == 12);  // twelve_bit
  }

  if (aInfo.mMonochrome && aInfo.mProfile == 1) {
    NS_WARNING("Profile 1 does not support monochrome");
    return nullptr;
  }
  if (aInfo.mProfile != 1) {
    bw.WriteBit(aInfo.mMonochrome);
  }

  const VideoColorSpace colors = aInfo.mColorSpace;
  bool colorsPresent =
      colors.mPrimaries != ColourPrimaries::CP_UNSPECIFIED ||
      colors.mTransfer != TransferCharacteristics::TC_UNSPECIFIED ||
      colors.mMatrix != MatrixCoefficients::MC_UNSPECIFIED;
  bw.WriteBit(colorsPresent);

  if (colorsPresent) {
    bw.WriteBits(static_cast<uint8_t>(colors.mPrimaries), 8);
    bw.WriteBits(static_cast<uint8_t>(colors.mTransfer), 8);
    bw.WriteBits(static_cast<uint8_t>(colors.mMatrix), 8);
  }

  if (aInfo.mMonochrome) {
    if (!aInfo.mSubsamplingX || !aInfo.mSubsamplingY) {
      NS_WARNING("Monochrome requires 4:0:0 subsampling");
      return nullptr;
    }
    if (aInfo.mChromaSamplePosition != ChromaSamplePosition::Unknown) {
      NS_WARNING(
          "Cannot specify chroma sample position on monochrome sequence");
      return nullptr;
    }
    bw.WriteBit(colors.mRange == ColorRange::FULL);
  } else if (colors.mPrimaries == ColourPrimaries::CP_BT709 &&
             colors.mTransfer == TransferCharacteristics::TC_SRGB &&
             colors.mMatrix == MatrixCoefficients::MC_IDENTITY) {
    if (aInfo.mSubsamplingX || aInfo.mSubsamplingY ||
        colors.mRange != ColorRange::FULL ||
        aInfo.mChromaSamplePosition != ChromaSamplePosition::Unknown) {
      NS_WARNING("sRGB requires 4:4:4 subsampling with full color range");
      return nullptr;
    }
  } else {
    bw.WriteBit(colors.mRange == ColorRange::FULL);
    switch (aInfo.mProfile) {
      case 0:
        if (!aInfo.mSubsamplingX || !aInfo.mSubsamplingY) {
          NS_WARNING("Main Profile requires 4:2:0 subsampling");
          return nullptr;
        }
        break;
      case 1:
        if (aInfo.mSubsamplingX || aInfo.mSubsamplingY) {
          NS_WARNING("High Profile requires 4:4:4 subsampling");
          return nullptr;
        }
        break;
      case 2:
        if (aInfo.mBitDepth == 12) {
          bw.WriteBit(aInfo.mSubsamplingX);
          if (aInfo.mSubsamplingX) {
            bw.WriteBit(aInfo.mSubsamplingY);
          }
        } else {
          if (!aInfo.mSubsamplingX || aInfo.mSubsamplingY) {
            NS_WARNING(
                "Professional Profile < 12-bit requires 4:2:2 subsampling");
            return nullptr;
          }
        }
        break;
    }

    if (aInfo.mSubsamplingX && aInfo.mSubsamplingY) {
      bw.WriteBits(static_cast<uint8_t>(aInfo.mChromaSamplePosition), 2);
    } else {
      if (aInfo.mChromaSamplePosition != ChromaSamplePosition::Unknown) {
        NS_WARNING("Only 4:2:0 subsampling can specify chroma position");
        return nullptr;
      }
    }
  }

  bw.WriteBit(false);  // separate_uv_delta_q
  // end color_config( )

  bw.WriteBit(true);  // film_grain_params_present

  // trailing_bits( )
  // https://aomediacodec.github.io/av1-spec/#trailing-bits-syntax
  size_t numTrailingBits = 8 - (bw.BitCount() % 8);
  bw.WriteBit(true);
  bw.WriteBits(0, numTrailingBits - 1);
  ASSERT_BYTE_ALIGNED(bw);

  Span<const uint8_t> seqHdr(seqHdrBuffer->Elements(), seqHdrBuffer->Length());
  aResult = NS_OK;
  return CreateOBU(OBUType::SequenceHeader, seqHdr);
}

/* static */
void AOMDecoder::TryReadAV1CBox(const MediaByteBuffer* aBox,
                                AV1SequenceInfo& aDestInfo,
                                MediaResult& aSeqHdrResult) {
  // See av1C specification:
  // https://aomediacodec.github.io/av1-isobmff/#av1codecconfigurationbox-section
  BitReader br(aBox);

  br.ReadBits(8);  // marker, version

  aDestInfo.mProfile = br.ReadBits(3);

  OperatingPoint op;
  op.mLevel = br.ReadBits(5);
  op.mTier = br.ReadBits(1);
  aDestInfo.mOperatingPoints.AppendElement(op);

  bool highBitDepth = br.ReadBit();
  bool twelveBit = br.ReadBit();
  aDestInfo.mBitDepth = highBitDepth ? twelveBit ? 12 : 10 : 8;

  aDestInfo.mMonochrome = br.ReadBit();
  aDestInfo.mSubsamplingX = br.ReadBit();
  aDestInfo.mSubsamplingY = br.ReadBit();
  aDestInfo.mChromaSamplePosition =
      static_cast<ChromaSamplePosition>(br.ReadBits(2));

  br.ReadBits(3);  // reserved
  br.ReadBit();    // initial_presentation_delay_present
  br.ReadBits(4);  // initial_presentation_delay_minus_one or reserved

  ASSERT_BYTE_ALIGNED(br);

  size_t skipBytes = br.BitCount() / 8;
  Span<const uint8_t> obus(aBox->Elements() + skipBytes,
                           aBox->Length() - skipBytes);

  // Minimum possible OBU header size
  if (obus.Length() < 1) {
    aSeqHdrResult = NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA;
    return;
  }

  // If present, the sequence header will be redundant to some values, but any
  // values stored in it should be treated as more accurate than av1C.
  aSeqHdrResult = ReadSequenceHeaderInfo(obus, aDestInfo);
}

/* static */
void AOMDecoder::WriteAV1CBox(const AV1SequenceInfo& aInfo,
                              MediaByteBuffer* aDestBox, bool& aHasSeqHdr) {
  aHasSeqHdr = false;

  BitWriter bw(aDestBox);

  bw.WriteBit(true);   // marker
  bw.WriteBits(1, 7);  // version

  bw.WriteBits(aInfo.mProfile, 3);

  MOZ_DIAGNOSTIC_ASSERT(aInfo.mOperatingPoints.Length() > 0);
  bw.WriteBits(aInfo.mOperatingPoints[0].mLevel, 5);
  bw.WriteBits(aInfo.mOperatingPoints[0].mTier, 1);

  bw.WriteBit(aInfo.mBitDepth >= 10);  // high_bitdepth
  bw.WriteBit(aInfo.mBitDepth == 12);  // twelve_bit

  bw.WriteBit(aInfo.mMonochrome);
  bw.WriteBit(aInfo.mSubsamplingX);
  bw.WriteBit(aInfo.mSubsamplingY);
  bw.WriteBits(static_cast<uint8_t>(aInfo.mChromaSamplePosition), 2);

  bw.WriteBits(0, 3);  // reserved
  bw.WriteBit(false);  // initial_presentation_delay_present
  bw.WriteBits(0, 4);  // initial_presentation_delay_minus_one or reserved

  ASSERT_BYTE_ALIGNED(bw);

  nsresult rv;
  RefPtr<MediaByteBuffer> seqHdrBuffer = CreateSequenceHeader(aInfo, rv);

  if (NS_SUCCEEDED(rv)) {
    aDestBox->AppendElements(seqHdrBuffer->Elements(), seqHdrBuffer->Length());
    aHasSeqHdr = true;
  }
}

/* static */
Maybe<AOMDecoder::AV1SequenceInfo> AOMDecoder::CreateSequenceInfoFromCodecs(
    const nsAString& aCodec) {
  AV1SequenceInfo info;
  OperatingPoint op;
  uint8_t chromaSamplePosition;
  if (!ExtractAV1CodecDetails(aCodec, info.mProfile, op.mLevel, op.mTier,
                              info.mBitDepth, info.mMonochrome,
                              info.mSubsamplingX, info.mSubsamplingY,
                              chromaSamplePosition, info.mColorSpace)) {
    return Nothing();
  }
  info.mOperatingPoints.AppendElement(op);
  info.mChromaSamplePosition =
      static_cast<ChromaSamplePosition>(chromaSamplePosition);
  return Some(info);
}

/* static */
bool AOMDecoder::SetVideoInfo(VideoInfo* aDestInfo, const nsAString& aCodec) {
  Maybe<AV1SequenceInfo> info = CreateSequenceInfoFromCodecs(aCodec);
  if (info.isNothing()) {
    return false;
  }

  if (!aDestInfo->mImage.IsEmpty()) {
    info->mImage = aDestInfo->mImage;
  }

  RefPtr<MediaByteBuffer> extraData = new MediaByteBuffer();
  bool hasSeqHdr;
  WriteAV1CBox(info.value(), extraData, hasSeqHdr);
  aDestInfo->mExtraData = extraData;
  return true;
}

}  // namespace mozilla
#undef LOG
#undef ASSERT_BYTE_ALIGNED
