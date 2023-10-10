/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLogging.h"  // Must appear first

#include "nsAVIFDecoder.h"

#include "aom/aomdx.h"

#include "DAV1DDecoder.h"
#include "gfxPlatform.h"
#include "YCbCrUtils.h"
#include "libyuv.h"

#include "SurfacePipeFactory.h"

#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryComms.h"

using namespace mozilla::gfx;

namespace mozilla {

namespace image {

using Telemetry::LABELS_AVIF_A1LX;
using Telemetry::LABELS_AVIF_A1OP;
using Telemetry::LABELS_AVIF_ALPHA;
using Telemetry::LABELS_AVIF_AOM_DECODE_ERROR;
using Telemetry::LABELS_AVIF_BIT_DEPTH;
using Telemetry::LABELS_AVIF_CICP_CP;
using Telemetry::LABELS_AVIF_CICP_MC;
using Telemetry::LABELS_AVIF_CICP_TC;
using Telemetry::LABELS_AVIF_CLAP;
using Telemetry::LABELS_AVIF_COLR;
using Telemetry::LABELS_AVIF_DECODE_RESULT;
using Telemetry::LABELS_AVIF_DECODER;
using Telemetry::LABELS_AVIF_GRID;
using Telemetry::LABELS_AVIF_IPRO;
using Telemetry::LABELS_AVIF_ISPE;
using Telemetry::LABELS_AVIF_LSEL;
using Telemetry::LABELS_AVIF_MAJOR_BRAND;
using Telemetry::LABELS_AVIF_PASP;
using Telemetry::LABELS_AVIF_PIXI;
using Telemetry::LABELS_AVIF_SEQUENCE;
using Telemetry::LABELS_AVIF_YUV_COLOR_SPACE;

static LazyLogModule sAVIFLog("AVIFDecoder");

static const LABELS_AVIF_BIT_DEPTH gColorDepthLabel[] = {
    LABELS_AVIF_BIT_DEPTH::color_8, LABELS_AVIF_BIT_DEPTH::color_10,
    LABELS_AVIF_BIT_DEPTH::color_12, LABELS_AVIF_BIT_DEPTH::color_16};

static const LABELS_AVIF_YUV_COLOR_SPACE gColorSpaceLabel[] = {
    LABELS_AVIF_YUV_COLOR_SPACE::BT601, LABELS_AVIF_YUV_COLOR_SPACE::BT709,
    LABELS_AVIF_YUV_COLOR_SPACE::BT2020, LABELS_AVIF_YUV_COLOR_SPACE::identity};

static MaybeIntSize GetImageSize(const Mp4parseAvifInfo& aInfo) {
  // Note this does not take cropping via CleanAperture (clap) into account
  const struct Mp4parseImageSpatialExtents* ispe = aInfo.spatial_extents;

  if (ispe) {
    // Decoder::PostSize takes int32_t, but ispe contains uint32_t
    CheckedInt<int32_t> width = ispe->image_width;
    CheckedInt<int32_t> height = ispe->image_height;

    if (width.isValid() && height.isValid()) {
      return Some(IntSize{width.value(), height.value()});
    }
  }

  return Nothing();
}

// Translate the MIAF/HEIF-based orientation transforms (imir, irot) into
// ImageLib's representation. Note that the interpretation of imir was reversed
// Between HEIF (ISO 23008-12:2017) and ISO/IEC 23008-12:2017/DAmd 2. This is
// handled by mp4parse. See mp4parse::read_imir for details.
Orientation GetImageOrientation(const Mp4parseAvifInfo& aInfo) {
  // Per MIAF (ISO/IEC 23000-22:2019) § 7.3.6.7
  //   These properties, if used, shall be indicated to be applied in the
  //   following order: clean aperture first, then rotation, then mirror.
  // The Orientation type does the same order, but opposite rotation direction

  const Mp4parseIrot heifRot = aInfo.image_rotation;
  const Mp4parseImir* heifMir = aInfo.image_mirror;
  Angle mozRot;
  Flip mozFlip;

  if (!heifMir) {  // No mirroring
    mozFlip = Flip::Unflipped;

    switch (heifRot) {
      case MP4PARSE_IROT_D0:
        // ⥠ UPWARDS HARPOON WITH BARB LEFT FROM BAR
        mozRot = Angle::D0;
        break;
      case MP4PARSE_IROT_D90:
        // ⥞ LEFTWARDS HARPOON WITH BARB DOWN FROM BAR
        mozRot = Angle::D270;
        break;
      case MP4PARSE_IROT_D180:
        // ⥝ DOWNWARDS HARPOON WITH BARB RIGHT FROM BAR
        mozRot = Angle::D180;
        break;
      case MP4PARSE_IROT_D270:
        // ⥛  RIGHTWARDS HARPOON WITH BARB UP FROM BAR
        mozRot = Angle::D90;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE();
    }
  } else {
    MOZ_ASSERT(heifMir);
    mozFlip = Flip::Horizontal;

    enum class HeifFlippedOrientation : uint8_t {
      IROT_D0_IMIR_V = (MP4PARSE_IROT_D0 << 1) | MP4PARSE_IMIR_LEFT_RIGHT,
      IROT_D0_IMIR_H = (MP4PARSE_IROT_D0 << 1) | MP4PARSE_IMIR_TOP_BOTTOM,
      IROT_D90_IMIR_V = (MP4PARSE_IROT_D90 << 1) | MP4PARSE_IMIR_LEFT_RIGHT,
      IROT_D90_IMIR_H = (MP4PARSE_IROT_D90 << 1) | MP4PARSE_IMIR_TOP_BOTTOM,
      IROT_D180_IMIR_V = (MP4PARSE_IROT_D180 << 1) | MP4PARSE_IMIR_LEFT_RIGHT,
      IROT_D180_IMIR_H = (MP4PARSE_IROT_D180 << 1) | MP4PARSE_IMIR_TOP_BOTTOM,
      IROT_D270_IMIR_V = (MP4PARSE_IROT_D270 << 1) | MP4PARSE_IMIR_LEFT_RIGHT,
      IROT_D270_IMIR_H = (MP4PARSE_IROT_D270 << 1) | MP4PARSE_IMIR_TOP_BOTTOM,
    };

    HeifFlippedOrientation heifO =
        HeifFlippedOrientation((heifRot << 1) | *heifMir);

    switch (heifO) {
      case HeifFlippedOrientation::IROT_D0_IMIR_V:
      case HeifFlippedOrientation::IROT_D180_IMIR_H:
        // ⥜ UPWARDS HARPOON WITH BARB RIGHT FROM BAR
        mozRot = Angle::D0;
        break;
      case HeifFlippedOrientation::IROT_D270_IMIR_V:
      case HeifFlippedOrientation::IROT_D90_IMIR_H:
        // ⥚ LEFTWARDS HARPOON WITH BARB UP FROM BAR
        mozRot = Angle::D90;
        break;
      case HeifFlippedOrientation::IROT_D180_IMIR_V:
      case HeifFlippedOrientation::IROT_D0_IMIR_H:
        // ⥡ DOWNWARDS HARPOON WITH BARB LEFT FROM BAR
        mozRot = Angle::D180;
        break;
      case HeifFlippedOrientation::IROT_D90_IMIR_V:
      case HeifFlippedOrientation::IROT_D270_IMIR_H:
        // ⥟ RIGHTWARDS HARPOON WITH BARB DOWN FROM BAR
        mozRot = Angle::D270;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE();
    }
  }

  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("GetImageOrientation: (rot%d, imir(%s)) -> (Angle%d, "
           "Flip%d)",
           static_cast<int>(heifRot),
           heifMir ? (*heifMir == MP4PARSE_IMIR_LEFT_RIGHT ? "left-right"
                                                           : "top-bottom")
                   : "none",
           static_cast<int>(mozRot), static_cast<int>(mozFlip)));
  return Orientation{mozRot, mozFlip};
}
bool AVIFDecoderStream::ReadAt(int64_t offset, void* data, size_t size,
                               size_t* bytes_read) {
  size = std::min(size, size_t(mBuffer->length() - offset));

  if (size <= 0) {
    return false;
  }

  memcpy(data, mBuffer->begin() + offset, size);
  *bytes_read = size;
  return true;
}

bool AVIFDecoderStream::Length(int64_t* size) {
  *size =
      static_cast<int64_t>(std::min<uint64_t>(mBuffer->length(), INT64_MAX));
  return true;
}

const uint8_t* AVIFDecoderStream::GetContiguousAccess(int64_t aOffset,
                                                      size_t aSize) {
  if (aOffset + aSize >= mBuffer->length()) {
    return nullptr;
  }

  return mBuffer->begin() + aOffset;
}

AVIFParser::~AVIFParser() {
  MOZ_LOG(sAVIFLog, LogLevel::Debug, ("Destroy AVIFParser=%p", this));
}

Mp4parseStatus AVIFParser::Create(const Mp4parseIo* aIo, ByteStream* aBuffer,
                                  UniquePtr<AVIFParser>& aParserOut,
                                  bool aAllowSequences,
                                  bool aAnimateAVIFMajor) {
  MOZ_ASSERT(aIo);
  MOZ_ASSERT(!aParserOut);

  UniquePtr<AVIFParser> p(new AVIFParser(aIo));
  Mp4parseStatus status = p->Init(aBuffer, aAllowSequences, aAnimateAVIFMajor);

  if (status == MP4PARSE_STATUS_OK) {
    MOZ_ASSERT(p->mParser);
    aParserOut = std::move(p);
  }

  return status;
}

nsAVIFDecoder::DecodeResult AVIFParser::GetImage(AVIFImage& aImage) {
  MOZ_ASSERT(mParser);

  // If the AVIF is animated, get next frame and yield if sequence is not done.
  if (IsAnimated()) {
    aImage.mColorImage = mColorSampleIter->GetNext();

    if (!aImage.mColorImage) {
      return AsVariant(nsAVIFDecoder::NonDecoderResult::NoSamples);
    }

    aImage.mFrameNum = mFrameNum++;
    int64_t durationMs = aImage.mColorImage->mDuration.ToMilliseconds();
    aImage.mDuration = FrameTimeout::FromRawMilliseconds(
        static_cast<int32_t>(std::min<int64_t>(durationMs, INT32_MAX)));

    if (mAlphaSampleIter) {
      aImage.mAlphaImage = mAlphaSampleIter->GetNext();
      if (!aImage.mAlphaImage) {
        return AsVariant(nsAVIFDecoder::NonDecoderResult::NoSamples);
      }
    }

    bool hasNext = mColorSampleIter->HasNext();
    if (mAlphaSampleIter && (hasNext != mAlphaSampleIter->HasNext())) {
      MOZ_LOG(
          sAVIFLog, LogLevel::Warning,
          ("[this=%p] The %s sequence ends before frame %d, aborting decode.",
           this, hasNext ? "alpha" : "color", mFrameNum));
      return AsVariant(nsAVIFDecoder::NonDecoderResult::NoSamples);
    }
    if (!hasNext) {
      return AsVariant(nsAVIFDecoder::NonDecoderResult::Complete);
    }
    return AsVariant(nsAVIFDecoder::NonDecoderResult::OutputAvailable);
  }

  if (!mInfo.has_primary_item) {
    return AsVariant(nsAVIFDecoder::NonDecoderResult::NoSamples);
  }

  // If the AVIF is not animated, get the pitm image and return Complete.
  Mp4parseAvifImage image = {};
  Mp4parseStatus status = mp4parse_avif_get_image(mParser.get(), &image);
  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] mp4parse_avif_get_image -> %d; primary_item length: "
           "%zu, alpha_item length: %zu",
           this, status, image.primary_image.length, image.alpha_image.length));
  if (status != MP4PARSE_STATUS_OK) {
    return AsVariant(status);
  }

  // Ideally has_primary_item and no errors would guarantee primary_image.data
  // exists but it doesn't so we check it too.
  if (!image.primary_image.data) {
    return AsVariant(nsAVIFDecoder::NonDecoderResult::NoSamples);
  }

  RefPtr<MediaRawData> colorImage =
      new MediaRawData(image.primary_image.data, image.primary_image.length);
  RefPtr<MediaRawData> alphaImage = nullptr;

  if (image.alpha_image.length) {
    alphaImage =
        new MediaRawData(image.alpha_image.data, image.alpha_image.length);
  }

  aImage.mFrameNum = 0;
  aImage.mDuration = FrameTimeout::Forever();
  aImage.mColorImage = colorImage;
  aImage.mAlphaImage = alphaImage;
  return AsVariant(nsAVIFDecoder::NonDecoderResult::Complete);
}

