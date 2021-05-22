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
#include "libyuv.h"

#include "SurfacePipeFactory.h"

#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryComms.h"

using namespace mozilla::gfx;

namespace mozilla {

Maybe<gfx::YUVColorSpace> GetColorSpace(const Dav1dPicture&, LazyLogModule&);

namespace image {

using Telemetry::LABELS_AVIF_AOM_DECODE_ERROR;
using Telemetry::LABELS_AVIF_BIT_DEPTH;
using Telemetry::LABELS_AVIF_DECODE_RESULT;
using Telemetry::LABELS_AVIF_DECODER;
using Telemetry::LABELS_AVIF_YUV_COLOR_SPACE;

static LazyLogModule sAVIFLog("AVIFDecoder");

static const LABELS_AVIF_BIT_DEPTH gColorDepthLabel[] = {
    LABELS_AVIF_BIT_DEPTH::color_8, LABELS_AVIF_BIT_DEPTH::color_10,
    LABELS_AVIF_BIT_DEPTH::color_12, LABELS_AVIF_BIT_DEPTH::color_16};

static const LABELS_AVIF_YUV_COLOR_SPACE gColorSpaceLabel[] = {
    LABELS_AVIF_YUV_COLOR_SPACE::BT601, LABELS_AVIF_YUV_COLOR_SPACE::BT709,
    LABELS_AVIF_YUV_COLOR_SPACE::BT2020, LABELS_AVIF_YUV_COLOR_SPACE::identity};

class AVIFParser {
 public:
  static AVIFParser* Create(const Mp4parseIo* aIo) {
    MOZ_ASSERT(aIo);

    UniquePtr<AVIFParser> p(new AVIFParser(aIo));
    if (!p->Init()) {
      return nullptr;
    }
    MOZ_ASSERT(p->mParser);
    return p.release();
  }

  ~AVIFParser() {
    MOZ_LOG(sAVIFLog, LogLevel::Debug, ("Destroy AVIFParser=%p", this));
  }

  AvifImage* GetImage() {
    MOZ_ASSERT(mParser);

    if (mAvifImage.isNothing()) {
      mAvifImage.emplace();
      Mp4parseStatus status =
          mp4parse_avif_get_image(mParser.get(), mAvifImage.ptr());
      MOZ_LOG(sAVIFLog, LogLevel::Debug,
              ("[this=%p] mp4parse_avif_get_image -> %d; primary_item length: "
               "%zu, alpha_item length: %zu",
               this, status, mAvifImage->primary_item.length,
               mAvifImage->alpha_item.length));
      if (status != MP4PARSE_STATUS_OK) {
        mAvifImage.reset();
        return nullptr;
      }
    }
    return mAvifImage.ptr();
  }

 private:
  explicit AVIFParser(const Mp4parseIo* aIo) : mIo(aIo) {
    MOZ_ASSERT(mIo);
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("Create AVIFParser=%p, image.avif.compliance_strictness: %d", this,
             StaticPrefs::image_avif_compliance_strictness()));
  }

  bool Init() {
    MOZ_ASSERT(!mParser);

    Mp4parseAvifParser* parser = nullptr;
    Mp4parseStatus status =
        mp4parse_avif_new(mIo,
                          static_cast<enum Mp4parseStrictness>(
                              StaticPrefs::image_avif_compliance_strictness()),
                          &parser);
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] mp4parse_avif_new status: %d", this, status));
    if (status != MP4PARSE_STATUS_OK) {
      return false;
    }
    mParser.reset(parser);
    return true;
  }

  struct FreeAvifParser {
    void operator()(Mp4parseAvifParser* aPtr) { mp4parse_avif_free(aPtr); }
  };

  const Mp4parseIo* mIo;
  UniquePtr<Mp4parseAvifParser, FreeAvifParser> mParser;
  Maybe<AvifImage> mAvifImage;
};

// An interface to do decode and get the decoded data
class AVIFDecoderInterface {
 public:
  using Dav1dResult = nsAVIFDecoder::Dav1dResult;
  using NonAOMCodecError = nsAVIFDecoder::NonAOMCodecError;
  using AOMResult = nsAVIFDecoder::AOMResult;
  using NonDecoderResult = nsAVIFDecoder::NonDecoderResult;
  using DecodeResult = nsAVIFDecoder::DecodeResult;

