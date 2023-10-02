/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VideoDecoder.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/VideoDecoderBinding.h"

#include "DecoderTraits.h"
#include "GPUVideoImage.h"
#include "H264.h"
#include "ImageContainer.h"
#include "MediaContainerType.h"
#include "MediaData.h"
#include "VideoUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Try.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/EncodedVideoChunk.h"
#include "mozilla/dom/EncodedVideoChunkBinding.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/ImageUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/VideoFrame.h"
#include "mozilla/dom/VideoFrameBinding.h"
#include "mozilla/dom/WebCodecsUtils.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/media/MediaUtils.h"
#include "nsGkAtoms.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"

#ifdef XP_MACOSX
#  include "MacIOSurfaceImage.h"
#elif MOZ_WAYLAND
#  include "mozilla/layers/DMABUFSurfaceImage.h"
#  include "mozilla/widget/DMABufSurface.h"
#endif

mozilla::LazyLogModule gWebCodecsLog("WebCodecs");

namespace mozilla::dom {

#ifdef LOG_INTERNAL
#  undef LOG_INTERNAL
#endif  // LOG_INTERNAL
#define LOG_INTERNAL(level, msg, ...) \
  MOZ_LOG(gWebCodecsLog, LogLevel::level, (msg, ##__VA_ARGS__))

#ifdef LOG
#  undef LOG
#endif  // LOG
#define LOG(msg, ...) LOG_INTERNAL(Debug, msg, ##__VA_ARGS__)

#ifdef LOGW
#  undef LOGW
#endif  // LOGW
#define LOGW(msg, ...) LOG_INTERNAL(Warning, msg, ##__VA_ARGS__)

#ifdef LOGE
#  undef LOGE
#endif  // LOGE
#define LOGE(msg, ...) LOG_INTERNAL(Error, msg, ##__VA_ARGS__)

#ifdef LOGV
#  undef LOGV
#endif  // LOGV
#define LOGV(msg, ...) LOG_INTERNAL(Verbose, msg, ##__VA_ARGS__)

NS_IMPL_CYCLE_COLLECTION_INHERITED(VideoDecoder, DOMEventTargetHelper,
                                   mErrorCallback, mOutputCallback)
NS_IMPL_ADDREF_INHERITED(VideoDecoder, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(VideoDecoder, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(VideoDecoder)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/*
 * The followings are helpers for VideoDecoder methods
 */

static Maybe<nsString> ParseCodecString(const nsAString& aCodec) {
  // Trim the spaces on each end.
  nsString str(aCodec);
  str.Trim(" ");
  nsTArray<nsString> codecs;
  if (!ParseCodecsString(str, codecs) || codecs.Length() != 1 ||
      codecs[0] != str) {
    return Nothing();
  }
  return Some(codecs[0]);
}

// https://w3c.github.io/webcodecs/#valid-videodecoderconfig
static Result<Ok, nsCString> Validate(const VideoDecoderConfig& aConfig) {
  Maybe<nsString> codec = ParseCodecString(aConfig.mCodec);
  if (!codec || codec->IsEmpty()) {
    return Err("invalid codec string"_ns);
  }

  if (aConfig.mCodedWidth.WasPassed() != aConfig.mCodedHeight.WasPassed()) {
    return aConfig.mCodedWidth.WasPassed()
               ? Err("Invalid VideoDecoderConfig: codedWidth passed without codedHeight"_ns)
               : Err("Invalid VideoDecoderConfig: codedHeight passed without codedWidth"_ns);
  }
  if (aConfig.mCodedWidth.WasPassed() &&
      (aConfig.mCodedWidth.Value() == 0 || aConfig.mCodedHeight.Value() == 0)) {
    return Err("codedWidth and/or codedHeight can't be zero"_ns);
  }

  if (aConfig.mDisplayAspectWidth.WasPassed() !=
      aConfig.mDisplayAspectHeight.WasPassed()) {
    return Err(
        "display aspect width or height cannot be set without the other"_ns);
  }
  if (aConfig.mDisplayAspectWidth.WasPassed() &&
      (aConfig.mDisplayAspectWidth.Value() == 0 ||
       aConfig.mDisplayAspectHeight.Value() == 0)) {
    return Err("display aspect width and height cannot be zero"_ns);
  }

  return Ok();
}

// MOZ_IMPLICIT as GuessMIMETypes, CanDecode, and GetTracksInfo are intended to
// called by both VideoDecoderConfig and VideoDecoderConfigInternal via
// MIMECreateParam.
struct MIMECreateParam {
  MOZ_IMPLICIT MIMECreateParam(const VideoDecoderConfigInternal& aConfig)
      : mParsedCodec(ParseCodecString(aConfig.mCodec).valueOr(EmptyString())),
        mWidth(aConfig.mCodedWidth),
        mHeight(aConfig.mCodedHeight) {}
  MOZ_IMPLICIT MIMECreateParam(const VideoDecoderConfig& aConfig)
      : mParsedCodec(ParseCodecString(aConfig.mCodec).valueOr(EmptyString())),
        mWidth(OptionalToMaybe(aConfig.mCodedWidth)),
        mHeight(OptionalToMaybe(aConfig.mCodedHeight)) {}

  const nsString mParsedCodec;
  const Maybe<uint32_t> mWidth;
  const Maybe<uint32_t> mHeight;
};

static nsTArray<nsCString> GuessMIMETypes(MIMECreateParam aParam) {
  const auto codec = NS_ConvertUTF16toUTF8(aParam.mParsedCodec);
  nsTArray<nsCString> types;
  for (const nsCString& container : GuessContainers(aParam.mParsedCodec)) {
    nsPrintfCString mime("video/%s; codecs=%s", container.get(), codec.get());
    if (aParam.mWidth) {
      mime.Append(nsPrintfCString("; width=%d", *aParam.mWidth));
    }
    if (aParam.mHeight) {
      mime.Append(nsPrintfCString("; height=%d", *aParam.mHeight));
    }
    types.AppendElement(mime);
  }
  return types;
}

static bool IsOnAndroid() {
#if defined(ANDROID)
  return true;
#else
  return false;
#endif
}

static bool IsSupportedCodec(const nsAString& aCodec) {
  // H265 is unsupported.
  if (!IsAV1CodecString(aCodec) && !IsVP9CodecString(aCodec) &&
      !IsVP8CodecString(aCodec) && !IsH264CodecString(aCodec)) {
    return false;
  }

  // Gecko allows codec string starts with vp9 or av1 but Webcodecs requires to
  // starts with av01 and vp09.
  // https://www.w3.org/TR/webcodecs-codec-registry/#video-codec-registry
  if (StringBeginsWith(aCodec, u"vp9"_ns) ||
      StringBeginsWith(aCodec, u"av1"_ns)) {
    return false;
  }

  return true;
}

// https://w3c.github.io/webcodecs/#check-configuration-support
static bool CanDecode(MIMECreateParam aParam) {
  // TODO: Enable WebCodecs on Android (Bug 1840508)
  if (IsOnAndroid()) {
    return false;
  }
  if (!IsSupportedCodec(aParam.mParsedCodec)) {
    return false;
  }
  // TODO: Instead of calling CanHandleContainerType with the guessed the
  // containers, DecoderTraits should provide an API to tell if a codec is
  // decodable or not.
  for (const nsCString& mime : GuessMIMETypes(aParam)) {
    if (Maybe<MediaContainerType> containerType =
            MakeMediaExtendedMIMEType(mime)) {
      if (DecoderTraits::CanHandleContainerType(
              *containerType, nullptr /* DecoderDoctorDiagnostics */) !=
          CANPLAY_NO) {
        return true;
      }
    }
  }
  return false;
}

static nsTArray<UniquePtr<TrackInfo>> GetTracksInfo(MIMECreateParam aParam) {
  // TODO: Instead of calling GetTracksInfo with the guessed containers,
  // DecoderTraits should provide an API to create the TrackInfo directly.
  for (const nsCString& mime : GuessMIMETypes(aParam)) {
    if (Maybe<MediaContainerType> containerType =
            MakeMediaExtendedMIMEType(mime)) {
      if (nsTArray<UniquePtr<TrackInfo>> tracks =
              DecoderTraits::GetTracksInfo(*containerType);
          !tracks.IsEmpty()) {
        return tracks;
      }
    }
  }
  return {};
}

static Result<RefPtr<MediaByteBuffer>, nsresult> GetExtraData(
    const OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer) {
  RefPtr<MediaByteBuffer> data = MakeRefPtr<MediaByteBuffer>();
  Unused << AppendTypedArrayDataTo(aBuffer, *data);
  return data->Length() > 0 ? data : nullptr;
}

static Result<UniquePtr<TrackInfo>, nsresult> CreateVideoInfo(
    const VideoDecoderConfigInternal& aConfig) {
  LOG("Create a VideoInfo from %s config",
      NS_ConvertUTF16toUTF8(aConfig.mCodec).get());

  nsTArray<UniquePtr<TrackInfo>> tracks = GetTracksInfo(aConfig);
  if (tracks.Length() != 1 || tracks[0]->GetType() != TrackInfo::kVideoTrack) {
    LOGE("Failed to get TrackInfo");
    return Err(NS_ERROR_INVALID_ARG);
  }

  UniquePtr<TrackInfo> track(std::move(tracks[0]));
  VideoInfo* vi = track->GetAsVideoInfo();
  if (!vi) {
    LOGE("Failed to get VideoInfo");
    return Err(NS_ERROR_INVALID_ARG);
  }

  constexpr uint32_t MAX = static_cast<uint32_t>(
      std::numeric_limits<decltype(gfx::IntSize::width)>::max());
  if (aConfig.mCodedHeight.isSome()) {
    if (aConfig.mCodedHeight.value() > MAX) {
      LOGE("codedHeight overflows");
      return Err(NS_ERROR_INVALID_ARG);
    }
    vi->mImage.height = static_cast<decltype(gfx::IntSize::height)>(
        aConfig.mCodedHeight.value());
  }
  if (aConfig.mCodedWidth.isSome()) {
    if (aConfig.mCodedWidth.value() > MAX) {
      LOGE("codedWidth overflows");
      return Err(NS_ERROR_INVALID_ARG);
    }
    vi->mImage.width =
        static_cast<decltype(gfx::IntSize::width)>(aConfig.mCodedWidth.value());
  }

  if (aConfig.mDisplayAspectHeight.isSome()) {
    if (aConfig.mDisplayAspectHeight.value() > MAX) {
      LOGE("displayAspectHeight overflows");
      return Err(NS_ERROR_INVALID_ARG);
    }
    vi->mDisplay.height = static_cast<decltype(gfx::IntSize::height)>(
        aConfig.mDisplayAspectHeight.value());
  }
  if (aConfig.mDisplayAspectWidth.isSome()) {
    if (aConfig.mDisplayAspectWidth.value() > MAX) {
      LOGE("displayAspectWidth overflows");
      return Err(NS_ERROR_INVALID_ARG);
    }
    vi->mDisplay.width = static_cast<decltype(gfx::IntSize::width)>(
        aConfig.mDisplayAspectWidth.value());
  }

  if (aConfig.mColorSpace.isSome()) {
    const VideoColorSpaceInternal& colorSpace(aConfig.mColorSpace.value());
    if (colorSpace.mFullRange.isSome()) {
      vi->mColorRange = ToColorRange(colorSpace.mFullRange.value());
    }
    if (colorSpace.mMatrix.isSome()) {
      vi->mColorSpace.emplace(ToColorSpace(colorSpace.mMatrix.value()));
    }
    if (colorSpace.mPrimaries.isSome()) {
      vi->mColorPrimaries.emplace(ToPrimaries(colorSpace.mPrimaries.value()));
    }
    if (colorSpace.mTransfer.isSome()) {
      vi->mTransferFunction.emplace(
          ToTransferFunction(colorSpace.mTransfer.value()));
    }
  }

  if (aConfig.mDescription.isSome()) {
    RefPtr<MediaByteBuffer> buf;
    buf = aConfig.mDescription.value();
    if (buf) {
      LOG("The given config has %zu bytes of description data", buf->Length());
      if (vi->mExtraData) {
        LOGW("The default extra data is overwritten");
      }
      vi->mExtraData = buf;
    }

    // TODO: Make this utility and replace the similar one in MP4Demuxer.cpp.
    if (vi->mExtraData && !vi->mExtraData->IsEmpty() &&
        IsH264CodecString(aConfig.mCodec)) {
      SPSData spsdata;
      if (H264::DecodeSPSFromExtraData(vi->mExtraData.get(), spsdata) &&
          spsdata.pic_width > 0 && spsdata.pic_height > 0 &&
          H264::EnsureSPSIsSane(spsdata)) {
        LOG("H264 sps data - pic size: %d x %d, display size: %d x %d",
            spsdata.pic_width, spsdata.pic_height, spsdata.display_width,
            spsdata.display_height);

        if (spsdata.pic_width > MAX || spsdata.pic_height > MAX ||
            spsdata.display_width > MAX || spsdata.display_height > MAX) {
          LOGE("H264 width or height in sps data overflows");
          return Err(NS_ERROR_INVALID_ARG);
        }

        vi->mImage.width =
            static_cast<decltype(gfx::IntSize::width)>(spsdata.pic_width);
        vi->mImage.height =
            static_cast<decltype(gfx::IntSize::height)>(spsdata.pic_height);
        vi->mDisplay.width =
            static_cast<decltype(gfx::IntSize::width)>(spsdata.display_width);
        vi->mDisplay.height =
            static_cast<decltype(gfx::IntSize::height)>(spsdata.display_height);
      }
    }
  } else {
    vi->mExtraData = new MediaByteBuffer();
  }

  // By spec, it'ok to not set the coded-width and coded-height, but it makes
  // the image size empty here and our decoder cannot be initialized without a
  // valid image size. The image size will be set to the display's size if it
  // exists. Otherwise, a default size will be applied.
  const gfx::IntSize DEFAULT_IMG(640, 480);
  if (vi->mImage.width <= 0) {
    vi->mImage.width =
        vi->mDisplay.width >= 0 ? vi->mDisplay.width : DEFAULT_IMG.width;
    LOGW("image width is set to %d compulsively", vi->mImage.width);
  }
  if (vi->mImage.height <= 0) {
    vi->mImage.height =
        vi->mDisplay.height >= 0 ? vi->mDisplay.height : DEFAULT_IMG.height;
    LOGW("image height is set to %d compulsively", vi->mImage.height);
  }

  LOG("Create a VideoInfo - image: %d x %d, display: %d x %d, with extra-data: "
      "%s",
      vi->mImage.width, vi->mImage.height, vi->mDisplay.width,
      vi->mDisplay.height, vi->mExtraData ? "yes" : "no");

  return track;
}

static Result<Ok, nsresult> CloneConfiguration(
    RootedDictionary<VideoDecoderConfig>& aDest, JSContext* aCx,
    const VideoDecoderConfig& aConfig) {
  MOZ_ASSERT(Validate(aConfig).isOk());

  aDest.mCodec = aConfig.mCodec;
  if (aConfig.mCodedHeight.WasPassed()) {
    aDest.mCodedHeight.Construct(aConfig.mCodedHeight.Value());
  }
  if (aConfig.mCodedWidth.WasPassed()) {
    aDest.mCodedWidth.Construct(aConfig.mCodedWidth.Value());
  }
  if (aConfig.mColorSpace.WasPassed()) {
    aDest.mColorSpace.Construct(aConfig.mColorSpace.Value());
  }
  if (aConfig.mDescription.WasPassed()) {
    aDest.mDescription.Construct();
    MOZ_TRY(CloneBuffer(aCx, aDest.mDescription.Value(),
                        aConfig.mDescription.Value()));
  }
  if (aConfig.mDisplayAspectHeight.WasPassed()) {
    aDest.mDisplayAspectHeight.Construct(aConfig.mDisplayAspectHeight.Value());
  }
  if (aConfig.mDisplayAspectWidth.WasPassed()) {
    aDest.mDisplayAspectWidth.Construct(aConfig.mDisplayAspectWidth.Value());
  }
  aDest.mHardwareAcceleration = aConfig.mHardwareAcceleration;
  if (aConfig.mOptimizeForLatency.WasPassed()) {
    aDest.mOptimizeForLatency.Construct(aConfig.mOptimizeForLatency.Value());
  }

  return Ok();
}

static nsresult FireEvent(DOMEventTargetHelper* aEventTarget,
                          nsAtom* aTypeWithOn, const nsAString& aEventType) {
  MOZ_ASSERT(aEventTarget);

  if (aTypeWithOn && !aEventTarget->HasListenersFor(aTypeWithOn)) {
    LOGV("EventTarget %p has no %s event listener", aEventTarget,
         NS_ConvertUTF16toUTF8(aEventType).get());
    return NS_ERROR_ABORT;
  }

  LOGV("Dispatch %s event to EventTarget %p",
       NS_ConvertUTF16toUTF8(aEventType).get(), aEventTarget);
  RefPtr<Event> event = new Event(aEventTarget, nullptr, nullptr);
  event->InitEvent(aEventType, true, true);
  event->SetTrusted(true);
  aEventTarget->DispatchEvent(*event);
  return NS_OK;
}

static Maybe<VideoPixelFormat> GuessPixelFormat(layers::Image* aImage) {
  if (aImage) {
    // TODO: Implement ImageUtils::Impl for MacIOSurfaceImage and
    // DMABUFSurfaceImage?
    if (aImage->AsPlanarYCbCrImage() || aImage->AsNVImage()) {
      const ImageUtils imageUtils(aImage);
      Maybe<VideoPixelFormat> f =
          ImageBitmapFormatToVideoPixelFormat(imageUtils.GetFormat());

      // ImageBitmapFormat cannot distinguish YUV420 or YUV420A.
      bool hasAlpha = aImage->AsPlanarYCbCrImage() &&
                      aImage->AsPlanarYCbCrImage()->GetData() &&
                      aImage->AsPlanarYCbCrImage()->GetData()->mAlpha;
      if (f && *f == VideoPixelFormat::I420 && hasAlpha) {
        return Some(VideoPixelFormat::I420A);
      }
      return f;
    }
    if (layers::GPUVideoImage* image = aImage->AsGPUVideoImage()) {
      RefPtr<layers::ImageBridgeChild> imageBridge =
          layers::ImageBridgeChild::GetSingleton();
      layers::TextureClient* texture = image->GetTextureClient(imageBridge);
      if (NS_WARN_IF(!texture)) {
        return Nothing();
      }
      return SurfaceFormatToVideoPixelFormat(texture->GetFormat());
    }
#ifdef XP_MACOSX
    if (layers::MacIOSurfaceImage* image = aImage->AsMacIOSurfaceImage()) {
      MOZ_ASSERT(image->GetSurface());
      return SurfaceFormatToVideoPixelFormat(image->GetSurface()->GetFormat());
    }
#endif
#ifdef MOZ_WAYLAND
    if (layers::DMABUFSurfaceImage* image = aImage->AsDMABUFSurfaceImage()) {
      MOZ_ASSERT(image->GetSurface());
      return SurfaceFormatToVideoPixelFormat(image->GetSurface()->GetFormat());
    }
#endif
  }
  LOGW("Failed to get pixel format from layers::Image");
  return Nothing();
}

static VideoColorSpaceInternal GuessColorSpace(
    const layers::PlanarYCbCrData* aData) {
  if (!aData) {
    return {};
  }

  VideoColorSpaceInternal colorSpace;
  colorSpace.mFullRange = Some(ToFullRange(aData->mColorRange));
  if (Maybe<VideoMatrixCoefficients> m =
          ToMatrixCoefficients(aData->mYUVColorSpace)) {
    colorSpace.mMatrix = Some(*m);
  }
  if (Maybe<VideoColorPrimaries> p = ToPrimaries(aData->mColorPrimaries)) {
    colorSpace.mPrimaries = Some(*p);
  }
  if (Maybe<VideoTransferCharacteristics> c =
          ToTransferCharacteristics(aData->mTransferFunction)) {
    colorSpace.mTransfer = Some(*c);
  }
  return colorSpace;
}

#ifdef XP_MACOSX
static VideoColorSpaceInternal GuessColorSpace(const MacIOSurface* aSurface) {
  if (!aSurface) {
    return {};
  }
  VideoColorSpaceInternal colorSpace;
  // TODO: Could ToFullRange(aSurface->GetColorRange()) conflict with the below?
  colorSpace.mFullRange = Some(aSurface->IsFullRange());
  if (Maybe<dom::VideoMatrixCoefficients> m =
          ToMatrixCoefficients(aSurface->GetYUVColorSpace())) {
    colorSpace.mMatrix = Some(*m);
  }
  if (Maybe<VideoColorPrimaries> p = ToPrimaries(aSurface->mColorPrimaries)) {
    colorSpace.mPrimaries = Some(*p);
  }
  // TODO: Track gfx::TransferFunction setting in
  // MacIOSurface::CreateNV12OrP010Surface to get colorSpace.mTransfer
  return colorSpace;
}
#endif
#ifdef MOZ_WAYLAND
// TODO: Set DMABufSurface::IsFullRange() to const so aSurface can be const.
static VideoColorSpaceInternal GuessColorSpace(DMABufSurface* aSurface) {
  if (!aSurface) {
    return {};
  }
  VideoColorSpaceInternal colorSpace;
  colorSpace.mFullRange = Some(aSurface->IsFullRange());
  if (Maybe<dom::VideoMatrixCoefficients> m =
          ToMatrixCoefficients(aSurface->GetYUVColorSpace())) {
    colorSpace.mMatrix = Some(*m);
  }
  // No other color space information.
  return colorSpace;
}
#endif

static VideoColorSpaceInternal GuessColorSpace(layers::Image* aImage) {
  if (aImage) {
    if (layers::PlanarYCbCrImage* image = aImage->AsPlanarYCbCrImage()) {
      return GuessColorSpace(image->GetData());
    }
    if (layers::NVImage* image = aImage->AsNVImage()) {
      return GuessColorSpace(image->GetData());
    }
#ifdef XP_MACOSX
    // TODO: Make sure VideoFrame can interpret its internal data in different
    // formats.
    if (layers::MacIOSurfaceImage* image = aImage->AsMacIOSurfaceImage()) {
      return GuessColorSpace(image->GetSurface());
    }
#endif
#ifdef MOZ_WAYLAND
    // TODO: Make sure VideoFrame can interpret its internal data in different
    // formats.
    if (layers::DMABUFSurfaceImage* image = aImage->AsDMABUFSurfaceImage()) {
      return GuessColorSpace(image->GetSurface());
    }
#endif
  }
  LOGW("Failed to get color space from layers::Image");
  return {};
}

static Result<gfx::IntSize, nsresult> AdjustDisplaySize(
    const uint32_t aDisplayAspectWidth, const uint32_t aDisplayAspectHeight,
    const gfx::IntSize& aDisplaySize) {
  if (aDisplayAspectHeight == 0) {
    return Err(NS_ERROR_ILLEGAL_VALUE);
  }

  const double aspectRatio =
      static_cast<double>(aDisplayAspectWidth) / aDisplayAspectHeight;

  double w = aDisplaySize.width;
  double h = aDisplaySize.height;

  if (aspectRatio >= w / h) {
    // Adjust w to match the aspect ratio
    w = aspectRatio * h;
  } else {
    // Adjust h to match the aspect ratio
    h = w / aspectRatio;
  }

  w = std::round(w);
  h = std::round(h);
  constexpr double MAX = static_cast<double>(
      std::numeric_limits<decltype(gfx::IntSize::width)>::max());
  if (w > MAX || h > MAX || w < 1.0 || h < 1.0) {
    return Err(NS_ERROR_ILLEGAL_VALUE);
  }
  return gfx::IntSize(static_cast<decltype(gfx::IntSize::width)>(w),
                      static_cast<decltype(gfx::IntSize::height)>(h));
}

// https://w3c.github.io/webcodecs/#create-a-videoframe
static RefPtr<VideoFrame> CreateVideoFrame(
    nsIGlobalObject* aGlobalObject, const VideoData* aData, int64_t aTimestamp,
    uint64_t aDuration, const Maybe<uint32_t> aDisplayAspectWidth,
    const Maybe<uint32_t> aDisplayAspectHeight,
    const VideoColorSpaceInternal& aColorSpace) {
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT(aData);
  MOZ_ASSERT((!!aDisplayAspectWidth) == (!!aDisplayAspectHeight));

  Maybe<VideoPixelFormat> format = GuessPixelFormat(aData->mImage.get());
  gfx::IntSize displaySize = aData->mDisplay;
  if (aDisplayAspectWidth && aDisplayAspectHeight) {
    auto r = AdjustDisplaySize(*aDisplayAspectWidth, *aDisplayAspectHeight,
                               displaySize);
    if (r.isOk()) {
      displaySize = r.unwrap();
    }
  }

  return MakeRefPtr<VideoFrame>(
      aGlobalObject, aData->mImage, format, aData->mImage->GetSize(),
      aData->mImage->GetPictureRect(), displaySize, Some(aDuration), aTimestamp,
      aColorSpace.ToColorSpaceInit());
}

static nsTArray<RefPtr<VideoFrame>> DecodedDataToVideoFrames(
    nsIGlobalObject* aGlobalObject, nsTArray<RefPtr<MediaData>>&& aData,
    VideoDecoderConfigInternal& aConfig) {
  nsTArray<RefPtr<VideoFrame>> frames;
  for (RefPtr<MediaData>& data : aData) {
    MOZ_RELEASE_ASSERT(data->mType == MediaData::Type::VIDEO_DATA);
    RefPtr<const VideoData> d(data->As<const VideoData>());
    VideoColorSpaceInternal colorSpace = GuessColorSpace(d->mImage.get());
    frames.AppendElement(CreateVideoFrame(
        aGlobalObject, d.get(), d->mTime.ToMicroseconds(),
        static_cast<uint64_t>(d->mDuration.ToMicroseconds()),
        aConfig.mDisplayAspectWidth, aConfig.mDisplayAspectHeight,
        aConfig.mColorSpace.isSome() ? aConfig.mColorSpace.value()
                                     : colorSpace));
  }
  return frames;
}

/*
 * Below are helper classes used in VideoDecoder
 */

VideoColorSpaceInternal::VideoColorSpaceInternal(
    const VideoColorSpaceInit& aColorSpaceInit) {
  mFullRange = NullableToMaybe(aColorSpaceInit.mFullRange);
  mMatrix = NullableToMaybe(aColorSpaceInit.mMatrix);
  mPrimaries = NullableToMaybe(aColorSpaceInit.mPrimaries);
  mTransfer = NullableToMaybe(aColorSpaceInit.mTransfer);
}

VideoColorSpaceInit VideoColorSpaceInternal::ToColorSpaceInit() const {
  VideoColorSpaceInit init;
  init.mFullRange = MaybeToNullable(mFullRange);
  init.mMatrix = MaybeToNullable(mMatrix);
  init.mPrimaries = MaybeToNullable(mPrimaries);
  init.mTransfer = MaybeToNullable(mTransfer);
  return init;
}

/* static */
UniquePtr<VideoDecoderConfigInternal> VideoDecoderConfigInternal::Create(
    const VideoDecoderConfig& aConfig) {
  if (auto r = Validate(aConfig); r.isErr()) {
    nsCString e = r.unwrapErr();
    LOGE("Failed to create VideoDecoderConfigInternal: %s", e.get());
    return nullptr;
  }

  Maybe<RefPtr<MediaByteBuffer>> description;
  if (aConfig.mDescription.WasPassed()) {
    auto rv = GetExtraData(aConfig.mDescription.Value());
    if (rv.isErr()) {  // Invalid description data.
      LOGE(
          "Failed to create VideoDecoderConfigInternal due to invalid "
          "description data. Error: 0x%08" PRIx32,
          static_cast<uint32_t>(rv.unwrapErr()));
      return nullptr;
    }
    description.emplace(rv.unwrap());
  }

  Maybe<VideoColorSpaceInternal> colorSpace;
  if (aConfig.mColorSpace.WasPassed()) {
    colorSpace.emplace(VideoColorSpaceInternal(aConfig.mColorSpace.Value()));
  }

  return UniquePtr<VideoDecoderConfigInternal>(new VideoDecoderConfigInternal(
      aConfig.mCodec, OptionalToMaybe(aConfig.mCodedHeight),
      OptionalToMaybe(aConfig.mCodedWidth), std::move(colorSpace),
      std::move(description), OptionalToMaybe(aConfig.mDisplayAspectHeight),
      OptionalToMaybe(aConfig.mDisplayAspectWidth),
      aConfig.mHardwareAcceleration,
      OptionalToMaybe(aConfig.mOptimizeForLatency)));
}

VideoDecoderConfigInternal::VideoDecoderConfigInternal(
    const nsAString& aCodec, Maybe<uint32_t>&& aCodedHeight,
    Maybe<uint32_t>&& aCodedWidth, Maybe<VideoColorSpaceInternal>&& aColorSpace,
    Maybe<RefPtr<MediaByteBuffer>>&& aDescription,
    Maybe<uint32_t>&& aDisplayAspectHeight,
    Maybe<uint32_t>&& aDisplayAspectWidth,
    const HardwareAcceleration& aHardwareAcceleration,
    Maybe<bool>&& aOptimizeForLatency)
    : mCodec(aCodec),
      mCodedHeight(std::move(aCodedHeight)),
      mCodedWidth(std::move(aCodedWidth)),
      mColorSpace(std::move(aColorSpace)),
      mDescription(std::move(aDescription)),
      mDisplayAspectHeight(std::move(aDisplayAspectHeight)),
      mDisplayAspectWidth(std::move(aDisplayAspectWidth)),
      mHardwareAcceleration(aHardwareAcceleration),
      mOptimizeForLatency(std::move(aOptimizeForLatency)) {}

template <typename T>
class MessageRequestHolder {
 public:
  MessageRequestHolder() = default;
  ~MessageRequestHolder() = default;

  MozPromiseRequestHolder<T>& Request() { return mRequest; }
  void Disconnect() { mRequest.Disconnect(); }
  void Complete() { mRequest.Complete(); }
  bool Exists() const { return mRequest.Exists(); }

 protected:
  MozPromiseRequestHolder<T> mRequest;
};

ControlMessage::ControlMessage(const nsACString& aTitle) : mTitle(aTitle) {}

class ConfigureMessage final
    : public ControlMessage,
      public MessageRequestHolder<DecoderAgent::ConfigurePromise> {
 public:
  using Id = DecoderAgent::Id;
  static constexpr Id NoId = 0;
  static ConfigureMessage* Create(
      UniquePtr<VideoDecoderConfigInternal>&& aConfig) {
    // This needs to be atomic since this can run on the main thread or worker
    // thread.
    static std::atomic<Id> sNextId = NoId;
    return new ConfigureMessage(++sNextId, std::move(aConfig));
  }

  ~ConfigureMessage() = default;
  virtual void Cancel() override { Disconnect(); }
  virtual bool IsProcessing() override { return Exists(); };
  virtual ConfigureMessage* AsConfigureMessage() override { return this; }
  const VideoDecoderConfigInternal& Config() { return *mConfig; }
  UniquePtr<VideoDecoderConfigInternal> TakeConfig() {
    return std::move(mConfig);
  }

  const Id mId;  // A unique id shown in log.

 private:
  ConfigureMessage(Id aId, UniquePtr<VideoDecoderConfigInternal>&& aConfig)
      : ControlMessage(
            nsPrintfCString("configure #%d (%s)", aId,
                            NS_ConvertUTF16toUTF8(aConfig->mCodec).get())),
        mId(aId),
        mConfig(std::move(aConfig)) {}

  UniquePtr<VideoDecoderConfigInternal> mConfig;
};

class DecodeMessage final
    : public ControlMessage,
      public MessageRequestHolder<DecoderAgent::DecodePromise> {
 public:
  using Id = size_t;

  DecodeMessage(Id aId, ConfigureMessage::Id aConfigId,
                UniquePtr<EncodedVideoChunkData>&& aData)
      : ControlMessage(
            nsPrintfCString("decode #%zu (config #%d)", aId, aConfigId)),
        mId(aId),
        mData(std::move(aData)) {}
  ~DecodeMessage() = default;
  virtual void Cancel() override { Disconnect(); }
  virtual bool IsProcessing() override { return Exists(); };
  virtual DecodeMessage* AsDecodeMessage() override { return this; }
  already_AddRefed<MediaRawData> TakeData(
      const RefPtr<MediaByteBuffer>& aExtraData,
      const VideoDecoderConfigInternal& aConfig) {
    if (!mData) {
      LOGE("No data in DecodeMessage");
      return nullptr;
    }

    RefPtr<MediaRawData> sample = mData->TakeData();
    if (!sample) {
      LOGE("Take no data in DecodeMessage");
      return nullptr;
    }

    // aExtraData is either provided by Configure() or a default one created for
    // the decoder creation. If it's created for decoder creation only, we don't
    // set it to sample.
    if (aConfig.mDescription && aExtraData) {
      sample->mExtraData = aExtraData;
    }

    LOGV(
        "EncodedVideoChunkData %p converted to %zu-byte MediaRawData - time: "
        "%" PRIi64 "us, timecode: %" PRIi64 "us, duration: %" PRIi64
        "us, key-frame: %s, has extra data: %s",
        mData.get(), sample->Size(), sample->mTime.ToMicroseconds(),
        sample->mTimecode.ToMicroseconds(), sample->mDuration.ToMicroseconds(),
        sample->mKeyframe ? "yes" : "no", sample->mExtraData ? "yes" : "no");

    return sample.forget();
  }
  const Id mId;  // A unique id shown in log.

 private:
  UniquePtr<EncodedVideoChunkData> mData;
};

class FlushMessage final
    : public ControlMessage,
      public MessageRequestHolder<DecoderAgent::DecodePromise> {
 public:
  using Id = size_t;
  FlushMessage(Id aId, ConfigureMessage::Id aConfigId, Promise* aPromise)
      : ControlMessage(
            nsPrintfCString("flush #%zu (config #%d)", aId, aConfigId)),
        mId(aId),
        mPromise(aPromise) {}
  ~FlushMessage() = default;
  virtual void Cancel() override { Disconnect(); }
  virtual bool IsProcessing() override { return Exists(); };
  virtual FlushMessage* AsFlushMessage() override { return this; }
  already_AddRefed<Promise> TakePromise() { return mPromise.forget(); }
  void RejectPromiseIfAny(const nsresult& aReason) {
    if (mPromise) {
      mPromise->MaybeReject(aReason);
    }
  }

  const Id mId;  // A unique id shown in log.

 private:
  RefPtr<Promise> mPromise;
};

/*
 * Below are VideoDecoder implementation
 */

VideoDecoder::VideoDecoder(nsIGlobalObject* aParent,
                           RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
                           RefPtr<VideoFrameOutputCallback>&& aOutputCallback)
    : DOMEventTargetHelper(aParent),
      mErrorCallback(std::move(aErrorCallback)),
      mOutputCallback(std::move(aOutputCallback)),
      mState(CodecState::Unconfigured),
      mKeyChunkRequired(true),
      mMessageQueueBlocked(false),
      mAgent(nullptr),
      mActiveConfig(nullptr),
      mDecodeQueueSize(0),
      mDequeueEventScheduled(false),
      mLatestConfigureId(ConfigureMessage::NoId),
      mDecodeCounter(0),
      mFlushCounter(0) {
  MOZ_ASSERT(mErrorCallback);
  MOZ_ASSERT(mOutputCallback);
  LOG("VideoDecoder %p ctor", this);
}

VideoDecoder::~VideoDecoder() {
  LOG("VideoDecoder %p dtor", this);
  Unused << Reset(NS_ERROR_DOM_ABORT_ERR);
}

JSObject* VideoDecoder::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnOwningThread();

  return VideoDecoder_Binding::Wrap(aCx, this, aGivenProto);
}

// https://w3c.github.io/webcodecs/#dom-videodecoder-videodecoder
/* static */
already_AddRefed<VideoDecoder> VideoDecoder::Constructor(
    const GlobalObject& aGlobal, const VideoDecoderInit& aInit,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return MakeAndAddRef<VideoDecoder>(
      global.get(), RefPtr<WebCodecsErrorCallback>(aInit.mError),
      RefPtr<VideoFrameOutputCallback>(aInit.mOutput));
}

// https://w3c.github.io/webcodecs/#dom-videodecoder-state
CodecState VideoDecoder::State() const { return mState; }

uint32_t VideoDecoder::DecodeQueueSize() const { return mDecodeQueueSize; }

// https://w3c.github.io/webcodecs/#dom-videodecoder-configure
void VideoDecoder::Configure(const VideoDecoderConfig& aConfig,
                             ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("VideoDecoder %p, Configure: codec %s", this,
      NS_ConvertUTF16toUTF8(aConfig.mCodec).get());

  if (auto r = Validate(aConfig); r.isErr()) {
    nsCString e = r.unwrapErr();
    LOGE("config is invalid: %s", e.get());
    aRv.ThrowTypeError(e);
    return;
  }

  if (mState == CodecState::Closed) {
    aRv.ThrowInvalidStateError("The codec is no longer usable");
    return;
  }

  // Clone a VideoDecoderConfig as the active decoder config.
  UniquePtr<VideoDecoderConfigInternal> config =
      VideoDecoderConfigInternal::Create(aConfig);
  if (!config) {
    aRv.Throw(NS_ERROR_UNEXPECTED);  // Invalid description data.
    return;
  }

  mState = CodecState::Configured;
  mKeyChunkRequired = true;
  mDecodeCounter = 0;
  mFlushCounter = 0;

  mControlMessageQueue.emplace(
      UniquePtr<ControlMessage>(ConfigureMessage::Create(std::move(config))));
  mLatestConfigureId = mControlMessageQueue.back()->AsConfigureMessage()->mId;
  LOG("VideoDecoder %p enqueues %s", this,
      mControlMessageQueue.back()->ToString().get());
  ProcessControlMessageQueue();
}

// https://w3c.github.io/webcodecs/#dom-videodecoder-decode
void VideoDecoder::Decode(EncodedVideoChunk& aChunk, ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("VideoDecoder %p, Decode", this);

  if (mState != CodecState::Configured) {
    aRv.ThrowInvalidStateError("Decoder must be configured first");
    return;
  }

  if (mKeyChunkRequired) {
    // TODO: Verify aChunk's data is truly a key chunk
    if (aChunk.Type() != EncodedVideoChunkType::Key) {
      aRv.ThrowDataError("VideoDecoder needs a key chunk");
      return;
    }
    mKeyChunkRequired = false;
  }

  mDecodeQueueSize += 1;
  mControlMessageQueue.emplace(UniquePtr<ControlMessage>(
      new DecodeMessage(++mDecodeCounter, mLatestConfigureId, aChunk.Clone())));
  LOGV("VideoDecoder %p enqueues %s", this,
       mControlMessageQueue.back()->ToString().get());
  ProcessControlMessageQueue();
}

// https://w3c.github.io/webcodecs/#dom-videodecoder-flush
already_AddRefed<Promise> VideoDecoder::Flush(ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("VideoDecoder %p, Flush", this);

  if (mState != CodecState::Configured) {
    LOG("VideoDecoder %p, wrong state!", this);
    aRv.ThrowInvalidStateError("Decoder must be configured first");
    return nullptr;
  }

  RefPtr<Promise> p = Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return p.forget();
  }

  mKeyChunkRequired = true;

  mControlMessageQueue.emplace(UniquePtr<ControlMessage>(
      new FlushMessage(++mFlushCounter, mLatestConfigureId, p)));
  LOG("VideoDecoder %p enqueues %s", this,
      mControlMessageQueue.back()->ToString().get());
  ProcessControlMessageQueue();
  return p.forget();
}

// https://w3c.github.io/webcodecs/#dom-videodecoder-reset
void VideoDecoder::Reset(ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("VideoDecoder %p, Reset", this);

  if (auto r = Reset(NS_ERROR_DOM_ABORT_ERR); r.isErr()) {
    aRv.Throw(r.unwrapErr());
  }
}

// https://w3c.github.io/webcodecs/#dom-videodecoder-close
void VideoDecoder::Close(ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("VideoDecoder %p, Close", this);

  if (auto r = Close(NS_ERROR_DOM_ABORT_ERR); r.isErr()) {
    aRv.Throw(r.unwrapErr());
  }
}

// https://w3c.github.io/webcodecs/#dom-videodecoder-isconfigsupported
/* static */
already_AddRefed<Promise> VideoDecoder::IsConfigSupported(
    const GlobalObject& aGlobal, const VideoDecoderConfig& aConfig,
    ErrorResult& aRv) {
  LOG("VideoDecoder::IsConfigSupported, config: %s",
      NS_ConvertUTF16toUTF8(aConfig.mCodec).get());

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> p = Promise::Create(global.get(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return p.forget();
  }

  if (auto r = Validate(aConfig); r.isErr()) {
    nsCString e = r.unwrapErr();
    LOGE("config is invalid: %s", e.get());
    p->MaybeRejectWithTypeError(e);
    return p.forget();
  }

  // TODO: Move the following works to another thread to unblock the current
  // thread, as what spec suggests.

  RootedDictionary<VideoDecoderConfig> config(aGlobal.Context());
  auto r = CloneConfiguration(config, aGlobal.Context(), aConfig);
  if (r.isErr()) {
    nsresult e = r.unwrapErr();
    LOGE("Failed to clone VideoDecoderConfig. Error: 0x%08" PRIx32,
         static_cast<uint32_t>(e));
    p->MaybeRejectWithTypeError("Failed to clone VideoDecoderConfig");
    aRv.Throw(e);
    return p.forget();
  }

  bool canDecode = CanDecode(config);
  RootedDictionary<VideoDecoderSupport> s(aGlobal.Context());
  s.mConfig.Construct(std::move(config));
  s.mSupported.Construct(canDecode);

  p->MaybeResolve(s);
  return p.forget();
}

// https://w3c.github.io/webcodecs/#reset-videodecoder
Result<Ok, nsresult> VideoDecoder::Reset(const nsresult& aResult) {
  AssertIsOnOwningThread();

  LOG("VideoDecoder %p reset with 0x%08" PRIx32, this,
      static_cast<uint32_t>(aResult));

  if (mState == CodecState::Closed) {
    return Err(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  mState = CodecState::Unconfigured;
  mDecodeCounter = 0;
  mFlushCounter = 0;

  CancelPendingControlMessages(aResult);
  DestroyDecoderAgentIfAny();

  if (mDecodeQueueSize > 0) {
    mDecodeQueueSize = 0;
    ScheduleDequeueEvent();
  }

  LOG("VideoDecoder %p now has its message queue unblocked", this);
  mMessageQueueBlocked = false;

  return Ok();
}

// https://w3c.github.io/webcodecs/#close-videodecoder
Result<Ok, nsresult> VideoDecoder::Close(const nsresult& aResult) {
  AssertIsOnOwningThread();

  LOG("VideoDecoder %p close with 0x%08" PRIx32, this,
      static_cast<uint32_t>(aResult));

  MOZ_TRY(Reset(aResult));
  mState = CodecState::Closed;
  if (aResult != NS_ERROR_DOM_ABORT_ERR) {
    LOGE("VideoDecoder %p Close on error: 0x%08" PRIx32, this,
         static_cast<uint32_t>(aResult));
    ScheduleReportError(aResult);
  }
  return Ok();
}

void VideoDecoder::ReportError(const nsresult& aResult) {
  AssertIsOnOwningThread();

  RefPtr<DOMException> e = DOMException::Create(aResult);
  RefPtr<WebCodecsErrorCallback> cb(mErrorCallback);
  cb->Call(*e);
}

void VideoDecoder::OutputVideoFrames(nsTArray<RefPtr<MediaData>>&& aData) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(mActiveConfig);

  nsTArray<RefPtr<VideoFrame>> frames = DecodedDataToVideoFrames(
      GetParentObject(), std::move(aData), *mActiveConfig);
  RefPtr<VideoFrameOutputCallback> cb(mOutputCallback);
  for (RefPtr<VideoFrame>& frame : frames) {
    RefPtr<VideoFrame> f = frame;
    cb->Call((VideoFrame&)(*f));
  }
}

class VideoDecoder::ErrorRunnable final : public DiscardableRunnable {
 public:
  ErrorRunnable(VideoDecoder* aVideoDecoder, const nsresult& aError)
      : DiscardableRunnable("VideoDecoder ErrorRunnable"),
        mVideoDecoder(aVideoDecoder),
        mError(aError) {
    MOZ_ASSERT(mVideoDecoder);
  }
  ~ErrorRunnable() = default;

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.
  // See bug 1535398.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
    LOGE("VideoDecoder %p report error: 0x%08" PRIx32, mVideoDecoder.get(),
         static_cast<uint32_t>(mError));
    RefPtr<VideoDecoder> d = std::move(mVideoDecoder);
    d->ReportError(mError);
    return NS_OK;
  }

 private:
  RefPtr<VideoDecoder> mVideoDecoder;
  const nsresult mError;
};

void VideoDecoder::ScheduleReportError(const nsresult& aResult) {
  LOGE("VideoDecoder %p, schedule to report error: 0x%08" PRIx32, this,
       static_cast<uint32_t>(aResult));
  MOZ_ALWAYS_SUCCEEDS(
      NS_DispatchToCurrentThread(MakeAndAddRef<ErrorRunnable>(this, aResult)));
}

class VideoDecoder::OutputRunnable final : public DiscardableRunnable {
 public:
  OutputRunnable(VideoDecoder* aVideoDecoder, DecoderAgent::Id aAgentId,
                 const nsACString& aLabel, nsTArray<RefPtr<MediaData>>&& aData)
      : DiscardableRunnable("VideoDecoder OutputRunnable"),
        mVideoDecoder(aVideoDecoder),
        mAgentId(aAgentId),
        mLabel(aLabel),
        mData(std::move(aData)) {
    MOZ_ASSERT(mVideoDecoder);
  }
  ~OutputRunnable() = default;

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.
  // See bug 1535398.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
    if (mVideoDecoder->State() != CodecState::Configured) {
      LOGV(
          "VideoDecoder %p has been %s. Discard %s-result for DecoderAgent #%d",
          mVideoDecoder.get(),
          mVideoDecoder->State() == CodecState::Closed ? "closed" : "reset",
          mLabel.get(), mAgentId);
      return NS_OK;
    }

    MOZ_ASSERT(mVideoDecoder->mAgent);
    if (mAgentId != mVideoDecoder->mAgent->mId) {
      LOGW(
          "VideoDecoder %p has been re-configured. Still yield %s-result for "
          "DecoderAgent #%d",
          mVideoDecoder.get(), mLabel.get(), mAgentId);
    }

    LOGV("VideoDecoder %p, yields %s-result for DecoderAgent #%d",
         mVideoDecoder.get(), mLabel.get(), mAgentId);
    RefPtr<VideoDecoder> d = std::move(mVideoDecoder);
    d->OutputVideoFrames(std::move(mData));

    return NS_OK;
  }

 private:
  RefPtr<VideoDecoder> mVideoDecoder;
  const DecoderAgent::Id mAgentId;
  const nsCString mLabel;
  nsTArray<RefPtr<MediaData>> mData;
};

void VideoDecoder::ScheduleOutputVideoFrames(
    nsTArray<RefPtr<MediaData>>&& aData, const nsACString& aLabel) {
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(mAgent);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(MakeAndAddRef<OutputRunnable>(
      this, mAgent->mId, aLabel, std::move(aData))));
}

void VideoDecoder::ScheduleClose(const nsresult& aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);

  LOG("VideoDecoder %p has schedule a close with 0x%08" PRIx32, this,
      static_cast<uint32_t>(aResult));

  auto task = [self = RefPtr{this}, result = aResult] {
    if (self->mState == CodecState::Closed) {
      LOGW("VideoDecoder %p has been closed. Ignore close with 0x%08" PRIx32,
           self.get(), static_cast<uint32_t>(result));
      return;
    }
    DebugOnly<Result<Ok, nsresult>> r = self->Close(result);
    MOZ_ASSERT(r.value.isOk());
  };
  nsISerialEventTarget* target = GetCurrentSerialEventTarget();

  if (NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(target->Dispatch(
        NS_NewRunnableFunction("ScheduleClose Runnable (main)", task)));
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(target->Dispatch(NS_NewCancelableRunnableFunction(
      "ScheduleClose Runnable (worker)", task)));
}

void VideoDecoder::ScheduleDequeueEvent() {
  AssertIsOnOwningThread();

  if (mDequeueEventScheduled) {
    return;
  }
  mDequeueEventScheduled = true;

  auto dispatcher = [self = RefPtr{this}] {
    FireEvent(self.get(), nsGkAtoms::ondequeue, u"dequeue"_ns);
    self->mDequeueEventScheduled = false;
  };
  nsISerialEventTarget* target = GetCurrentSerialEventTarget();

  if (NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(target->Dispatch(NS_NewRunnableFunction(
        "ScheduleDequeueEvent Runnable (main)", dispatcher)));
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(target->Dispatch(NS_NewCancelableRunnableFunction(
      "ScheduleDequeueEvent Runnable (worker)", dispatcher)));
}

void VideoDecoder::SchedulePromiseResolveOrReject(
    already_AddRefed<Promise> aPromise, const nsresult& aResult) {
  AssertIsOnOwningThread();

  RefPtr<Promise> p = aPromise;
  auto resolver = [p, result = aResult] {
    if (NS_FAILED(result)) {
      p->MaybeReject(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
      return;
    }
    p->MaybeResolveWithUndefined();
  };
  nsISerialEventTarget* target = GetCurrentSerialEventTarget();

  if (NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(target->Dispatch(NS_NewRunnableFunction(
        "SchedulePromiseResolveOrReject Runnable (main)", resolver)));
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(target->Dispatch(NS_NewCancelableRunnableFunction(
      "SchedulePromiseResolveOrReject Runnable (worker)", resolver)));
}

void VideoDecoder::ProcessControlMessageQueue() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);

  while (!mMessageQueueBlocked && !mControlMessageQueue.empty()) {
    UniquePtr<ControlMessage>& msg = mControlMessageQueue.front();
    if (msg->AsConfigureMessage()) {
      if (ProcessConfigureMessage(msg) ==
          MessageProcessedResult::NotProcessed) {
        break;
      }
    } else if (msg->AsDecodeMessage()) {
      if (ProcessDecodeMessage(msg) == MessageProcessedResult::NotProcessed) {
        break;
      }
    } else {
      MOZ_ASSERT(msg->AsFlushMessage());
      if (ProcessFlushMessage(msg) == MessageProcessedResult::NotProcessed) {
        break;
      }
    }
  }
}

void VideoDecoder::CancelPendingControlMessages(const nsresult& aResult) {
  AssertIsOnOwningThread();

  // Cancel the message that is being processed.
  if (mProcessingMessage) {
    LOG("VideoDecoder %p cancels current %s", this,
        mProcessingMessage->ToString().get());
    mProcessingMessage->Cancel();

    if (FlushMessage* flush = mProcessingMessage->AsFlushMessage()) {
      flush->RejectPromiseIfAny(aResult);
    }

    mProcessingMessage.reset();
  }

  // Clear the message queue.
  while (!mControlMessageQueue.empty()) {
    LOG("VideoDecoder %p cancels pending %s", this,
        mControlMessageQueue.front()->ToString().get());

    MOZ_ASSERT(!mControlMessageQueue.front()->IsProcessing());
    if (FlushMessage* flush = mControlMessageQueue.front()->AsFlushMessage()) {
      flush->RejectPromiseIfAny(aResult);
    }

    mControlMessageQueue.pop();
  }
}

VideoDecoder::MessageProcessedResult VideoDecoder::ProcessConfigureMessage(
    UniquePtr<ControlMessage>& aMessage) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(aMessage->AsConfigureMessage());

  if (mProcessingMessage) {
    LOG("VideoDecoder %p is processing %s. Defer %s", this,
        mProcessingMessage->ToString().get(), aMessage->ToString().get());
    return MessageProcessedResult::NotProcessed;
  }

  mProcessingMessage.reset(aMessage.release());
  mControlMessageQueue.pop();

  ConfigureMessage* msg = mProcessingMessage->AsConfigureMessage();
  LOG("VideoDecoder %p starts processing %s", this, msg->ToString().get());

  DestroyDecoderAgentIfAny();

  auto i = CreateVideoInfo(msg->Config());
  if (!CanDecode(msg->Config()) || i.isErr() ||
      !CreateDecoderAgent(msg->mId, msg->TakeConfig(), i.unwrap())) {
    mProcessingMessage.reset();
    DebugOnly<Result<Ok, nsresult>> r =
        Close(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR);
    MOZ_ASSERT(r.value.isOk());
    LOGE("VideoDecoder %p cannot be configured", this);
    return MessageProcessedResult::Processed;
  }

  MOZ_ASSERT(mAgent);
  MOZ_ASSERT(mActiveConfig);

  LOG("VideoDecoder %p now blocks message-queue-processing", this);
  mMessageQueueBlocked = true;

  bool preferSW = mActiveConfig->mHardwareAcceleration ==
                  HardwareAcceleration::Prefer_software;
  bool lowLatency = mActiveConfig->mOptimizeForLatency.isSome() &&
                    mActiveConfig->mOptimizeForLatency.value();
  mAgent->Configure(preferSW, lowLatency)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr{this}, id = mAgent->mId](
                 const DecoderAgent::ConfigurePromise::ResolveOrRejectValue&
                     aResult) {
               MOZ_ASSERT(self->mProcessingMessage);
               MOZ_ASSERT(self->mProcessingMessage->AsConfigureMessage());
               MOZ_ASSERT(self->mState == CodecState::Configured);
               MOZ_ASSERT(self->mAgent);
               MOZ_ASSERT(id == self->mAgent->mId);
               MOZ_ASSERT(self->mActiveConfig);

               ConfigureMessage* msg =
                   self->mProcessingMessage->AsConfigureMessage();
               LOG("VideoDecoder %p, DecodeAgent #%d %s has been %s. now "
                   "unblocks message-queue-processing",
                   self.get(), id, msg->ToString().get(),
                   aResult.IsResolve() ? "resolved" : "rejected");

               msg->Complete();
               self->mProcessingMessage.reset();

               if (aResult.IsReject()) {
                 // The spec asks to close VideoDecoder with an
                 // NotSupportedError so we log the exact error here.
                 const MediaResult& error = aResult.RejectValue();
                 LOGE(
                     "VideoDecoder %p, DecodeAgent #%d failed to configure: %s",
                     self.get(), id, error.Description().get());
                 DebugOnly<Result<Ok, nsresult>> r =
                     self->Close(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR);
                 MOZ_ASSERT(r.value.isOk());
                 return;  // No further process
               }

               self->mMessageQueueBlocked = false;
               self->ProcessControlMessageQueue();
             })
      ->Track(msg->Request());