AVIFParser::AVIFParser(const Mp4parseIo* aIo) : mIo(aIo) {
  MOZ_ASSERT(mIo);
  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("Create AVIFParser=%p, image.avif.compliance_strictness: %d", this,
           StaticPrefs::image_avif_compliance_strictness()));
}

static Mp4parseStatus CreateSampleIterator(
    Mp4parseAvifParser* aParser, ByteStream* aBuffer, uint32_t trackID,
    UniquePtr<SampleIterator>& aIteratorOut) {
  Mp4parseByteData data;
  uint64_t timescale;
  Mp4parseStatus rv =
      mp4parse_avif_get_indice_table(aParser, trackID, &data, &timescale);
  if (rv != MP4PARSE_STATUS_OK) {
    return rv;
  }

  UniquePtr<IndiceWrapper> wrapper = MakeUnique<IndiceWrapper>(data);
  RefPtr<MP4SampleIndex> index = new MP4SampleIndex(
      *wrapper, aBuffer, trackID, false, AssertedCast<int32_t>(timescale));
  aIteratorOut = MakeUnique<SampleIterator>(index);
  return MP4PARSE_STATUS_OK;
}

Mp4parseStatus AVIFParser::Init(ByteStream* aBuffer, bool aAllowSequences,
                                bool aAnimateAVIFMajor) {
#define CHECK_MP4PARSE_STATUS(v)     \
  do {                               \
    if ((v) != MP4PARSE_STATUS_OK) { \
      return v;                      \
    }                                \
  } while (false)

  MOZ_ASSERT(!mParser);

  Mp4parseAvifParser* parser = nullptr;
  Mp4parseStatus status =
      mp4parse_avif_new(mIo,
                        static_cast<enum Mp4parseStrictness>(
                            StaticPrefs::image_avif_compliance_strictness()),
                        &parser);
  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] mp4parse_avif_new status: %d", this, status));
  CHECK_MP4PARSE_STATUS(status);
  MOZ_ASSERT(parser);
  mParser.reset(parser);

  status = mp4parse_avif_get_info(mParser.get(), &mInfo);
  CHECK_MP4PARSE_STATUS(status);

  bool useSequence = mInfo.has_sequence;
  if (useSequence) {
    if (!aAllowSequences) {
      MOZ_LOG(sAVIFLog, LogLevel::Debug,
              ("[this=%p] AVIF sequences disabled", this));
      useSequence = false;
    } else if (!aAnimateAVIFMajor &&
               !!memcmp(mInfo.major_brand, "avis", sizeof(mInfo.major_brand))) {
      useSequence = false;
      MOZ_LOG(sAVIFLog, LogLevel::Debug,
              ("[this=%p] AVIF prefers still image", this));
    }
  }

  if (useSequence) {
    status = CreateSampleIterator(parser, aBuffer, mInfo.color_track_id,
                                  mColorSampleIter);
    CHECK_MP4PARSE_STATUS(status);
    MOZ_ASSERT(mColorSampleIter);

    if (mInfo.alpha_track_id) {
      status = CreateSampleIterator(parser, aBuffer, mInfo.alpha_track_id,
                                    mAlphaSampleIter);
      CHECK_MP4PARSE_STATUS(status);
      MOZ_ASSERT(mAlphaSampleIter);
    }
  }

  return status;
}

bool AVIFParser::IsAnimated() const { return !!mColorSampleIter; }

// The gfx::YUVColorSpace value is only used in the conversion from YUV -> RGB.
// Typically this comes directly from the CICP matrix_coefficients value, but
// certain values require additionally considering the colour_primaries value.
// See `gfxUtils::CicpToColorSpace` for details. We return a gfx::YUVColorSpace
// rather than CICP::MatrixCoefficients, since that's what
// `gfx::ConvertYCbCrATo[A]RGB` uses. `aBitstreamColorSpaceFunc` abstracts the
// fact that different decoder libraries require different methods for
// extracting the CICP values from the AV1 bitstream and we don't want to do
// that work unnecessarily because in addition to wasted effort, it would make
// the logging more confusing.
template <typename F>
static gfx::YUVColorSpace GetAVIFColorSpace(
    const Mp4parseNclxColourInformation* aNclx, F&& aBitstreamColorSpaceFunc) {
  return ToMaybe(aNclx)
      .map([=](const auto& nclx) {
        return gfxUtils::CicpToColorSpace(
            static_cast<CICP::MatrixCoefficients>(nclx.matrix_coefficients),
            static_cast<CICP::ColourPrimaries>(nclx.colour_primaries),
            sAVIFLog);
      })
      .valueOrFrom(aBitstreamColorSpaceFunc)
      .valueOr(gfx::YUVColorSpace::BT601);
}

static gfx::ColorRange GetAVIFColorRange(
    const Mp4parseNclxColourInformation* aNclx,
    const gfx::ColorRange av1ColorRange) {
  return ToMaybe(aNclx)
      .map([=](const auto& nclx) {
        return aNclx->full_range_flag ? gfx::ColorRange::FULL
                                      : gfx::ColorRange::LIMITED;
      })
      .valueOr(av1ColorRange);
}

void AVIFDecodedData::SetCicpValues(
    const Mp4parseNclxColourInformation* aNclx,
    const gfx::CICP::ColourPrimaries aAv1ColourPrimaries,
    const gfx::CICP::TransferCharacteristics aAv1TransferCharacteristics,
    const gfx::CICP::MatrixCoefficients aAv1MatrixCoefficients) {
  auto cp = CICP::ColourPrimaries::CP_UNSPECIFIED;
  auto tc = CICP::TransferCharacteristics::TC_UNSPECIFIED;
  auto mc = CICP::MatrixCoefficients::MC_UNSPECIFIED;

  if (aNclx) {
    cp = static_cast<CICP::ColourPrimaries>(aNclx->colour_primaries);
    tc = static_cast<CICP::TransferCharacteristics>(
        aNclx->transfer_characteristics);
    mc = static_cast<CICP::MatrixCoefficients>(aNclx->matrix_coefficients);
  }

  if (cp == CICP::ColourPrimaries::CP_UNSPECIFIED) {
    if (aAv1ColourPrimaries != CICP::ColourPrimaries::CP_UNSPECIFIED) {
      cp = aAv1ColourPrimaries;
      MOZ_LOG(sAVIFLog, LogLevel::Info,
              ("Unspecified colour_primaries value specified in colr box, "
               "using AV1 sequence header (%hhu)",
               cp));
    } else {
      cp = CICP::ColourPrimaries::CP_BT709;
      MOZ_LOG(sAVIFLog, LogLevel::Warning,
              ("Unspecified colour_primaries value specified in colr box "
               "or AV1 sequence header, using fallback value (%hhu)",
               cp));
    }
  } else if (cp != aAv1ColourPrimaries) {
    MOZ_LOG(sAVIFLog, LogLevel::Warning,
            ("colour_primaries mismatch: colr box = %hhu, AV1 "
             "sequence header = %hhu, using colr box",
             cp, aAv1ColourPrimaries));
  }

  if (tc == CICP::TransferCharacteristics::TC_UNSPECIFIED) {
    if (aAv1TransferCharacteristics !=
        CICP::TransferCharacteristics::TC_UNSPECIFIED) {
      tc = aAv1TransferCharacteristics;
      MOZ_LOG(sAVIFLog, LogLevel::Info,
              ("Unspecified transfer_characteristics value specified in "
               "colr box, using AV1 sequence header (%hhu)",
               tc));
    } else {
      tc = CICP::TransferCharacteristics::TC_SRGB;
      MOZ_LOG(sAVIFLog, LogLevel::Warning,
              ("Unspecified transfer_characteristics value specified in "
               "colr box or AV1 sequence header, using fallback value (%hhu)",
               tc));
    }
  } else if (tc != aAv1TransferCharacteristics) {
    MOZ_LOG(sAVIFLog, LogLevel::Warning,
            ("transfer_characteristics mismatch: colr box = %hhu, "
             "AV1 sequence header = %hhu, using colr box",
             tc, aAv1TransferCharacteristics));
  }

  if (mc == CICP::MatrixCoefficients::MC_UNSPECIFIED) {
    if (aAv1MatrixCoefficients != CICP::MatrixCoefficients::MC_UNSPECIFIED) {
      mc = aAv1MatrixCoefficients;
      MOZ_LOG(sAVIFLog, LogLevel::Info,
              ("Unspecified matrix_coefficients value specified in "
               "colr box, using AV1 sequence header (%hhu)",
               mc));
    } else {
      mc = CICP::MatrixCoefficients::MC_BT601;
      MOZ_LOG(sAVIFLog, LogLevel::Warning,
              ("Unspecified matrix_coefficients value specified in "
               "colr box or AV1 sequence header, using fallback value (%hhu)",
               mc));
    }
  } else if (mc != aAv1MatrixCoefficients) {
    MOZ_LOG(sAVIFLog, LogLevel::Warning,
            ("matrix_coefficients mismatch: colr box = %hhu, "
             "AV1 sequence header = %hhu, using colr box",
             mc, aAv1TransferCharacteristics));
  }

  mColourPrimaries = cp;
  mTransferCharacteristics = tc;
  mMatrixCoefficients = mc;
}