  virtual ~AVIFDecoderInterface() = default;

  // Set the mDecodedData if Decode() succeeds
  virtual DecodeResult Decode(bool aIsMetadataDecode) = 0;
  // Must be called after Decode() succeeds
  layers::PlanarYCbCrAData& GetDecodedData() {
    MOZ_ASSERT(mDecodedData.isSome());
    return mDecodedData.ref();
  }

 protected:
  explicit AVIFDecoderInterface(UniquePtr<AVIFParser>&& aParser)
      : mParser(std::move(aParser)) {
    MOZ_ASSERT(mParser);
  }

  inline static bool IsDecodeSuccess(const DecodeResult& aResult) {
    return nsAVIFDecoder::IsDecodeSuccess(aResult);
  }

  UniquePtr<AVIFParser> mParser;

  // The mDecodedData is valid after Decode() succeeds
  Maybe<layers::PlanarYCbCrAData> mDecodedData;
};

class Dav1dDecoder final : AVIFDecoderInterface {
 public:
  ~Dav1dDecoder() {
    MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("Destroy Dav1dDecoder=%p", this));

    if (mPicture) {
      dav1d_picture_unref(mPicture.take().ptr());
    }

    if (mAlphaPlane) {
      dav1d_picture_unref(mAlphaPlane.take().ptr());
    }

    if (mContext) {
      dav1d_close(&mContext);
      MOZ_ASSERT(!mContext);
    }
  }

  static DecodeResult Create(UniquePtr<AVIFParser>&& aParser,
                             UniquePtr<AVIFDecoderInterface>& aDecoder) {
    UniquePtr<Dav1dDecoder> d(new Dav1dDecoder(std::move(aParser)));
    Dav1dResult r = d->Init();
    if (r == 0) {
      MOZ_ASSERT(d->mContext);
      aDecoder.reset(d.release());
    }
    return AsVariant(r);
  }

  DecodeResult Decode(bool aIsMetadataDecode) override {
    MOZ_ASSERT(mParser);
    MOZ_ASSERT(mContext);
    MOZ_ASSERT(mPicture.isNothing());
    MOZ_ASSERT(mDecodedData.isNothing());

    MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("[this=%p] Beginning Decode", this));

    AvifImage* parsedImg = mParser->GetImage();
    if (!parsedImg || !parsedImg->primary_item.data ||
        !parsedImg->primary_item.length) {
      return AsVariant(NonDecoderResult::NoPrimaryItem);
    }

    mPicture.emplace();
    Dav1dResult r = GetPicture(&(parsedImg->primary_item), mPicture.ptr(),
                               aIsMetadataDecode);
    if (r != 0) {
      mPicture.reset();
      return AsVariant(r);
    }

    if (parsedImg->alpha_item.data && parsedImg->alpha_item.length) {
      mAlphaPlane.emplace();
      Dav1dResult r = GetPicture(&(parsedImg->alpha_item), mAlphaPlane.ptr(),
                                 aIsMetadataDecode);
      if (r != 0) {
        mAlphaPlane.reset();
        return AsVariant(r);
      }

      // Per § 4 of the AVIF spec
      // https://aomediacodec.github.io/av1-avif/#auxiliary-images: An AV1 Alpha
      // Image Item […] shall be encoded with the same bit depth as the
      // associated master AV1 Image Item
      if (mPicture->p.bpc != mAlphaPlane->p.bpc) {
        return AsVariant(NonDecoderResult::AlphaYColorDepthMismatch);
      }
    }

    MOZ_ASSERT_IF(mAlphaPlane.isNothing(), !parsedImg->premultiplied_alpha);
    mDecodedData.emplace(
        Dav1dPictureToYCbCrAData(mPicture.ptr(), mAlphaPlane.ptrOr(nullptr),
                                 parsedImg->premultiplied_alpha));

    return AsVariant(r);
  }

 private:
  explicit Dav1dDecoder(UniquePtr<AVIFParser>&& aParser)
      : AVIFDecoderInterface(std::move(aParser)) {
    MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("Create Dav1dDecoder=%p", this));
  }

  Dav1dResult Init() {
    MOZ_ASSERT(!mContext);

    Dav1dSettings settings;
    dav1d_default_settings(&settings);
    settings.all_layers = 0;
    // TODO: tune settings a la DAV1DDecoder for AV1 (Bug 1681816)

    return dav1d_open(&mContext, &settings);
  }

  Dav1dResult GetPicture(Mp4parseByteData* aBytes, Dav1dPicture* aPicture,
                         bool aIsMetadataDecode) {
    MOZ_ASSERT(mContext);
    MOZ_ASSERT(aBytes);
    MOZ_ASSERT(aPicture);

    Dav1dData dav1dData;
    Dav1dResult r = dav1d_data_wrap(&dav1dData, aBytes->data, aBytes->length,
                                    Dav1dFreeCallback_s, nullptr);

    MOZ_LOG(sAVIFLog, r == 0 ? LogLevel::Verbose : LogLevel::Error,
            ("[this=%p] dav1d_data_wrap(%p, %zu) -> %d", this, dav1dData.data,
             dav1dData.sz, r));

    if (r != 0) {
      return r;
    }

    r = dav1d_send_data(mContext, &dav1dData);

    MOZ_LOG(sAVIFLog, r == 0 ? LogLevel::Debug : LogLevel::Error,
            ("[this=%p] dav1d_send_data -> %d", this, r));

    if (r != 0) {
      return r;
    }

    r = dav1d_get_picture(mContext, aPicture);

    MOZ_LOG(sAVIFLog, r == 0 ? LogLevel::Debug : LogLevel::Error,
            ("[this=%p] dav1d_get_picture -> %d", this, r));

    // When bug 1682662 is fixed, revise this assert and subsequent condition
    MOZ_ASSERT(aIsMetadataDecode || r == 0);

    // We already have the AVIF_DECODE_RESULT histogram to record all the
    // successful calls, so only bother recording what type of errors we see
    // via events. Unlike AOM, dav1d returns an int, not an enum, so this is the
    // easiest way to see if we're getting unexpected behavior to investigate.
    if (aIsMetadataDecode && r != 0) {
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
    // The buf is managed by the mParser inside Dav1dDecoder itself. Do nothing
    // here.
  }

  static layers::PlanarYCbCrAData Dav1dPictureToYCbCrAData(
      Dav1dPicture* aPicture, Dav1dPicture* aAlphaPlane,
      bool aPremultipliedAlpha);

  Dav1dContext* mContext = nullptr;

  // The pictures are allocated once Decode() succeeds and will be deallocated
  // when Dav1dDecoder is destroyed
  Maybe<Dav1dPicture> mPicture;
  Maybe<Dav1dPicture> mAlphaPlane;
};