  return MessageProcessedResult::Processed;
}

VideoDecoder::MessageProcessedResult VideoDecoder::ProcessDecodeMessage(
    UniquePtr<ControlMessage>& aMessage) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(aMessage->AsDecodeMessage());

  if (mProcessingMessage) {
    LOGV("VideoDecoder %p is processing %s. Defer %s", this,
         mProcessingMessage->ToString().get(), aMessage->ToString().get());
    return MessageProcessedResult::NotProcessed;
  }

  mProcessingMessage.reset(aMessage.release());
  mControlMessageQueue.pop();

  DecodeMessage* msg = mProcessingMessage->AsDecodeMessage();
  LOGV("VideoDecoder %p starts processing %s", this, msg->ToString().get());

  mDecodeQueueSize -= 1;
  ScheduleDequeueEvent();

  // Treat it like decode error if no DecoderAgent is available or the encoded
  // data is invalid.
  auto closeOnError = [&]() {
    mProcessingMessage.reset();
    ScheduleClose(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
    return MessageProcessedResult::Processed;
  };

  if (!mAgent) {
    LOGE("VideoDecoder %p is not configured", this);
    return closeOnError();
  }

  MOZ_ASSERT(mActiveConfig);
  RefPtr<MediaRawData> data = msg->TakeData(
      mAgent->mInfo->GetAsVideoInfo()->mExtraData, *mActiveConfig);
  if (!data) {
    LOGE("VideoDecoder %p, data for %s is empty or invalid", this,
         msg->ToString().get());
    return closeOnError();
  }

  mAgent->Decode(data.get())
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr{this}, id = mAgent->mId](
                 DecoderAgent::DecodePromise::ResolveOrRejectValue&& aResult) {
               MOZ_ASSERT(self->mProcessingMessage);
               MOZ_ASSERT(self->mProcessingMessage->AsDecodeMessage());
               MOZ_ASSERT(self->mState == CodecState::Configured);
               MOZ_ASSERT(self->mAgent);
               MOZ_ASSERT(id == self->mAgent->mId);
               MOZ_ASSERT(self->mActiveConfig);

               DecodeMessage* msg = self->mProcessingMessage->AsDecodeMessage();
               LOGV("VideoDecoder %p, DecodeAgent #%d %s has been %s",
                    self.get(), id, msg->ToString().get(),
                    aResult.IsResolve() ? "resolved" : "rejected");

               nsCString msgStr = msg->ToString();

               msg->Complete();
               self->mProcessingMessage.reset();

               if (aResult.IsReject()) {
                 // The spec asks to queue a task to run close-VideoDecoder
                 // with an EncodingError so we log the exact error here.
                 const MediaResult& error = aResult.RejectValue();
                 LOGE("VideoDecoder %p, DecodeAgent #%d %s failed: %s",
                      self.get(), id, msgStr.get(), error.Description().get());
                 self->ScheduleClose(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
                 return;  // No further process
               }

               MOZ_ASSERT(aResult.IsResolve());
               nsTArray<RefPtr<MediaData>> data =
                   std::move(aResult.ResolveValue());
               if (data.IsEmpty()) {
                 LOGV("VideoDecoder %p got no data for %s", self.get(),
                      msgStr.get());
               } else {
                 LOGV(
                     "VideoDecoder %p, schedule %zu decoded data output for "
                     "%s",
                     self.get(), data.Length(), msgStr.get());
                 self->ScheduleOutputVideoFrames(std::move(data), msgStr);
               }

               self->ProcessControlMessageQueue();
             })
      ->Track(msg->Request());

  return MessageProcessedResult::Processed;
}