class Dav1dDecoder final : AVIFDecoderInterface {
 public:
  ~Dav1dDecoder() {
    MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("Destroy Dav1dDecoder=%p", this));

    if (mColorContext) {
      dav1d_close(&mColorContext);
      MOZ_ASSERT(!mColorContext);
    }

    if (mAlphaContext) {
      dav1d_close(&mAlphaContext);
      MOZ_ASSERT(!mAlphaContext);
    }
  }

  static DecodeResult Create(UniquePtr<AVIFDecoderInterface>& aDecoder,
                             bool aHasAlpha) {
    UniquePtr<Dav1dDecoder> d(new Dav1dDecoder());
    Dav1dResult r = d->Init(aHasAlpha);
    if (r == 0) {
      aDecoder.reset(d.release());
    }
    return AsVariant(r);
  }

  DecodeResult Decode(bool aShouldSendTelemetry,
                      const Mp4parseAvifInfo& aAVIFInfo,
                      const AVIFImage& aSamples) override {
    MOZ_ASSERT(mColorContext);
    MOZ_ASSERT(!mDecodedData);
    MOZ_ASSERT(aSamples.mColorImage);

    MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("[this=%p] Decoding color", this));

    OwnedDav1dPicture colorPic = OwnedDav1dPicture(new Dav1dPicture());
    OwnedDav1dPicture alphaPic = nullptr;
    Dav1dResult r = GetPicture(*mColorContext, *aSamples.mColorImage,
                               colorPic.get(), aShouldSendTelemetry);
    if (r != 0) {
      return AsVariant(r);
    }

    if (aSamples.mAlphaImage) {
      MOZ_ASSERT(mAlphaContext);
      MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("[this=%p] Decoding alpha", this));

      alphaPic = OwnedDav1dPicture(new Dav1dPicture());
      r = GetPicture(*mAlphaContext, *aSamples.mAlphaImage, alphaPic.get(),
                     aShouldSendTelemetry);
      if (r != 0) {
        return AsVariant(r);
      }

      // Per § 4 of the AVIF spec
      // https://aomediacodec.github.io/av1-avif/#auxiliary-images: An AV1
      // Alpha Image Item […] shall be encoded with the same bit depth as the
      // associated master AV1 Image Item
      if (colorPic->p.bpc != alphaPic->p.bpc) {
        return AsVariant(NonDecoderResult::AlphaYColorDepthMismatch);
      }

      if (colorPic->stride[0] != alphaPic->stride[0]) {
        return AsVariant(NonDecoderResult::AlphaYSizeMismatch);
      }
    }

    MOZ_ASSERT_IF(!alphaPic, !aAVIFInfo.premultiplied_alpha);
    mDecodedData = Dav1dPictureToDecodedData(
        aAVIFInfo.nclx_colour_information, std::move(colorPic),
        std::move(alphaPic), aAVIFInfo.premultiplied_alpha);

    return AsVariant(r);
  }

 private:
  explicit Dav1dDecoder() {
    MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("Create Dav1dDecoder=%p", this));
  }

  Dav1dResult Init(bool aHasAlpha) {
    MOZ_ASSERT(!mColorContext);
    MOZ_ASSERT(!mAlphaContext);

    Dav1dSettings settings;
    dav1d_default_settings(&settings);
    settings.all_layers = 0;
    settings.max_frame_delay = 1;
    // TODO: tune settings a la DAV1DDecoder for AV1 (Bug 1681816)

    Dav1dResult r = dav1d_open(&mColorContext, &settings);
    if (r != 0) {
      return r;
    }
    MOZ_ASSERT(mColorContext);

    if (aHasAlpha) {
      r = dav1d_open(&mAlphaContext, &settings);
      if (r != 0) {
        return r;
      }
      MOZ_ASSERT(mAlphaContext);
    }

    return 0;
  }

  static Dav1dResult GetPicture(Dav1dContext& aContext,
                                const MediaRawData& aBytes,
                                Dav1dPicture* aPicture,
                                bool aShouldSendTelemetry) {
    MOZ_ASSERT(aPicture);

    Dav1dData dav1dData;
    Dav1dResult r = dav1d_data_wrap(&dav1dData, aBytes.Data(), aBytes.Size(),
                                    Dav1dFreeCallback_s, nullptr);

    MOZ_LOG(
        sAVIFLog, r == 0 ? LogLevel::Verbose : LogLevel::Error,
        ("dav1d_data_wrap(%p, %zu) -> %d", dav1dData.data, dav1dData.sz, r));

    if (r != 0) {
      return r;
    }

    r = dav1d_send_data(&aContext, &dav1dData);

    MOZ_LOG(sAVIFLog, r == 0 ? LogLevel::Debug : LogLevel::Error,
            ("dav1d_send_data -> %d", r));

    if (r != 0) {
      return r;
    }

    r = dav1d_get_picture(&aContext, aPicture);

    MOZ_LOG(sAVIFLog, r == 0 ? LogLevel::Debug : LogLevel::Error,
            ("dav1d_get_picture -> %d", r));

    // We already have the AVIF_DECODE_RESULT histogram to record all the
    // successful calls, so only bother recording what type of errors we see
    // via events. Unlike AOM, dav1d returns an int, not an enum, so this is
    // the easiest way to see if we're getting unexpected behavior to
    // investigate.
    if (aShouldSendTelemetry && r != 0) {
      // Uncomment once bug 1691156 is fixed
      // mozilla::Telemetry::SetEventRecordingEnabled("avif"_ns, true);

      mozilla::Telemetry::RecordEvent(
          mozilla::Telemetry::EventID::Avif_Dav1dGetPicture_ReturnValue,
          Some(nsPrintfCString("%d", r)), Nothing());
    }

    return r;
  }

  // A dummy callback for dav1d_data_wrap
  static void Dav1dFreeCallback_s(const uint8_t* aBuf, void* aCookie) {
    // The buf is managed by the mParser inside Dav1dDecoder itself. Do
    // nothing here.
  }

  static UniquePtr<AVIFDecodedData> Dav1dPictureToDecodedData(
      const Mp4parseNclxColourInformation* aNclx, OwnedDav1dPicture aPicture,
      OwnedDav1dPicture aAlphaPlane, bool aPremultipliedAlpha);

  Dav1dContext* mColorContext = nullptr;
  Dav1dContext* mAlphaContext = nullptr;
};

OwnedAOMImage::OwnedAOMImage() {
  MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("Create OwnedAOMImage=%p", this));
}

OwnedAOMImage::~OwnedAOMImage() {
  MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("Destroy OwnedAOMImage=%p", this));
}

bool OwnedAOMImage::CloneFrom(aom_image_t* aImage, bool aIsAlpha) {
  MOZ_ASSERT(aImage);
  MOZ_ASSERT(!mImage);
  MOZ_ASSERT(!mBuffer);

  uint8_t* srcY = aImage->planes[AOM_PLANE_Y];
  int yStride = aImage->stride[AOM_PLANE_Y];
  int yHeight = aom_img_plane_height(aImage, AOM_PLANE_Y);
  size_t yBufSize = yStride * yHeight;

  // If aImage is alpha plane. The data is located in Y channel.
  if (aIsAlpha) {
    mBuffer = MakeUnique<uint8_t[]>(yBufSize);
    if (!mBuffer) {
      return false;
    }
    uint8_t* destY = mBuffer.get();
    memcpy(destY, srcY, yBufSize);
    mImage.emplace(*aImage);
    mImage->planes[AOM_PLANE_Y] = destY;

    return true;
  }

  uint8_t* srcCb = aImage->planes[AOM_PLANE_U];
  int cbStride = aImage->stride[AOM_PLANE_U];
  int cbHeight = aom_img_plane_height(aImage, AOM_PLANE_U);
  size_t cbBufSize = cbStride * cbHeight;

  uint8_t* srcCr = aImage->planes[AOM_PLANE_V];
  int crStride = aImage->stride[AOM_PLANE_V];
  int crHeight = aom_img_plane_height(aImage, AOM_PLANE_V);
  size_t crBufSize = crStride * crHeight;

  mBuffer = MakeUnique<uint8_t[]>(yBufSize + cbBufSize + crBufSize);
  if (!mBuffer) {
    return false;
  }

  uint8_t* destY = mBuffer.get();
  uint8_t* destCb = destY + yBufSize;
  uint8_t* destCr = destCb + cbBufSize;

  memcpy(destY, srcY, yBufSize);
  memcpy(destCb, srcCb, cbBufSize);
  memcpy(destCr, srcCr, crBufSize);

  mImage.emplace(*aImage);
  mImage->planes[AOM_PLANE_Y] = destY;
  mImage->planes[AOM_PLANE_U] = destCb;
  mImage->planes[AOM_PLANE_V] = destCr;

  return true;
}

/* static */
OwnedAOMImage* OwnedAOMImage::CopyFrom(aom_image_t* aImage, bool aIsAlpha) {
  MOZ_ASSERT(aImage);
  UniquePtr<OwnedAOMImage> img(new OwnedAOMImage());
  if (!img->CloneFrom(aImage, aIsAlpha)) {
    return nullptr;
  }
  return img.release();
}