class AOMDecoder final : AVIFDecoderInterface {
 public:
  ~AOMDecoder() {
    MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("Destroy AOMDecoder=%p", this));

    if (mContext.isSome()) {
      aom_codec_err_t r = aom_codec_destroy(mContext.ptr());
      MOZ_LOG(sAVIFLog, LogLevel::Debug,
              ("[this=%p] aom_codec_destroy -> %d", this, r));
    }
  }

  static DecodeResult Create(UniquePtr<AVIFParser>&& aParser,
                             UniquePtr<AVIFDecoderInterface>& aDecoder) {
    UniquePtr<AOMDecoder> d(new AOMDecoder(std::move(aParser)));
    aom_codec_err_t e = d->Init();
    if (e == AOM_CODEC_OK) {
      MOZ_ASSERT(d->mContext);
      aDecoder.reset(d.release());
    }
    return AsVariant(AOMResult(e));
  }

  DecodeResult Decode(bool aIsMetadataDecode) override {
    MOZ_ASSERT(mParser);
    MOZ_ASSERT(mContext.isSome());
    MOZ_ASSERT(mDecodedData.isNothing());

    AvifImage* parsedImg = mParser->GetImage();
    if (!parsedImg || !parsedImg->primary_item.data ||
        !parsedImg->primary_item.length) {
      return AsVariant(NonDecoderResult::NoPrimaryItem);
    }

    aom_image_t* aomImg = nullptr;
    DecodeResult r =
        GetImage(parsedImg->primary_item, &aomImg, aIsMetadataDecode);
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

    if (parsedImg->alpha_item.data && parsedImg->alpha_item.length) {
      aom_image_t* alphaImg = nullptr;
      DecodeResult r =
          GetImage(parsedImg->alpha_item, &alphaImg, aIsMetadataDecode);
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
      // https://aomediacodec.github.io/av1-avif/#auxiliary-images: An AV1 Alpha
      // Image Item […] shall be encoded with the same bit depth as the
      // associated master AV1 Image Item
      MOZ_ASSERT(mOwnedImage->GetImage() && mOwnedAlphaPlane->GetImage());
      if (mOwnedImage->GetImage()->bit_depth !=
          mOwnedAlphaPlane->GetImage()->bit_depth) {
        return AsVariant(NonDecoderResult::AlphaYColorDepthMismatch);
      }
    }

    MOZ_ASSERT_IF(!mOwnedAlphaPlane, !parsedImg->premultiplied_alpha);
    mDecodedData.emplace(AOMImageToYCbCrAData(
        mOwnedImage->GetImage(),
        mOwnedAlphaPlane ? mOwnedAlphaPlane->GetImage() : nullptr,
        parsedImg->premultiplied_alpha));

    return r;
  }

 private:
  explicit AOMDecoder(UniquePtr<AVIFParser>&& aParser)
      : AVIFDecoderInterface(std::move(aParser)) {
    MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("Create AOMDecoder=%p", this));
  }

  aom_codec_err_t Init() {
    MOZ_ASSERT(mContext.isNothing());

    aom_codec_iface_t* iface = aom_codec_av1_dx();
    mContext.emplace();
    aom_codec_err_t r = aom_codec_dec_init(
        mContext.ptr(), iface, /* cfg = */ nullptr, /* flags = */ 0);

    MOZ_LOG(sAVIFLog, r == AOM_CODEC_OK ? LogLevel::Verbose : LogLevel::Error,
            ("[this=%p] aom_codec_dec_init -> %d, name = %s", this, r,
             mContext->name));

    if (r != AOM_CODEC_OK) {
      mContext.reset();
    }

    return r;
  }

  DecodeResult GetImage(Mp4parseByteData& aData, aom_image_t** aImage,
                        bool aIsMetadataDecode) {
    MOZ_ASSERT(mContext.isSome());

    aom_codec_err_t r =
        aom_codec_decode(mContext.ptr(), aData.data, aData.length, nullptr);

    MOZ_LOG(sAVIFLog, r == AOM_CODEC_OK ? LogLevel::Verbose : LogLevel::Error,
            ("[this=%p] aom_codec_decode -> %d", this, r));

    if (aIsMetadataDecode) {
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
    aom_image_t* img = aom_codec_get_frame(mContext.ptr(), &iter);

    MOZ_LOG(sAVIFLog, img == nullptr ? LogLevel::Error : LogLevel::Verbose,
            ("[this=%p] aom_codec_get_frame -> %p", this, img));

    if (img == nullptr) {
      return AsVariant(AOMResult(NonAOMCodecError::NoFrame));
    }

    const CheckedInt<int> decoded_width = img->d_w;
    const CheckedInt<int> decoded_height = img->d_h;

    if (!decoded_height.isValid() || !decoded_width.isValid()) {
      MOZ_LOG(sAVIFLog, LogLevel::Debug,
              ("[this=%p] image dimensions can't be stored in int: d_w: %u, "
               "d_h: %u",
               this, img->d_w, img->d_h));
      return AsVariant(AOMResult(NonAOMCodecError::SizeOverflow));
    }

    *aImage = img;
    return AsVariant(AOMResult(r));
  }

  class OwnedAOMImage {
   public:
    ~OwnedAOMImage() {
      MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("Destroy OwnedAOMImage=%p", this));
    };

    static OwnedAOMImage* CopyFrom(aom_image_t* aImage, bool aIsAlpha) {
      MOZ_ASSERT(aImage);
      UniquePtr<OwnedAOMImage> img(new OwnedAOMImage());
      if (!img->CloneFrom(aImage, aIsAlpha)) {
        return nullptr;
      }
      return img.release();
    }

    aom_image_t* GetImage() { return mImage.isSome() ? mImage.ptr() : nullptr; }

   private:
    OwnedAOMImage() {
      MOZ_LOG(sAVIFLog, LogLevel::Verbose, ("Create OwnedAOMImage=%p", this));
    };

    bool CloneFrom(aom_image_t* aImage, bool aIsAlpha) {
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

    // The mImage's planes are referenced to mBuffer
    Maybe<aom_image_t> mImage;
    UniquePtr<uint8_t[]> mBuffer;
  };

  static layers::PlanarYCbCrAData AOMImageToYCbCrAData(
      aom_image_t* aImage, aom_image_t* aAlphaPlane, bool aPremultipliedAlpha);

  Maybe<aom_codec_ctx_t> mContext;
  UniquePtr<OwnedAOMImage> mOwnedImage;
  UniquePtr<OwnedAOMImage> mOwnedAlphaPlane;
};