VideoDecoder::MessageProcessedResult VideoDecoder::ProcessFlushMessage(
    UniquePtr<ControlMessage>& aMessage) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(aMessage->AsFlushMessage());

  if (mProcessingMessage) {
    LOG("VideoDecoder %p is processing %s. Defer %s", this,
        mProcessingMessage->ToString().get(), aMessage->ToString().get());
    return MessageProcessedResult::NotProcessed;
  }

  mProcessingMessage.reset(aMessage.release());
  mControlMessageQueue.pop();

  FlushMessage* msg = mProcessingMessage->AsFlushMessage();
  LOG("VideoDecoder %p starts processing %s", this, msg->ToString().get());

  // Treat it like decode error if no DecoderAgent is available.
  if (!mAgent) {
    LOGE("VideoDecoder %p is not configured", this);

    SchedulePromiseResolveOrReject(msg->TakePromise(),
                                   NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
    mProcessingMessage.reset();

    ScheduleClose(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);

    return MessageProcessedResult::Processed;
  }

  mAgent->DrainAndFlush()
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr{this}, id = mAgent->mId](
                 DecoderAgent::DecodePromise::ResolveOrRejectValue&& aResult) {
               MOZ_ASSERT(self->mProcessingMessage);
               MOZ_ASSERT(self->mProcessingMessage->AsFlushMessage());
               MOZ_ASSERT(self->mState == CodecState::Configured);
               MOZ_ASSERT(self->mAgent);
               MOZ_ASSERT(id == self->mAgent->mId);
               MOZ_ASSERT(self->mActiveConfig);

               FlushMessage* msg = self->mProcessingMessage->AsFlushMessage();
               LOG("VideoDecoder %p, DecodeAgent #%d %s has been %s",
                   self.get(), id, msg->ToString().get(),
                   aResult.IsResolve() ? "resolved" : "rejected");

               nsCString msgStr = msg->ToString();

               msg->Complete();

               // If flush failed, it means decoder fails to decode the data
               // sent before, so we treat it like decode error. We reject the
               // promise first and then queue a task to close VideoDecoder with
               // an EncodingError.
               if (aResult.IsReject()) {
                 const MediaResult& error = aResult.RejectValue();
                 LOGE("VideoDecoder %p, DecodeAgent #%d failed to flush: %s",
                      self.get(), id, error.Description().get());

                 // Reject with an EncodingError instead of the error we got
                 // above.
                 self->SchedulePromiseResolveOrReject(
                     msg->TakePromise(),
                     NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);

                 self->mProcessingMessage.reset();

                 self->ScheduleClose(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
                 return;  // No further process
               }

               // If flush succeeded, schedule to output decoded data first
               // and then resolve the promise, then keep processing the
               // control messages.
               MOZ_ASSERT(aResult.IsResolve());
               nsTArray<RefPtr<MediaData>> data =
                   std::move(aResult.ResolveValue());

               if (data.IsEmpty()) {
                 LOG("VideoDecoder %p gets no data for %s", self.get(),
                     msgStr.get());
               } else {
                 LOG("VideoDecoder %p, schedule %zu decoded data output for "
                     "%s",
                     self.get(), data.Length(), msgStr.get());
                 self->ScheduleOutputVideoFrames(std::move(data), msgStr);
               }

               self->SchedulePromiseResolveOrReject(msg->TakePromise(), NS_OK);
               self->mProcessingMessage.reset();
               self->ProcessControlMessageQueue();
             })
      ->Track(msg->Request());

  return MessageProcessedResult::Processed;
}