class AOMDecoder final : AVIFDecoderInterface {
 public:
  ~AOMDecoder() {
    MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("Destroy AOMDecoder=%p", this));

    if (mColorContext.isSome()) {
      aom_codec_err_t r = aom_codec_destroy(mColorContext.ptr());
      MOZ_LOG(sAVIFLog, LogLevel::Debug,
              ("[this=%p] aom_codec_destroy -> %d", this, r));
    }

    if (mAlphaContext.isSome()) {
      aom_codec_err_t r = aom_codec_destroy(mAlphaContext.ptr());
      MOZ_LOG(sAVIFLog, LogLevel::Debug,
              ("[this=%p] aom_codec_destroy -> %d", this, r));
    }
  }

  static DecodeResult Create(UniquePtr<AVIFDecoderInterface>& aDecoder,
                             bool aHasAlpha) {
    UniquePtr<AOMDecoder> d(new AOMDecoder());
    aom_codec_err_t e = d->Init(aHasAlpha);
    if (e == AOM_CODEC_OK) {
      aDecoder.reset(d.release());
    }
    return AsVariant(AOMResult(e));
  }

  DecodeResult Decode(bool aShouldSendTelemetry,
                      const Mp4parseAvifInfo& aAVIFInfo,
                      const AVIFImage& aSamples) override {
    MOZ_ASSERT(mColorContext.isSome());
    MOZ_ASSERT(!mDecodedData);
    MOZ_ASSERT(aSamples.mColorImage);

    aom_image_t* aomImg = nullptr;
    DecodeResult r = GetImage(*mColorContext, *aSamples.mColorImage, &aomImg,
                              aShouldSendTelemetry);
    if (!IsDecodeSuccess(r)) {
      return r;
    }
    MOZ_ASSERT(aomImg);

    // The aomImg will be released in next GetImage call (aom_codec_decode
    // actually). The GetImage could be called again immediately if parsedImg
    // contains alpha data. Therefore, we need to copy the image and manage it
    // by AOMDecoder itself.
    OwnedAOMImage* clonedImg = OwnedAOMImage::CopyFrom(aomImg, false);
    if (!clonedImg) {
      return AsVariant(NonDecoderResult::OutOfMemory);
    }
    mOwnedImage.reset(clonedImg);

    if (aSamples.mAlphaImage) {
      MOZ_ASSERT(mAlphaContext.isSome());

      aom_image_t* alphaImg = nullptr;
      r = GetImage(*mAlphaContext, *aSamples.mAlphaImage, &alphaImg,
                   aShouldSendTelemetry);
      if (!IsDecodeSuccess(r)) {
        return r;
      }
      MOZ_ASSERT(alphaImg);

      OwnedAOMImage* clonedAlphaImg = OwnedAOMImage::CopyFrom(alphaImg, true);
      if (!clonedAlphaImg) {
        return AsVariant(NonDecoderResult::OutOfMemory);
      }
      mOwnedAlphaPlane.reset(clonedAlphaImg);

      // Per § 4 of the AVIF spec
      // https://aomediacodec.github.io/av1-avif/#auxiliary-images: An AV1
      // Alpha Image Item […] shall be encoded with the same bit depth as the
      // associated master AV1 Image Item
      MOZ_ASSERT(mOwnedImage->GetImage() && mOwnedAlphaPlane->GetImage());
      if (mOwnedImage->GetImage()->bit_depth !=
          mOwnedAlphaPlane->GetImage()->bit_depth) {
        return AsVariant(NonDecoderResult::AlphaYColorDepthMismatch);
      }

      if (mOwnedImage->GetImage()->stride[AOM_PLANE_Y] !=
          mOwnedAlphaPlane->GetImage()->stride[AOM_PLANE_Y]) {
        return AsVariant(NonDecoderResult::AlphaYSizeMismatch);
      }
    }

    MOZ_ASSERT_IF(!mOwnedAlphaPlane, !aAVIFInfo.premultiplied_alpha);
    mDecodedData = AOMImageToToDecodedData(
        aAVIFInfo.nclx_colour_information, std::move(mOwnedImage),
        std::move(mOwnedAlphaPlane), aAVIFInfo.premultiplied_alpha);

    return r;
  }

 private:
  explicit AOMDecoder() {
    MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("Create AOMDecoder=%p", this));
  }

  aom_codec_err_t Init(bool aHasAlpha) {
    MOZ_ASSERT(mColorContext.isNothing());
    MOZ_ASSERT(mAlphaContext.isNothing());

    aom_codec_iface_t* iface = aom_codec_av1_dx();

    // Init color decoder context
    mColorContext.emplace();
    aom_codec_err_t r = aom_codec_dec_init(
        mColorContext.ptr(), iface, /* cfg = */ nullptr, /* flags = */ 0);

    MOZ_LOG(sAVIFLog, r == AOM_CODEC_OK ? LogLevel::Verbose : LogLevel::Error,
            ("[this=%p] color decoder: aom_codec_dec_init -> %d, name = %s",
             this, r, mColorContext->name));

    if (r != AOM_CODEC_OK) {
      mColorContext.reset();
      return r;
    }

    if (aHasAlpha) {
      // Init alpha decoder context
      mAlphaContext.emplace();
      r = aom_codec_dec_init(mAlphaContext.ptr(), iface, /* cfg = */ nullptr,
                             /* flags = */ 0);

      MOZ_LOG(sAVIFLog, r == AOM_CODEC_OK ? LogLevel::Verbose : LogLevel::Error,
              ("[this=%p] color decoder: aom_codec_dec_init -> %d, name = %s",
               this, r, mAlphaContext->name));

      if (r != AOM_CODEC_OK) {
        mAlphaContext.reset();
        return r;
      }
    }

    return r;
  }

  static DecodeResult GetImage(aom_codec_ctx_t& aContext,
                               const MediaRawData& aData, aom_image_t** aImage,
                               bool aShouldSendTelemetry) {
    aom_codec_err_t r =
        aom_codec_decode(&aContext, aData.Data(), aData.Size(), nullptr);

    MOZ_LOG(sAVIFLog, r == AOM_CODEC_OK ? LogLevel::Verbose : LogLevel::Error,
            ("aom_codec_decode -> %d", r));

    if (aShouldSendTelemetry) {
      switch (r) {
        case AOM_CODEC_OK:
          // No need to record any telemetry for the common case
          break;
        case AOM_CODEC_ERROR:
          AccumulateCategorical(LABELS_AVIF_AOM_DECODE_ERROR::error);
          break;
        case AOM_CODEC_MEM_ERROR:
          AccumulateCategorical(LABELS_AVIF_AOM_DECODE_ERROR::mem_error);
          break;
        case AOM_CODEC_ABI_MISMATCH:
          AccumulateCategorical(LABELS_AVIF_AOM_DECODE_ERROR::abi_mismatch);
          break;
        case AOM_CODEC_INCAPABLE:
          AccumulateCategorical(LABELS_AVIF_AOM_DECODE_ERROR::incapable);
          break;
        case AOM_CODEC_UNSUP_BITSTREAM:
          AccumulateCategorical(LABELS_AVIF_AOM_DECODE_ERROR::unsup_bitstream);
          break;
        case AOM_CODEC_UNSUP_FEATURE:
          AccumulateCategorical(LABELS_AVIF_AOM_DECODE_ERROR::unsup_feature);
          break;
        case AOM_CODEC_CORRUPT_FRAME:
          AccumulateCategorical(LABELS_AVIF_AOM_DECODE_ERROR::corrupt_frame);
          break;
        case AOM_CODEC_INVALID_PARAM:
          AccumulateCategorical(LABELS_AVIF_AOM_DECODE_ERROR::invalid_param);
          break;
        default:
          MOZ_ASSERT_UNREACHABLE(
              "Unknown aom_codec_err_t value from aom_codec_decode");
      }
    }

    if (r != AOM_CODEC_OK) {
      return AsVariant(AOMResult(r));
    }

    aom_codec_iter_t iter = nullptr;
    aom_image_t* img = aom_codec_get_frame(&aContext, &iter);

    MOZ_LOG(sAVIFLog, img == nullptr ? LogLevel::Error : LogLevel::Verbose,
            ("aom_codec_get_frame -> %p", img));

    if (img == nullptr) {
      return AsVariant(AOMResult(NonAOMCodecError::NoFrame));
    }

    const CheckedInt<int> decoded_width = img->d_w;
    const CheckedInt<int> decoded_height = img->d_h;

    if (!decoded_height.isValid() || !decoded_width.isValid()) {
      MOZ_LOG(sAVIFLog, LogLevel::Debug,
              ("image dimensions can't be stored in int: d_w: %u, "
               "d_h: %u",
               img->d_w, img->d_h));
      return AsVariant(AOMResult(NonAOMCodecError::SizeOverflow));
    }

    *aImage = img;
    return AsVariant(AOMResult(r));
  }

  static UniquePtr<AVIFDecodedData> AOMImageToToDecodedData(
      const Mp4parseNclxColourInformation* aNclx,
      UniquePtr<OwnedAOMImage> aImage, UniquePtr<OwnedAOMImage> aAlphaPlane,
      bool aPremultipliedAlpha);

  Maybe<aom_codec_ctx_t> mColorContext;
  Maybe<aom_codec_ctx_t> mAlphaContext;
  UniquePtr<OwnedAOMImage> mOwnedImage;
  UniquePtr<OwnedAOMImage> mOwnedAlphaPlane;
};