/* static */
layers::PlanarYCbCrAData Dav1dDecoder::Dav1dPictureToYCbCrAData(
    Dav1dPicture* aPicture, Dav1dPicture* aAlphaPlane,
    bool aPremultipliedAlpha) {
  MOZ_ASSERT(aPicture);

  static_assert(std::is_same<int, decltype(aPicture->p.w)>::value);
  static_assert(std::is_same<int, decltype(aPicture->p.h)>::value);

  layers::PlanarYCbCrAData data;

  data.mYChannel = static_cast<uint8_t*>(aPicture->data[0]);
  data.mYStride = aPicture->stride[0];
  data.mYSize = gfx::IntSize(aPicture->p.w, aPicture->p.h);
  data.mYSkip = aPicture->stride[0] - aPicture->p.w;
  data.mCbChannel = static_cast<uint8_t*>(aPicture->data[1]);
  data.mCrChannel = static_cast<uint8_t*>(aPicture->data[2]);
  data.mCbCrStride = aPicture->stride[1];

  switch (aPicture->p.layout) {
    case DAV1D_PIXEL_LAYOUT_I400:  // Monochrome, so no Cb or Cr channels
      data.mCbCrSize = gfx::IntSize(0, 0);
      break;
    case DAV1D_PIXEL_LAYOUT_I420:
      data.mCbCrSize =
          gfx::IntSize((aPicture->p.w + 1) / 2, (aPicture->p.h + 1) / 2);
      break;
    case DAV1D_PIXEL_LAYOUT_I422:
      data.mCbCrSize = gfx::IntSize((aPicture->p.w + 1) / 2, aPicture->p.h);
      break;
    case DAV1D_PIXEL_LAYOUT_I444:
      data.mCbCrSize = gfx::IntSize(aPicture->p.w, aPicture->p.h);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown pixel layout");
  }

  data.mCbSkip = aPicture->stride[1] - aPicture->p.w;
  data.mCrSkip = aPicture->stride[1] - aPicture->p.w;
  data.mPicX = 0;
  data.mPicY = 0;
  data.mPicSize = data.mYSize;
  data.mStereoMode = StereoMode::MONO;
  data.mColorDepth = ColorDepthForBitDepth(aPicture->p.bpc);

  auto colorSpace = GetColorSpace(*aPicture, sAVIFLog);
  if (!colorSpace) {
    // MIAF specific: UNKNOWN color space should be treated as BT601
    colorSpace = Some(gfx::YUVColorSpace::BT601);
  }
  data.mYUVColorSpace = *colorSpace;

  data.mColorRange = aPicture->seq_hdr->color_range ? gfx::ColorRange::FULL
                                                    : gfx::ColorRange::LIMITED;

  if (aAlphaPlane) {
    MOZ_ASSERT(aAlphaPlane->stride[0] == data.mYStride);
    data.mAlphaChannel = static_cast<uint8_t*>(aAlphaPlane->data[0]);
    data.mAlphaSize = gfx::IntSize(aAlphaPlane->p.w, aAlphaPlane->p.h);
    data.mPremultipliedAlpha = aPremultipliedAlpha;
  }

  return data;
}