// CreateDecoderAgent will create an DecoderAgent paired with a xpcom-shutdown
// blocker and a worker-reference. Besides the needs mentioned in the header
// file, the blocker and the worker-reference also provides an entry point for
// us to clean up the resources. Other than ~VideoDecoder, Reset(), or
// Close(), the resources should be cleaned up in the following situations:
// 1. VideoDecoder on window, closing document
// 2. VideoDecoder on worker, closing document
// 3. VideoDecoder on worker, terminating worker
//
// In case 1, the entry point to clean up is in the mShutdownBlocker's
// ShutdownpPomise-resolver. In case 2, the entry point is in mWorkerRef's
// shutting down callback. In case 3, the entry point is in mWorkerRef's
// shutting down callback.
bool VideoDecoder::CreateDecoderAgent(
    DecoderAgent::Id aId, UniquePtr<VideoDecoderConfigInternal>&& aConfig,
    UniquePtr<TrackInfo>&& aInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(!mAgent);
  MOZ_ASSERT(!mActiveConfig);
  MOZ_ASSERT(!mShutdownBlocker);
  MOZ_ASSERT_IF(!NS_IsMainThread(), !mWorkerRef);

  auto resetOnFailure = MakeScopeExit([&]() {
    mAgent = nullptr;
    mActiveConfig = nullptr;
    mShutdownBlocker = nullptr;
    mWorkerRef = nullptr;
  });

  // If VideoDecoder is on worker, get a worker reference.
  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    if (NS_WARN_IF(!workerPrivate)) {
      return false;
    }

    // Clean up all the resources when worker is going away.
    RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
        workerPrivate, "VideoDecoder::SetupDecoder", [self = RefPtr{this}]() {
          LOG("VideoDecoder %p, worker is going away", self.get());
          Unused << self->Reset(NS_ERROR_DOM_ABORT_ERR);
        });
    if (NS_WARN_IF(!workerRef)) {
      return false;
    }

    mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  }

  mAgent = MakeRefPtr<DecoderAgent>(aId, std::move(aInfo));
  mActiveConfig = std::move(aConfig);

  // ShutdownBlockingTicket requires an unique name to register its own
  // nsIAsyncShutdownBlocker since each blocker needs a distinct name.
  // To do that, we use DecodeAgent's unique id to create a unique name.
  nsAutoString uniqueName;
  uniqueName.AppendPrintf(
      "Blocker for DecodeAgent #%d (codec: %s) @ %p", mAgent->mId,
      NS_ConvertUTF16toUTF8(mActiveConfig->mCodec).get(), mAgent.get());

  mShutdownBlocker = media::ShutdownBlockingTicket::Create(
      uniqueName, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__);
  if (!mShutdownBlocker) {
    LOGE("VideoDecoder %p failed to create %s", this,
         NS_ConvertUTF16toUTF8(uniqueName).get());
    return false;
  }

  // Clean up all the resources when xpcom-will-shutdown arrives since the page
  // is going to be closed.
  mShutdownBlocker->ShutdownPromise()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}, id = mAgent->mId,
       ref = mWorkerRef](bool /* aUnUsed*/) {
        LOG("VideoDecoder %p gets xpcom-will-shutdown notification for "
            "DecodeAgent #%d",
            self.get(), id);
        Unused << self->Reset(NS_ERROR_DOM_ABORT_ERR);
      },
      [self = RefPtr{this}, id = mAgent->mId,
       ref = mWorkerRef](bool /* aUnUsed*/) {
        LOG("VideoDecoder %p removes shutdown-blocker #%d before getting any "
            "notification. DecodeAgent #%d should have been dropped",
            self.get(), id, id);
        MOZ_ASSERT(!self->mAgent || self->mAgent->mId != id);
      });

  LOG("VideoDecoder %p creates DecodeAgent #%d @ %p and its shutdown-blocker",
      this, mAgent->mId, mAgent.get());

  resetOnFailure.release();
  return true;
}