/* static */
UniquePtr<AVIFDecodedData> Dav1dDecoder::Dav1dPictureToDecodedData(
    const Mp4parseNclxColourInformation* aNclx, OwnedDav1dPicture aPicture,
    OwnedDav1dPicture aAlphaPlane, bool aPremultipliedAlpha) {
  MOZ_ASSERT(aPicture);

  static_assert(std::is_same<int, decltype(aPicture->p.w)>::value);
  static_assert(std::is_same<int, decltype(aPicture->p.h)>::value);

  UniquePtr<AVIFDecodedData> data = MakeUnique<AVIFDecodedData>();

  data->mRenderSize.emplace(aPicture->frame_hdr->render_width,
                            aPicture->frame_hdr->render_height);

  data->mYChannel = static_cast<uint8_t*>(aPicture->data[0]);
  data->mYStride = aPicture->stride[0];
  data->mYSkip = aPicture->stride[0] - aPicture->p.w;
  data->mCbChannel = static_cast<uint8_t*>(aPicture->data[1]);
  data->mCrChannel = static_cast<uint8_t*>(aPicture->data[2]);
  data->mCbCrStride = aPicture->stride[1];

  switch (aPicture->p.layout) {
    case DAV1D_PIXEL_LAYOUT_I400:  // Monochrome, so no Cb or Cr channels
      break;
    case DAV1D_PIXEL_LAYOUT_I420:
      data->mChromaSubsampling = ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;
      break;
    case DAV1D_PIXEL_LAYOUT_I422:
      data->mChromaSubsampling = ChromaSubsampling::HALF_WIDTH;
      break;
    case DAV1D_PIXEL_LAYOUT_I444:
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown pixel layout");
  }

  data->mCbSkip = aPicture->stride[1] - aPicture->p.w;
  data->mCrSkip = aPicture->stride[1] - aPicture->p.w;
  data->mPictureRect = IntRect(0, 0, aPicture->p.w, aPicture->p.h);
  data->mStereoMode = StereoMode::MONO;
  data->mColorDepth = ColorDepthForBitDepth(aPicture->p.bpc);

  MOZ_ASSERT(aPicture->p.bpc == BitDepthForColorDepth(data->mColorDepth));

  data->mYUVColorSpace = GetAVIFColorSpace(aNclx, [&]() {
    MOZ_LOG(sAVIFLog, LogLevel::Info,
            ("YUVColorSpace cannot be determined from colr box, using AV1 "
             "sequence header"));
    return DAV1DDecoder::GetColorSpace(*aPicture, sAVIFLog);
  });

  auto av1ColourPrimaries = CICP::ColourPrimaries::CP_UNSPECIFIED;
  auto av1TransferCharacteristics =
      CICP::TransferCharacteristics::TC_UNSPECIFIED;
  auto av1MatrixCoefficients = CICP::MatrixCoefficients::MC_UNSPECIFIED;

  MOZ_ASSERT(aPicture->seq_hdr);
  auto& seq_hdr = *aPicture->seq_hdr;

  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("seq_hdr.color_description_present: %d",
           seq_hdr.color_description_present));
  if (seq_hdr.color_description_present) {
    av1ColourPrimaries = static_cast<CICP::ColourPrimaries>(seq_hdr.pri);
    av1TransferCharacteristics =
        static_cast<CICP::TransferCharacteristics>(seq_hdr.trc);
    av1MatrixCoefficients = static_cast<CICP::MatrixCoefficients>(seq_hdr.mtrx);
  }

  data->SetCicpValues(aNclx, av1ColourPrimaries, av1TransferCharacteristics,
                      av1MatrixCoefficients);

  gfx::ColorRange av1ColorRange =
      seq_hdr.color_range ? gfx::ColorRange::FULL : gfx::ColorRange::LIMITED;
  data->mColorRange = GetAVIFColorRange(aNclx, av1ColorRange);

  auto colorPrimaries =
      gfxUtils::CicpToColorPrimaries(data->mColourPrimaries, sAVIFLog);
  if (colorPrimaries.isSome()) {
    data->mColorPrimaries = *colorPrimaries;
  }

  if (aAlphaPlane) {
    MOZ_ASSERT(aAlphaPlane->stride[0] == data->mYStride);
    data->mAlpha.emplace();
    data->mAlpha->mChannel = static_cast<uint8_t*>(aAlphaPlane->data[0]);
    data->mAlpha->mSize = gfx::IntSize(aAlphaPlane->p.w, aAlphaPlane->p.h);
    data->mAlpha->mPremultiplied = aPremultipliedAlpha;
  }

  data->mColorDav1d = std::move(aPicture);
  data->mAlphaDav1d = std::move(aAlphaPlane);

  return data;
}

/* static */
UniquePtr<AVIFDecodedData> AOMDecoder::AOMImageToToDecodedData(
    const Mp4parseNclxColourInformation* aNclx, UniquePtr<OwnedAOMImage> aImage,
    UniquePtr<OwnedAOMImage> aAlphaPlane, bool aPremultipliedAlpha) {
  aom_image_t* colorImage = aImage->GetImage();
  aom_image_t* alphaImage = aAlphaPlane ? aAlphaPlane->GetImage() : nullptr;

  MOZ_ASSERT(colorImage);
  MOZ_ASSERT(colorImage->stride[AOM_PLANE_Y] ==
             colorImage->stride[AOM_PLANE_ALPHA]);
  MOZ_ASSERT(colorImage->stride[AOM_PLANE_Y] >=
             aom_img_plane_width(colorImage, AOM_PLANE_Y));
  MOZ_ASSERT(colorImage->stride[AOM_PLANE_U] ==
             colorImage->stride[AOM_PLANE_V]);
  MOZ_ASSERT(colorImage->stride[AOM_PLANE_U] >=
             aom_img_plane_width(colorImage, AOM_PLANE_U));
  MOZ_ASSERT(colorImage->stride[AOM_PLANE_V] >=
             aom_img_plane_width(colorImage, AOM_PLANE_V));
  MOZ_ASSERT(aom_img_plane_width(colorImage, AOM_PLANE_U) ==
             aom_img_plane_width(colorImage, AOM_PLANE_V));
  MOZ_ASSERT(aom_img_plane_height(colorImage, AOM_PLANE_U) ==
             aom_img_plane_height(colorImage, AOM_PLANE_V));

  UniquePtr<AVIFDecodedData> data = MakeUnique<AVIFDecodedData>();

  data->mRenderSize.emplace(colorImage->r_w, colorImage->r_h);

  data->mYChannel = colorImage->planes[AOM_PLANE_Y];
  data->mYStride = colorImage->stride[AOM_PLANE_Y];
  data->mYSkip = colorImage->stride[AOM_PLANE_Y] -
                 aom_img_plane_width(colorImage, AOM_PLANE_Y);
  data->mCbChannel = colorImage->planes[AOM_PLANE_U];
  data->mCrChannel = colorImage->planes[AOM_PLANE_V];
  data->mCbCrStride = colorImage->stride[AOM_PLANE_U];
  data->mCbSkip = colorImage->stride[AOM_PLANE_U] -
                  aom_img_plane_width(colorImage, AOM_PLANE_U);
  data->mCrSkip = colorImage->stride[AOM_PLANE_V] -
                  aom_img_plane_width(colorImage, AOM_PLANE_V);
  data->mPictureRect = gfx::IntRect(0, 0, colorImage->d_w, colorImage->d_h);
  data->mStereoMode = StereoMode::MONO;
  data->mColorDepth = ColorDepthForBitDepth(colorImage->bit_depth);

  if (colorImage->x_chroma_shift == 1 && colorImage->y_chroma_shift == 1) {
    data->mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;
  } else if (colorImage->x_chroma_shift == 1 &&
             colorImage->y_chroma_shift == 0) {
    data->mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH;
  } else if (colorImage->x_chroma_shift != 0 ||
             colorImage->y_chroma_shift != 0) {
    MOZ_ASSERT_UNREACHABLE("unexpected chroma shifts");
  }

  MOZ_ASSERT(colorImage->bit_depth == BitDepthForColorDepth(data->mColorDepth));

  auto av1ColourPrimaries = static_cast<CICP::ColourPrimaries>(colorImage->cp);
  auto av1TransferCharacteristics =
      static_cast<CICP::TransferCharacteristics>(colorImage->tc);
  auto av1MatrixCoefficients =
      static_cast<CICP::MatrixCoefficients>(colorImage->mc);

  data->mYUVColorSpace = GetAVIFColorSpace(aNclx, [=]() {
    MOZ_LOG(sAVIFLog, LogLevel::Info,
            ("YUVColorSpace cannot be determined from colr box, using AV1 "
             "sequence header"));
    return gfxUtils::CicpToColorSpace(av1MatrixCoefficients, av1ColourPrimaries,
                                      sAVIFLog);
  });

  gfx::ColorRange av1ColorRange;
  if (colorImage->range == AOM_CR_STUDIO_RANGE) {
    av1ColorRange = gfx::ColorRange::LIMITED;
  } else {
    MOZ_ASSERT(colorImage->range == AOM_CR_FULL_RANGE);
    av1ColorRange = gfx::ColorRange::FULL;
  }
  data->mColorRange = GetAVIFColorRange(aNclx, av1ColorRange);

  data->SetCicpValues(aNclx, av1ColourPrimaries, av1TransferCharacteristics,
                      av1MatrixCoefficients);

  auto colorPrimaries =
      gfxUtils::CicpToColorPrimaries(data->mColourPrimaries, sAVIFLog);
  if (colorPrimaries.isSome()) {
    data->mColorPrimaries = *colorPrimaries;
  }

  if (alphaImage) {
    MOZ_ASSERT(alphaImage->stride[AOM_PLANE_Y] == data->mYStride);
    data->mAlpha.emplace();
    data->mAlpha->mChannel = alphaImage->planes[AOM_PLANE_Y];
    data->mAlpha->mSize = gfx::IntSize(alphaImage->d_w, alphaImage->d_h);
    data->mAlpha->mPremultiplied = aPremultipliedAlpha;
  }

  data->mColorAOM = std::move(aImage);
  data->mAlphaAOM = std::move(aAlphaPlane);

  return data;
}

// Wrapper to allow rust to call our read adaptor.
intptr_t nsAVIFDecoder::ReadSource(uint8_t* aDestBuf, uintptr_t aDestBufSize,
                                   void* aUserData) {
  MOZ_ASSERT(aDestBuf);
  MOZ_ASSERT(aUserData);

  MOZ_LOG(sAVIFLog, LogLevel::Verbose,
          ("AVIF ReadSource, aDestBufSize: %zu", aDestBufSize));

  auto* decoder = reinterpret_cast<nsAVIFDecoder*>(aUserData);

  MOZ_ASSERT(decoder->mReadCursor);

  size_t bufferLength = decoder->mBufferedData.end() - decoder->mReadCursor;
  size_t n_bytes = std::min(aDestBufSize, bufferLength);

  MOZ_LOG(
      sAVIFLog, LogLevel::Verbose,
      ("AVIF ReadSource, %zu bytes ready, copying %zu", bufferLength, n_bytes));

  memcpy(aDestBuf, decoder->mReadCursor, n_bytes);
  decoder->mReadCursor += n_bytes;

  return n_bytes;
}