/* static */
layers::PlanarYCbCrAData AOMDecoder::AOMImageToYCbCrAData(
    aom_image_t* aImage, aom_image_t* aAlphaPlane, bool aPremultipliedAlpha) {
  MOZ_ASSERT(aImage);
  MOZ_ASSERT(aImage->stride[AOM_PLANE_Y] == aImage->stride[AOM_PLANE_ALPHA]);
  MOZ_ASSERT(aImage->stride[AOM_PLANE_Y] >=
             aom_img_plane_width(aImage, AOM_PLANE_Y));
  MOZ_ASSERT(aImage->stride[AOM_PLANE_U] == aImage->stride[AOM_PLANE_V]);
  MOZ_ASSERT(aImage->stride[AOM_PLANE_U] >=
             aom_img_plane_width(aImage, AOM_PLANE_U));
  MOZ_ASSERT(aImage->stride[AOM_PLANE_V] >=
             aom_img_plane_width(aImage, AOM_PLANE_V));
  MOZ_ASSERT(aom_img_plane_width(aImage, AOM_PLANE_U) ==
             aom_img_plane_width(aImage, AOM_PLANE_V));
  MOZ_ASSERT(aom_img_plane_height(aImage, AOM_PLANE_U) ==
             aom_img_plane_height(aImage, AOM_PLANE_V));

  layers::PlanarYCbCrAData data;

  data.mYChannel = aImage->planes[AOM_PLANE_Y];
  data.mYStride = aImage->stride[AOM_PLANE_Y];
  data.mYSize = gfx::IntSize(aom_img_plane_width(aImage, AOM_PLANE_Y),
                             aom_img_plane_height(aImage, AOM_PLANE_Y));
  data.mYSkip =
      aImage->stride[AOM_PLANE_Y] - aom_img_plane_width(aImage, AOM_PLANE_Y);
  data.mCbChannel = aImage->planes[AOM_PLANE_U];
  data.mCrChannel = aImage->planes[AOM_PLANE_V];
  data.mCbCrStride = aImage->stride[AOM_PLANE_U];
  data.mCbCrSize = gfx::IntSize(aom_img_plane_width(aImage, AOM_PLANE_U),
                                aom_img_plane_height(aImage, AOM_PLANE_U));
  data.mCbSkip =
      aImage->stride[AOM_PLANE_U] - aom_img_plane_width(aImage, AOM_PLANE_U);
  data.mCrSkip =
      aImage->stride[AOM_PLANE_V] - aom_img_plane_width(aImage, AOM_PLANE_V);
  data.mPicX = 0;
  data.mPicY = 0;
  data.mPicSize = gfx::IntSize(aImage->d_w, aImage->d_h);
  data.mStereoMode = StereoMode::MONO;
  data.mColorDepth = ColorDepthForBitDepth(aImage->bit_depth);

  Maybe<gfx::YUVColorSpace> colorSpace;
  switch (aImage->mc) {
    case AOM_CICP_MC_BT_601:
      colorSpace = Some(gfx::YUVColorSpace::BT601);
      break;
    case AOM_CICP_MC_BT_709:
      colorSpace = Some(gfx::YUVColorSpace::BT709);
      break;
    case AOM_CICP_MC_BT_2020_NCL:
      colorSpace = Some(gfx::YUVColorSpace::BT2020);
      break;
    case AOM_CICP_MC_BT_2020_CL:
      colorSpace = Some(gfx::YUVColorSpace::BT2020);
      break;
    case AOM_CICP_MC_IDENTITY:
      colorSpace = Some(gfx::YUVColorSpace::Identity);
      break;
    case AOM_CICP_MC_CHROMAT_NCL:
    case AOM_CICP_MC_CHROMAT_CL:
    case AOM_CICP_MC_UNSPECIFIED:  // MIAF specific
      switch (aImage->cp) {
        case AOM_CICP_CP_BT_601:
          colorSpace = Some(gfx::YUVColorSpace::BT601);
          break;
        case AOM_CICP_CP_BT_709:
          colorSpace = Some(gfx::YUVColorSpace::BT709);
          break;
        case AOM_CICP_CP_BT_2020:
          colorSpace = Some(gfx::YUVColorSpace::BT2020);
          break;
        default:
          break;
      }
      break;
    default:
      MOZ_LOG(sAVIFLog, LogLevel::Debug,
              ("unsupported aom_matrix_coefficients value: %u", aImage->mc));
      break;
  }
  if (!colorSpace) {
    // MIAF specific: UNKNOWN color space should be treated as BT601
    colorSpace = Some(gfx::YUVColorSpace::BT601);
  }
  data.mYUVColorSpace = *colorSpace;

  switch (aImage->range) {
    case AOM_CR_STUDIO_RANGE:
      data.mColorRange = gfx::ColorRange::LIMITED;
      break;
    case AOM_CR_FULL_RANGE:
      data.mColorRange = gfx::ColorRange::FULL;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unknown color range");
  }

  if (aAlphaPlane) {
    MOZ_ASSERT(aAlphaPlane->stride[AOM_PLANE_Y] == data.mYStride);
    data.mAlphaChannel = aAlphaPlane->planes[AOM_PLANE_Y];
    data.mAlphaSize = gfx::IntSize(aAlphaPlane->d_w, aAlphaPlane->d_h);
    data.mPremultipliedAlpha = aPremultipliedAlpha;
  }

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
  DecodeResult result = Decode(aIterator, aOnResume);

  RecordDecodeResultTelemetry(result);

  if (result.is<NonDecoderResult>()) {
    NonDecoderResult r = result.as<NonDecoderResult>();
    if (r == NonDecoderResult::NeedMoreData) {
      return LexerResult(Yield::NEED_MORE_DATA);
    }
    return r == NonDecoderResult::MetadataOk
               ? LexerResult(TerminalState::SUCCESS)
               : LexerResult(TerminalState::FAILURE);
  }

  MOZ_ASSERT(result.is<Dav1dResult>() || result.is<AOMResult>());
  return LexerResult(IsDecodeSuccess(result) ? TerminalState::SUCCESS
                                             : TerminalState::FAILURE);
}