void VideoDecoder::DestroyDecoderAgentIfAny() {
  AssertIsOnOwningThread();

  if (!mAgent) {
    LOG("VideoDecoder %p has no DecodeAgent to destroy", this);
    return;
  }

  MOZ_ASSERT(mActiveConfig);
  MOZ_ASSERT(mShutdownBlocker);
  MOZ_ASSERT_IF(!NS_IsMainThread(), mWorkerRef);

  LOG("VideoDecoder %p destroys DecodeAgent #%d @ %p", this, mAgent->mId,
      mAgent.get());
  mActiveConfig = nullptr;
  RefPtr<DecoderAgent> agent = std::move(mAgent);
  // mShutdownBlocker should be kept alive until the shutdown is done.
  // mWorkerRef is used to ensure this task won't be discarded in worker.
  agent->Shutdown()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}, id = agent->mId, ref = std::move(mWorkerRef),
       blocker = std::move(mShutdownBlocker)](
          const ShutdownPromise::ResolveOrRejectValue& aResult) {
        LOG("VideoDecoder %p, DecoderAgent #%d's shutdown has been %s. "
            "Drop its shutdown-blocker now",
            self.get(), id, aResult.IsResolve() ? "resolved" : "rejected");
      });
}

#undef LOG
#undef LOGW
#undef LOGE
#undef LOGV
#undef LOG_INTERNAL

}  // namespace mozilla::dom