nsAVIFDecoder::nsAVIFDecoder(RasterImage* aImage) : Decoder(aImage) {
  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] nsAVIFDecoder::nsAVIFDecoder", this));
}

nsAVIFDecoder::~nsAVIFDecoder() {
  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] nsAVIFDecoder::~nsAVIFDecoder", this));
}

LexerResult nsAVIFDecoder::DoDecode(SourceBufferIterator& aIterator,
                                    IResumable* aOnResume) {
  MOZ_LOG(sAVIFLog, LogLevel::Info,
          ("[this=%p] nsAVIFDecoder::DoDecode start", this));

  DecodeResult result = DoDecodeInternal(aIterator, aOnResume);

  RecordDecodeResultTelemetry(result);

  if (result.is<NonDecoderResult>()) {
    NonDecoderResult r = result.as<NonDecoderResult>();
    if (r == NonDecoderResult::NeedMoreData) {
      return LexerResult(Yield::NEED_MORE_DATA);
    }
    if (r == NonDecoderResult::OutputAvailable) {
      MOZ_ASSERT(HasSize());
      return LexerResult(Yield::OUTPUT_AVAILABLE);
    }
    if (r == NonDecoderResult::Complete) {
      MOZ_ASSERT(HasSize());
      return LexerResult(TerminalState::SUCCESS);
    }
    return LexerResult(TerminalState::FAILURE);
  }

  MOZ_ASSERT(result.is<Dav1dResult>() || result.is<AOMResult>() ||
             result.is<Mp4parseStatus>());
  // If IsMetadataDecode(), a successful parse should return
  // NonDecoderResult::MetadataOk or else continue to the decode stage
  MOZ_ASSERT_IF(result.is<Mp4parseStatus>(),
                result.as<Mp4parseStatus>() != MP4PARSE_STATUS_OK);
  auto rv = LexerResult(IsDecodeSuccess(result) ? TerminalState::SUCCESS
                                                : TerminalState::FAILURE);
  MOZ_LOG(sAVIFLog, LogLevel::Info,
          ("[this=%p] nsAVIFDecoder::DoDecode end", this));
  return rv;
}

Mp4parseStatus nsAVIFDecoder::CreateParser() {
  if (!mParser) {
    Mp4parseIo io = {nsAVIFDecoder::ReadSource, this};
    mBufferStream = new AVIFDecoderStream(&mBufferedData);

    Mp4parseStatus status = AVIFParser::Create(
        &io, mBufferStream.get(), mParser,
        bool(GetDecoderFlags() & DecoderFlags::AVIF_SEQUENCES_ENABLED),
        bool(GetDecoderFlags() & DecoderFlags::AVIF_ANIMATE_AVIF_MAJOR));

    if (status != MP4PARSE_STATUS_OK) {
      return status;
    }

    const Mp4parseAvifInfo& info = mParser->GetInfo();
    mIsAnimated = mParser->IsAnimated();
    mHasAlpha = mIsAnimated ? !!info.alpha_track_id : info.has_alpha_item;
  }

  return MP4PARSE_STATUS_OK;
}

nsAVIFDecoder::DecodeResult nsAVIFDecoder::CreateDecoder() {
  if (!mDecoder) {
    DecodeResult r = StaticPrefs::image_avif_use_dav1d()
                         ? Dav1dDecoder::Create(mDecoder, mHasAlpha)
                         : AOMDecoder::Create(mDecoder, mHasAlpha);

    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] Create %sDecoder %ssuccessfully", this,
             StaticPrefs::image_avif_use_dav1d() ? "Dav1d" : "AOM",
             IsDecodeSuccess(r) ? "" : "un"));

    return r;
  }

  return StaticPrefs::image_avif_use_dav1d()
             ? DecodeResult(Dav1dResult(0))
             : DecodeResult(AOMResult(AOM_CODEC_OK));
}