nsAVIFDecoder::DecodeResult nsAVIFDecoder::Decode(
    SourceBufferIterator& aIterator, IResumable* aOnResume) {
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

  Mp4parseIo io = {nsAVIFDecoder::ReadSource, this};
  UniquePtr<AVIFParser> parser(AVIFParser::Create(&io));
  if (!parser) {
    return AsVariant(NonDecoderResult::ParseError);
  }

  UniquePtr<AVIFDecoderInterface> decoder;
  DecodeResult r = StaticPrefs::image_avif_use_dav1d()
                       ? Dav1dDecoder::Create(std::move(parser), decoder)
                       : AOMDecoder::Create(std::move(parser), decoder);

  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] Create %sDecoder %ssuccessfully", this,
           StaticPrefs::image_avif_use_dav1d() ? "Dav1d" : "AOM",
           IsDecodeSuccess(r) ? "" : "un"));

  if (!IsDecodeSuccess(r)) {
    return r;
  }

  MOZ_ASSERT(decoder);
  r = decoder->Decode(IsMetadataDecode());
  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] Decoder%s->Decode() %s", this,
           StaticPrefs::image_avif_use_dav1d() ? "Dav1d" : "AOM",
           IsDecodeSuccess(r) ? "succeeds" : "fails"));

  if (!IsDecodeSuccess(r)) {
    return r;
  }

  layers::PlanarYCbCrAData& decodedData = decoder->GetDecodedData();

  // Technically it's valid but we don't handle it now (Bug 1682318).
  if (decodedData.hasAlpha() && decodedData.mAlphaSize != decodedData.mYSize) {
    return AsVariant(NonDecoderResult::AlphaYSizeMismatch);
  }

  PostSize(decodedData.mPicSize.width, decodedData.mPicSize.height);

  const bool hasAlpha = decodedData.hasAlpha();
  if (hasAlpha) {
    PostHasTransparency();
  }

  if (IsMetadataDecode()) {
    return AsVariant(NonDecoderResult::MetadataOk);
  }

  // These data must be recorded after metadata has been decoded
  // (IsMetadataDecode()=false) or else they would be double-counted.
  AccumulateCategorical(
      gColorSpaceLabel[static_cast<size_t>(decodedData.mYUVColorSpace)]);
  AccumulateCategorical(
      gColorDepthLabel[static_cast<size_t>(decodedData.mColorDepth)]);

  IntSize rgbSize = Size();
  MOZ_ASSERT(rgbSize == decodedData.mPicSize);

  // Get suggested format and size. Note that GetYCbCrToRGBDestFormatAndSize
  // force format to be B8G8R8X8 if it's not.
  gfx::SurfaceFormat format = SurfaceFormat::OS_RGBX;
  gfx::GetYCbCrToRGBDestFormatAndSize(decodedData, format, rgbSize);
  if (hasAlpha) {
    // We would use libyuv to do the YCbCrA -> ARGB convertion, which only works
    // for B8G8R8A8.
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

  if (hasAlpha) {
    const auto wantPremultiply =
        !bool(GetSurfaceFlags() & SurfaceFlags::NO_PREMULTIPLY_ALPHA);
    const bool& hasPremultiply = decodedData.mPremultipliedAlpha;

    PremultFunc premultOp = nullptr;
    if (wantPremultiply && !hasPremultiply) {
      premultOp = libyuv::ARGBAttenuate;
    } else if (!wantPremultiply && hasPremultiply) {
      premultOp = libyuv::ARGBUnattenuate;
    }

    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] calling gfx::ConvertYCbCrAToARGB premultOp: %p", this,
             premultOp));
    gfx::ConvertYCbCrAToARGB(decodedData, format, rgbSize, rgbBuf.get(),
                             rgbStride.value(), premultOp);
  } else {
    MOZ_LOG(sAVIFLog, LogLevel::Debug,
            ("[this=%p] calling gfx::ConvertYCbCrToRGB", this));
    gfx::ConvertYCbCrToRGB(decodedData, format, rgbSize, rgbBuf.get(),
                           rgbStride.value());
  }

  MOZ_LOG(sAVIFLog, LogLevel::Debug,
          ("[this=%p] calling SurfacePipeFactory::CreateSurfacePipe", this));
  Maybe<SurfacePipe> pipe = SurfacePipeFactory::CreateSurfacePipe(
      this, rgbSize, OutputSize(), FullFrame(), format, format, Nothing(),
      nullptr, SurfacePipeFlags());

  if (!pipe) {
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
    PostFrameStop(hasAlpha ? Opacity::SOME_TRANSPARENCY
                           : Opacity::FULLY_OPAQUE);
    PostDecodeDone();
    return r;
  }

  return AsVariant(NonDecoderResult::WriteBufferError);
}