// Records all telemetry available in the AVIF metadata, called only once during
// the metadata decode to avoid multiple counts.
static void RecordMetadataTelem(const Mp4parseAvifInfo& aInfo) {
  if (aInfo.pixel_aspect_ratio) {
    const uint32_t& h_spacing = aInfo.pixel_aspect_ratio->h_spacing;
    const uint32_t& v_spacing = aInfo.pixel_aspect_ratio->v_spacing;

    if (h_spacing == 0 || v_spacing == 0) {
      AccumulateCategorical(LABELS_AVIF_PASP::invalid);
    } else if (h_spacing == v_spacing) {
      AccumulateCategorical(LABELS_AVIF_PASP::square);
    } else {
      AccumulateCategorical(LABELS_AVIF_PASP::nonsquare);
    }
  } else {
    AccumulateCategorical(LABELS_AVIF_PASP::absent);
  }

  const auto& major_brand = aInfo.major_brand;
  if (!memcmp(major_brand, "avif", sizeof(major_brand))) {
    AccumulateCategorical(LABELS_AVIF_MAJOR_BRAND::avif);
  } else if (!memcmp(major_brand, "avis", sizeof(major_brand))) {
    AccumulateCategorical(LABELS_AVIF_MAJOR_BRAND::avis);
  } else {
    AccumulateCategorical(LABELS_AVIF_MAJOR_BRAND::other);
  }

  AccumulateCategorical(aInfo.has_sequence ? LABELS_AVIF_SEQUENCE::present
                                           : LABELS_AVIF_SEQUENCE::absent);

#define FEATURE_TELEMETRY(fourcc)                                              \
  AccumulateCategorical(                                                       \
      (aInfo.unsupported_features_bitfield & (1 << MP4PARSE_FEATURE_##fourcc)) \
          ? LABELS_AVIF_##fourcc::present                                      \
          : LABELS_AVIF_##fourcc::absent)
  FEATURE_TELEMETRY(A1LX);
  FEATURE_TELEMETRY(A1OP);
  FEATURE_TELEMETRY(CLAP);
  FEATURE_TELEMETRY(GRID);
  FEATURE_TELEMETRY(IPRO);
  FEATURE_TELEMETRY(LSEL);

  if (aInfo.nclx_colour_information && aInfo.icc_colour_information.data) {
    AccumulateCategorical(LABELS_AVIF_COLR::both);
  } else if (aInfo.nclx_colour_information) {
    AccumulateCategorical(LABELS_AVIF_COLR::nclx);
  } else if (aInfo.icc_colour_information.data) {
    AccumulateCategorical(LABELS_AVIF_COLR::icc);
  } else {
    AccumulateCategorical(LABELS_AVIF_COLR::absent);
  }
}

static void RecordPixiTelemetry(uint8_t aPixiBitDepth,
                                uint8_t aBitstreamBitDepth,
                                const char* aItemName) {
  if (aPixiBitDepth == 0) {
    AccumulateCategorical(LABELS_AVIF_PIXI::absent);
  } else if (aPixiBitDepth == aBitstreamBitDepth) {
    AccumulateCategorical(LABELS_AVIF_PIXI::valid);
  } else {
    MOZ_LOG(sAVIFLog, LogLevel::Error,
            ("%s item pixi bit depth (%hhu) doesn't match "
             "bitstream (%hhu)",
             aItemName, aPixiBitDepth, aBitstreamBitDepth));
    AccumulateCategorical(LABELS_AVIF_PIXI::bitstream_mismatch);
  }
}

// This telemetry depends on the results of decoding.
// These data must be recorded only on the first frame decoded after metadata
// decode finishes.
static void RecordFrameTelem(bool aAnimated, const Mp4parseAvifInfo& aInfo,
                             const AVIFDecodedData& aData) {
  AccumulateCategorical(
      gColorSpaceLabel[static_cast<size_t>(aData.mYUVColorSpace)]);
  AccumulateCategorical(
      gColorDepthLabel[static_cast<size_t>(aData.mColorDepth)]);

  RecordPixiTelemetry(
      aAnimated ? aInfo.color_track_bit_depth : aInfo.primary_item_bit_depth,
      BitDepthForColorDepth(aData.mColorDepth), "color");

  if (aData.mAlpha) {
    AccumulateCategorical(LABELS_AVIF_ALPHA::present);
    RecordPixiTelemetry(
        aAnimated ? aInfo.alpha_track_bit_depth : aInfo.alpha_item_bit_depth,
        BitDepthForColorDepth(aData.mColorDepth), "alpha");
  } else {
    AccumulateCategorical(LABELS_AVIF_ALPHA::absent);
  }

  if (CICP::IsReserved(aData.mColourPrimaries)) {
    AccumulateCategorical(LABELS_AVIF_CICP_CP::RESERVED_REST);
  } else {
    AccumulateCategorical(
        static_cast<LABELS_AVIF_CICP_CP>(aData.mColourPrimaries));
  }

  if (CICP::IsReserved(aData.mTransferCharacteristics)) {
    AccumulateCategorical(LABELS_AVIF_CICP_TC::RESERVED);
  } else {
    AccumulateCategorical(
        static_cast<LABELS_AVIF_CICP_TC>(aData.mTransferCharacteristics));
  }

  if (CICP::IsReserved(aData.mMatrixCoefficients)) {
    AccumulateCategorical(LABELS_AVIF_CICP_MC::RESERVED);
  } else {
    AccumulateCategorical(
        static_cast<LABELS_AVIF_CICP_MC>(aData.mMatrixCoefficients));
  }
}

nsAVIFDecoder::DecodeResult nsAVIFDecoder::DoDecodeInternal(
    SourceBufferIterator& aIterator, IResumable* aOnResume) {
  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] nsAVIFDecoder::DoDecodeInternal", this));

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
        return AsVariant(NonDecoderResult::NeedMoreData);

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

  Mp4parseStatus parserStatus = CreateParser();

  if (parserStatus != MP4PARSE_STATUS_OK) {
    return AsVariant(parserStatus);
  }

  const Mp4parseAvifInfo& parsedInfo = mParser->GetInfo();

  if (parsedInfo.icc_colour_information.data) {
    const auto& icc = parsedInfo.icc_colour_information;
    MOZ_LOG(
        sAVIFLog, LogLevel::Debug,
        ("[this=%p] colr type ICC: %zu bytes %p", this, icc.length, icc.data));
  }

  if (IsMetadataDecode()) {
    RecordMetadataTelem(parsedInfo);
  }

  if (parsedInfo.nclx_colour_information) {
    const auto& nclx = *parsedInfo.nclx_colour_information;
    MOZ_LOG(
        sAVIFLog, LogLevel::Debug,
        ("[this=%p] colr type CICP: cp/tc/mc/full-range %u/%u/%u/%s", this,
         nclx.colour_primaries, nclx.transfer_characteristics,
         nclx.matrix_coefficients, nclx.full_range_flag ? "true" : "false"));
  }

  if (!parsedInfo.icc_colour_information.data &&
      !parsedInfo.nclx_colour_information) {
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] colr box not present", this));
  }

  AVIFImage parsedImage;
  DecodeResult r = mParser->GetImage(parsedImage);
  if (!IsDecodeSuccess(r)) {
    return r;
  }
  bool isDone =
      !IsMetadataDecode() && r == DecodeResult(NonDecoderResult::Complete);

  if (mIsAnimated) {
    PostIsAnimated(parsedImage.mDuration);
  }
  if (mHasAlpha) {
    PostHasTransparency();
  }

  Orientation orientation = StaticPrefs::image_avif_apply_transforms()
                                ? GetImageOrientation(parsedInfo)
                                : Orientation{};
  // TODO: Orientation should probably also apply to animated AVIFs.
  if (mIsAnimated) {
    orientation = Orientation{};
  }

  MaybeIntSize ispeImageSize = GetImageSize(parsedInfo);

  bool sendDecodeTelemetry = IsMetadataDecode();
  if (ispeImageSize.isSome()) {
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] Parser returned image size %d x %d (%d/%d bit)", this,
             ispeImageSize->width, ispeImageSize->height,
             mIsAnimated ? parsedInfo.color_track_bit_depth
                         : parsedInfo.primary_item_bit_depth,
             mIsAnimated ? parsedInfo.alpha_track_bit_depth
                         : parsedInfo.alpha_item_bit_depth));
    PostSize(ispeImageSize->width, ispeImageSize->height, orientation);
    if (IsMetadataDecode()) {
      MOZ_LOG(
          sAVIFLog, LogLevel::Debug,
          ("[this=%p] Finishing metadata decode without image decode", this));
      return AsVariant(NonDecoderResult::Complete);
    }
    // If we're continuing to decode here, this means we skipped decode
    // telemetry for the metadata decode pass. Send it this time.
    sendDecodeTelemetry = true;
  } else {
    MOZ_LOG(sAVIFLog, LogLevel::Error,
            ("[this=%p] Parser returned no image size, decoding...", this));
  }

  r = CreateDecoder();
  if (!IsDecodeSuccess(r)) {
    return r;
  }
  MOZ_ASSERT(mDecoder);
  r = mDecoder->Decode(sendDecodeTelemetry, parsedInfo, parsedImage);
  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] Decoder%s->Decode() %s", this,
           StaticPrefs::image_avif_use_dav1d() ? "Dav1d" : "AOM",
           IsDecodeSuccess(r) ? "succeeds" : "fails"));

  if (!IsDecodeSuccess(r)) {
    return r;
  }

  UniquePtr<AVIFDecodedData> decodedData = mDecoder->GetDecodedData();

  MOZ_ASSERT_IF(mHasAlpha, decodedData->mAlpha.isSome());

  MOZ_ASSERT(decodedData->mColourPrimaries !=
             CICP::ColourPrimaries::CP_UNSPECIFIED);
  MOZ_ASSERT(decodedData->mTransferCharacteristics !=
             CICP::TransferCharacteristics::TC_UNSPECIFIED);
  MOZ_ASSERT(decodedData->mColorRange <= gfx::ColorRange::_Last);
  MOZ_ASSERT(decodedData->mYUVColorSpace <= gfx::YUVColorSpace::_Last);

  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] decodedData.mColorRange: %hhd", this,
           static_cast<uint8_t>(decodedData->mColorRange)));

  // Technically it's valid but we don't handle it now (Bug 1682318).
  if (decodedData->mAlpha &&
      decodedData->mAlpha->mSize != decodedData->YDataSize()) {
    return AsVariant(NonDecoderResult::AlphaYSizeMismatch);
  }

  bool isFirstFrame = GetFrameCount() == 0;

  if (!HasSize()) {
    MOZ_ASSERT(isFirstFrame);
    MOZ_LOG(
        sAVIFLog, LogLevel::Error,
        ("[this=%p] Using decoded image size: %d x %d", this,
         decodedData->mPictureRect.width, decodedData->mPictureRect.height));
    PostSize(decodedData->mPictureRect.width, decodedData->mPictureRect.height,
             orientation);
    AccumulateCategorical(LABELS_AVIF_ISPE::absent);
  } else {
    // Verify that the bitstream hasn't changed the image size compared to
    // either the ispe box or the previous frames.
    IntSize expectedSize = GetImageMetadata()
                               .GetOrientation()
                               .ToUnoriented(Size())
                               .ToUnknownSize();
    if (decodedData->mPictureRect.width != expectedSize.width ||
        decodedData->mPictureRect.height != expectedSize.height) {
      if (isFirstFrame) {
        MOZ_LOG(
            sAVIFLog, LogLevel::Error,
            ("[this=%p] Metadata image size doesn't match decoded image size: "
             "(%d x %d) != (%d x %d)",
             this, ispeImageSize->width, ispeImageSize->height,
             decodedData->mPictureRect.width,
             decodedData->mPictureRect.height));
        AccumulateCategorical(LABELS_AVIF_ISPE::bitstream_mismatch);
        return AsVariant(NonDecoderResult::MetadataImageSizeMismatch);
      }

      MOZ_LOG(
          sAVIFLog, LogLevel::Error,
          ("[this=%p] Frame size has changed in the bitstream: "
           "(%d x %d) != (%d x %d)",
           this, expectedSize.width, expectedSize.height,
           decodedData->mPictureRect.width, decodedData->mPictureRect.height));
      return AsVariant(NonDecoderResult::FrameSizeChanged);
    }

    if (isFirstFrame) {
      AccumulateCategorical(LABELS_AVIF_ISPE::valid);
    }
  }

  if (IsMetadataDecode()) {
    return AsVariant(NonDecoderResult::Complete);
  }

  IntSize rgbSize = decodedData->mPictureRect.Size();

  if (parsedImage.mFrameNum == 0) {
    RecordFrameTelem(mIsAnimated, parsedInfo, *decodedData);
  }

  if (decodedData->mRenderSize &&
      decodedData->mRenderSize->ToUnknownSize() != rgbSize) {
    // This may be supported by allowing all metadata decodes to decode a frame
    // and get the render size from the bitstream. However it's unlikely to be
    // used often.
    return AsVariant(NonDecoderResult::RenderSizeMismatch);
  }

  // Read color profile
  if (mCMSMode != CMSMode::Off) {
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] Processing color profile", this));

    // See comment on AVIFDecodedData
    if (parsedInfo.icc_colour_information.data) {
      // same profile for every frame of image, only create it once
      if (!mInProfile) {
        const auto& icc = parsedInfo.icc_colour_information;
        mInProfile = qcms_profile_from_memory(icc.data, icc.length);
      }
    } else {
      // potentially different profile every frame, destroy the old one
      if (mInProfile) {
        if (mTransform) {
          qcms_transform_release(mTransform);
          mTransform = nullptr;
        }
        qcms_profile_release(mInProfile);
        mInProfile = nullptr;
      }

      const auto& cp = decodedData->mColourPrimaries;
      const auto& tc = decodedData->mTransferCharacteristics;

      if (CICP::IsReserved(cp)) {
        MOZ_LOG(sAVIFLog, LogLevel::Error,
                ("[this=%p] colour_primaries reserved value (%hhu) is invalid; "
                 "failing",
                 this, cp));
        return AsVariant(NonDecoderResult::InvalidCICP);
      }

      if (CICP::IsReserved(tc)) {
        MOZ_LOG(sAVIFLog, LogLevel::Error,
                ("[this=%p] transfer_characteristics reserved value (%hhu) is "
                 "invalid; failing",
                 this, tc));
        return AsVariant(NonDecoderResult::InvalidCICP);
      }

      MOZ_ASSERT(cp != CICP::ColourPrimaries::CP_UNSPECIFIED &&
                 !CICP::IsReserved(cp));
      MOZ_ASSERT(tc != CICP::TransferCharacteristics::TC_UNSPECIFIED &&
                 !CICP::IsReserved(tc));

      mInProfile = qcms_profile_create_cicp(cp, tc);
    }

    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] mInProfile %p", this, mInProfile));
  } else {
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] CMSMode::Off, skipping color profile", this));
  }

  if (mInProfile && GetCMSOutputProfile() && !mTransform) {
    auto intent = static_cast<qcms_intent>(gfxPlatform::GetRenderingIntent());
    qcms_data_type inType;
    qcms_data_type outType;

    // If we're not mandating an intent, use the one from the image.
    if (gfxPlatform::GetRenderingIntent() == -1) {
      intent = qcms_profile_get_rendering_intent(mInProfile);
    }

    uint32_t profileSpace = qcms_profile_get_color_space(mInProfile);
    if (profileSpace != icSigGrayData) {
      // If the transform happens with SurfacePipe, it will be in RGBA if we
      // have an alpha channel, because the swizzle and premultiplication
      // happens after color management. Otherwise it will be in BGRA because
      // the swizzle happens at the start.
      if (mHasAlpha) {
        inType = QCMS_DATA_RGBA_8;
        outType = QCMS_DATA_RGBA_8;
      } else {
        inType = gfxPlatform::GetCMSOSRGBAType();
        outType = inType;
      }
    } else {
      if (mHasAlpha) {
        inType = QCMS_DATA_GRAYA_8;
        outType = gfxPlatform::GetCMSOSRGBAType();
      } else {
        inType = QCMS_DATA_GRAY_8;
        outType = gfxPlatform::GetCMSOSRGBAType();
      }
    }

    mTransform = qcms_transform_create(mInProfile, inType,
                                       GetCMSOutputProfile(), outType, intent);
  }

  // Get suggested format and size. Note that GetYCbCrToRGBDestFormatAndSize
  // force format to be B8G8R8X8 if it's not.
  gfx::SurfaceFormat format = SurfaceFormat::OS_RGBX;
  gfx::GetYCbCrToRGBDestFormatAndSize(*decodedData, format, rgbSize);
  if (mHasAlpha) {
    // We would use libyuv to do the YCbCrA -> ARGB convertion, which only
    // works for B8G8R8A8.
    format = SurfaceFormat::B8G8R8A8;
  }

  const int bytesPerPixel = BytesPerPixel(format);

  const CheckedInt rgbStride = CheckedInt<int>(rgbSize.width) * bytesPerPixel;
  const CheckedInt rgbBufLength = rgbStride * rgbSize.height;

  if (!rgbStride.isValid() || !rgbBufLength.isValid()) {
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] overflow calculating rgbBufLength: rbgSize.width: %d, "
             "rgbSize.height: %d, "
             "bytesPerPixel: %u",
             this, rgbSize.width, rgbSize.height, bytesPerPixel));
    return AsVariant(NonDecoderResult::SizeOverflow);
  }

  UniquePtr<uint8_t[]> rgbBuf = MakeUnique<uint8_t[]>(rgbBufLength.value());
  const uint8_t* endOfRgbBuf = {rgbBuf.get() + rgbBufLength.value()};

  if (!rgbBuf) {
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] allocation of %u-byte rgbBuf failed", this,
             rgbBufLength.value()));
    return AsVariant(NonDecoderResult::OutOfMemory);
  }

  if (decodedData->mAlpha) {
    const auto wantPremultiply =
        !bool(GetSurfaceFlags() & SurfaceFlags::NO_PREMULTIPLY_ALPHA);
    const bool& hasPremultiply = decodedData->mAlpha->mPremultiplied;

    PremultFunc premultOp = nullptr;
    if (wantPremultiply && !hasPremultiply) {
      premultOp = libyuv::ARGBAttenuate;
    } else if (!wantPremultiply && hasPremultiply) {
      premultOp = libyuv::ARGBUnattenuate;
    }

    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] calling gfx::ConvertYCbCrAToARGB premultOp: %p", this,
             premultOp));
    gfx::ConvertYCbCrAToARGB(*decodedData, *decodedData->mAlpha, format,
                             rgbSize, rgbBuf.get(), rgbStride.value(),
                             premultOp);
  } else {
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] calling gfx::ConvertYCbCrToRGB", this));
    gfx::ConvertYCbCrToRGB(*decodedData, format, rgbSize, rgbBuf.get(),
                           rgbStride.value());
  }

  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] calling SurfacePipeFactory::CreateSurfacePipe", this));

  Maybe<SurfacePipe> pipe = Nothing();

  if (mIsAnimated) {
    SurfaceFormat outFormat =
        decodedData->mAlpha ? SurfaceFormat::OS_RGBA : SurfaceFormat::OS_RGBX;
    Maybe<AnimationParams> animParams;
    if (!IsFirstFrameDecode()) {
      animParams.emplace(FullFrame().ToUnknownRect(), parsedImage.mDuration,
                         parsedImage.mFrameNum, BlendMethod::SOURCE,
                         DisposalMethod::CLEAR_ALL);
    }
    pipe = SurfacePipeFactory::CreateSurfacePipe(
        this, Size(), OutputSize(), FullFrame(), format, outFormat, animParams,
        mTransform, SurfacePipeFlags());
  } else {
    pipe = SurfacePipeFactory::CreateReorientSurfacePipe(
        this, Size(), OutputSize(), format, mTransform, GetOrientation());
  }

  if (pipe.isNothing()) {
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] could not initialize surface pipe", this));
    return AsVariant(NonDecoderResult::PipeInitError);
  }

  MOZ_LOG(sAVIFLog, LogLevel::Debug, ("[this=%p] writing to surface", this));
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

  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] writing to surface complete", this));

  if (writeBufferResult == WriteState::FINISHED) {
    PostFrameStop(mHasAlpha ? Opacity::SOME_TRANSPARENCY
                            : Opacity::FULLY_OPAQUE);

    if (!mIsAnimated || IsFirstFrameDecode()) {
      PostDecodeDone(0);
      return DecodeResult(NonDecoderResult::Complete);
    }

    if (isDone) {
      switch (mParser->GetInfo().loop_mode) {
        case MP4PARSE_AVIF_LOOP_MODE_LOOP_BY_COUNT: {
          auto loopCount = mParser->GetInfo().loop_count;
          PostDecodeDone(
              loopCount > INT32_MAX ? -1 : static_cast<int32_t>(loopCount));
          break;
        }
        case MP4PARSE_AVIF_LOOP_MODE_LOOP_INFINITELY:
        case MP4PARSE_AVIF_LOOP_MODE_NO_EDITS:
        default:
          PostDecodeDone(-1);
          break;
      }
      return DecodeResult(NonDecoderResult::Complete);
    }

    return DecodeResult(NonDecoderResult::OutputAvailable);
  }

  return AsVariant(NonDecoderResult::WriteBufferError);
}