/* static */
bool nsAVIFDecoder::IsDecodeSuccess(const DecodeResult& aResult) {
  if (aResult.is<Dav1dResult>() || aResult.is<AOMResult>()) {
    return aResult == DecodeResult(Dav1dResult(0)) ||
           aResult == DecodeResult(AOMResult(AOM_CODEC_OK));
  }
  return false;
}

void nsAVIFDecoder::RecordDecodeResultTelemetry(
    const nsAVIFDecoder::DecodeResult& aResult) {
  if (aResult.is<NonDecoderResult>()) {
    switch (aResult.as<NonDecoderResult>()) {
      case NonDecoderResult::NeedMoreData:
        break;
      case NonDecoderResult::MetadataOk:
        break;
      case NonDecoderResult::ParseError:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::parse_error);
        break;
      case NonDecoderResult::NoPrimaryItem:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::no_primary_item);
        break;
      case NonDecoderResult::SizeOverflow:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::size_overflow);
        break;
      case NonDecoderResult::OutOfMemory:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::out_of_memory);
        break;
      case NonDecoderResult::PipeInitError:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::pipe_init_error);
        break;
      case NonDecoderResult::WriteBufferError:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::write_buffer_error);
        break;
      case NonDecoderResult::AlphaYSizeMismatch:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::alpha_y_sz_mismatch);
        break;
      case NonDecoderResult::AlphaYColorDepthMismatch:
        AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::alpha_y_bpc_mismatch);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("unknown result");
        break;
    }
  } else {
    MOZ_ASSERT(aResult.is<Dav1dResult>() || aResult.is<AOMResult>());
    AccumulateCategorical(aResult.is<Dav1dResult>() ? LABELS_AVIF_DECODER::dav1d
                                                    : LABELS_AVIF_DECODER::aom);
    AccumulateCategorical(IsDecodeSuccess(aResult)
                              ? LABELS_AVIF_DECODE_RESULT::success
                              : LABELS_AVIF_DECODE_RESULT::decode_error);
  }
}

}  // namespace image
}  // namespace mozilla