/* static */
bool nsAVIFDecoder::IsDecodeSuccess(const DecodeResult& aResult) {
  return aResult == DecodeResult(NonDecoderResult::OutputAvailable) ||
         aResult == DecodeResult(NonDecoderResult::Complete) ||
         aResult == DecodeResult(Dav1dResult(0)) ||
         aResult == DecodeResult(AOMResult(AOM_CODEC_OK));
}

void nsAVIFDecoder::RecordDecodeResultTelemetry(
    const nsAVIFDecoder::DecodeResult& aResult) {
  if (aResult.is<Mp4parseStatus>()) {
    switch (aResult.as<Mp4parseStatus>()) {
      case MP4PARSE_STATUS_OK:
        MOZ_ASSERT_UNREACHABLE(
            "Expect NonDecoderResult, Dav1dResult or AOMResult");
        return;
      case MP4PARSE_STATUS_BAD_ARG:
      case MP4PARSE_STATUS_INVALID:
      case MP4PARSE_STATUS_UNSUPPORTED:
      case MP4PARSE_STATUS_EOF:
      case MP4PARSE_STATUS_IO:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::parse_error);
        return;
      case MP4PARSE_STATUS_OOM:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::out_of_memory);
        return;
      case MP4PARSE_STATUS_MISSING_AVIF_OR_AVIS_BRAND:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::missing_brand);
        return;
      case MP4PARSE_STATUS_FTYP_NOT_FIRST:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::ftyp_not_first);
        return;
      case MP4PARSE_STATUS_NO_IMAGE:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::no_image);
        return;
      case MP4PARSE_STATUS_MOOV_BAD_QUANTITY:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::multiple_moov);
        return;
      case MP4PARSE_STATUS_MOOV_MISSING:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::no_moov);
        return;
      case MP4PARSE_STATUS_LSEL_NO_ESSENTIAL:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::lsel_no_essential);
        return;
      case MP4PARSE_STATUS_A1OP_NO_ESSENTIAL:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::a1op_no_essential);
        return;
      case MP4PARSE_STATUS_A1LX_ESSENTIAL:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::a1lx_essential);
        return;
      case MP4PARSE_STATUS_TXFORM_NO_ESSENTIAL:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::txform_no_essential);
        return;
      case MP4PARSE_STATUS_PITM_MISSING:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::no_primary_item);
        return;
      case MP4PARSE_STATUS_IMAGE_ITEM_TYPE:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::image_item_type);
        return;
      case MP4PARSE_STATUS_ITEM_TYPE_MISSING:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::item_type_missing);
        return;
      case MP4PARSE_STATUS_CONSTRUCTION_METHOD:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::construction_method);
        return;
      case MP4PARSE_STATUS_PITM_NOT_FOUND:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::item_loc_not_found);
        return;
      case MP4PARSE_STATUS_IDAT_MISSING:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::no_item_data_box);
        return;
      default:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::uncategorized);
        return;
    }

    MOZ_LOG(sAVIFLog, LogLevel::Error,
            ("[this=%p] unexpected Mp4parseStatus value: %d", this,
             aResult.as<Mp4parseStatus>()));
    MOZ_ASSERT(false, "unexpected Mp4parseStatus value");
    AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::invalid_parse_status);

  } else if (aResult.is<NonDecoderResult>()) {
    switch (aResult.as<NonDecoderResult>()) {
      case NonDecoderResult::NeedMoreData:
        return;
      case NonDecoderResult::OutputAvailable:
        return;
      case NonDecoderResult::Complete:
        return;
      case NonDecoderResult::SizeOverflow:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::size_overflow);
        return;
      case NonDecoderResult::OutOfMemory:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::out_of_memory);
        return;
      case NonDecoderResult::PipeInitError:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::pipe_init_error);
        return;
      case NonDecoderResult::WriteBufferError:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::write_buffer_error);
        return;
      case NonDecoderResult::AlphaYSizeMismatch:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::alpha_y_sz_mismatch);
        return;
      case NonDecoderResult::AlphaYColorDepthMismatch:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::alpha_y_bpc_mismatch);
        return;
      case NonDecoderResult::MetadataImageSizeMismatch:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::ispe_mismatch);
        return;
      case NonDecoderResult::RenderSizeMismatch:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::render_size_mismatch);
        return;
      case NonDecoderResult::FrameSizeChanged:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::frame_size_changed);
        return;
      case NonDecoderResult::InvalidCICP:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::invalid_cicp);
        return;
      case NonDecoderResult::NoSamples:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::no_samples);
        return;
    }
    MOZ_ASSERT_UNREACHABLE("unknown NonDecoderResult");
  } else {
    MOZ_ASSERT(aResult.is<Dav1dResult>() || aResult.is<AOMResult>());
    AccumulateCategorical(aResult.is<Dav1dResult>() ? LABELS_AVIF_DECODER::dav1d
                                                    : LABELS_AVIF_DECODER::aom);
    AccumulateCategorical(IsDecodeSuccess(aResult)
                              ? LABELS_AVIF_DECODE_RESULT::success
                              : LABELS_AVIF_DECODE_RESULT::decode_error);
  }
}

Maybe<Telemetry::HistogramID> nsAVIFDecoder::SpeedHistogram() const {
  return Some(Telemetry::IMAGE_DECODE_SPEED_AVIF);
}

}  // namespace image
}  // namespace mozilla
