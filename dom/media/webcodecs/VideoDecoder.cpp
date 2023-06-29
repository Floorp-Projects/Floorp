/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VideoDecoder.h"
#include "mozilla/dom/VideoDecoderBinding.h"

#include "DecoderTraits.h"
#include "H264.h"
#include "ImageContainer.h"
#include "MediaContainerType.h"
#include "MediaData.h"
#include "TimeUnits.h"
#include "VideoUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/EncodedVideoChunk.h"
#include "mozilla/dom/EncodedVideoChunkBinding.h"
#include "mozilla/dom/Event.h"
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

mozilla::LazyLogModule gWebCodecsLog("WebCodecs");
using mozilla::media::TimeUnit;

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

// https://w3c.github.io/webcodecs/#valid-videodecoderconfig
static bool IsValid(const VideoDecoderConfig& aConfig) {
  nsTArray<nsString> codecs;
  if (!ParseCodecsString(aConfig.mCodec, codecs) || codecs.Length() != 1 ||
      codecs[0] != aConfig.mCodec) {
    return false;
  }

  // WebCodecs doesn't support theora
  if (!IsAV1CodecString(codecs[0]) && !IsVP9CodecString(codecs[0]) &&
      !IsVP8CodecString(codecs[0]) && !IsH264CodecString(codecs[0])) {
    return false;
  }

  // Gecko allows codec string starts with vp9 or av1 but Webcodecs requires to
  // starts with av01 and vp09.
  // https://www.w3.org/TR/webcodecs-codec-registry/#video-codec-registry
  if (StringBeginsWith(aConfig.mCodec, u"vp9"_ns) ||
      StringBeginsWith(aConfig.mCodec, u"av1"_ns)) {
    return false;
  }

  if (aConfig.mCodedWidth.WasPassed() != aConfig.mCodedHeight.WasPassed()) {
    return false;
  }
  if (aConfig.mCodedWidth.WasPassed() &&
      (aConfig.mCodedWidth.Value() == 0 || aConfig.mCodedHeight.Value() == 0)) {
    return false;
  }

  if (aConfig.mDisplayAspectWidth.WasPassed() !=
      aConfig.mDisplayAspectHeight.WasPassed()) {
    return false;
  }
  if (aConfig.mDisplayAspectWidth.WasPassed() &&
      (aConfig.mDisplayAspectWidth.Value() == 0 ||
       aConfig.mDisplayAspectHeight.Value() == 0)) {
    return false;
  }

  return true;
}

static nsTArray<nsCString> GuessMIMETypes(const VideoDecoderConfig& aConfig) {
  const auto codec = NS_ConvertUTF16toUTF8(aConfig.mCodec);
  nsTArray<nsCString> types;
  for (const nsCString& container : GuessContainers(aConfig.mCodec)) {
    nsPrintfCString mime("video/%s; codecs=%s", container.get(), codec.get());
    if (aConfig.mCodedWidth.WasPassed()) {
      mime.Append(nsPrintfCString("; width=%d", aConfig.mCodedWidth.Value()));
    }
    if (aConfig.mCodedHeight.WasPassed()) {
      mime.Append(nsPrintfCString("; height=%d", aConfig.mCodedHeight.Value()));
    }
    types.AppendElement(mime);
  }
  return types;
}

// https://w3c.github.io/webcodecs/#check-configuration-support
static bool CanDecode(const VideoDecoderConfig& aConfig) {
  // TODO: Instead of calling CanHandleContainerType with the guessed the
  // containers, DecoderTraits should provide an API to tell if a codec is
  // decodable or not.
  for (const nsCString& mime : GuessMIMETypes(aConfig)) {
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

static nsTArray<UniquePtr<TrackInfo>> GetTracksInfo(
    const VideoDecoderConfig& aConfig) {
  // TODO: Instead of calling GetTracksInfo with the guessed containers,
  // DecoderTraits should provide an API to create the TrackInfo directly.
  for (const nsCString& mime : GuessMIMETypes(aConfig)) {
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
  RefPtr<MediaByteBuffer> data = nullptr;
  Span<uint8_t> buf;
  MOZ_TRY_VAR(buf, GetSharedArrayBufferData(aBuffer));
  if (buf.empty()) {
    return data;
  }
  data = MakeRefPtr<MediaByteBuffer>();
  data->AppendElements(buf);
  return data;
}

static Result<UniquePtr<TrackInfo>, nsresult> CreateVideoInfo(
    const VideoDecoderConfig& aConfig) {
  MOZ_ASSERT(IsValid(aConfig));

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
  if (aConfig.mCodedHeight.WasPassed()) {
    if (aConfig.mCodedHeight.Value() > MAX) {
      LOGE("codedHeight overflows");
      return Err(NS_ERROR_INVALID_ARG);
    }
    vi->mImage.height = static_cast<decltype(gfx::IntSize::height)>(
        aConfig.mCodedHeight.Value());
  }
  if (aConfig.mCodedWidth.WasPassed()) {
    if (aConfig.mCodedWidth.Value() > MAX) {
      LOGE("codedWidth overflows");
      return Err(NS_ERROR_INVALID_ARG);
    }
    vi->mImage.width =
        static_cast<decltype(gfx::IntSize::width)>(aConfig.mCodedWidth.Value());
  }

  if (aConfig.mDisplayAspectHeight.WasPassed()) {
    if (aConfig.mDisplayAspectHeight.Value() > MAX) {
      LOGE("displayAspectHeight overflows");
      return Err(NS_ERROR_INVALID_ARG);
    }
    vi->mDisplay.height = static_cast<decltype(gfx::IntSize::height)>(
        aConfig.mDisplayAspectHeight.Value());
  }
  if (aConfig.mDisplayAspectWidth.WasPassed()) {
    if (aConfig.mDisplayAspectWidth.Value() > MAX) {
      LOGE("displayAspectWidth overflows");
      return Err(NS_ERROR_INVALID_ARG);
    }
    vi->mDisplay.width = static_cast<decltype(gfx::IntSize::width)>(
        aConfig.mDisplayAspectWidth.Value());
  }

  if (aConfig.mColorSpace.WasPassed()) {
    const VideoColorSpaceInit& colorSpace = aConfig.mColorSpace.Value();
    if (!colorSpace.mFullRange.IsNull()) {
      vi->mColorRange = ToColorRange(colorSpace.mFullRange.Value());
    }
    if (!colorSpace.mMatrix.IsNull()) {
      vi->mColorSpace.emplace(ToColorSpace(colorSpace.mMatrix.Value()));
    }
    if (!colorSpace.mPrimaries.IsNull()) {
      vi->mColorPrimaries.emplace(ToPrimaries(colorSpace.mPrimaries.Value()));
    }
    if (!colorSpace.mTransfer.IsNull()) {
      vi->mTransferFunction.emplace(
          ToTransferFunction(colorSpace.mTransfer.Value()));
    }
  }

  if (aConfig.mDescription.WasPassed()) {
    RefPtr<MediaByteBuffer> buf;
    MOZ_TRY_VAR(buf, GetExtraData(aConfig.mDescription.Value()));
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
    VideoDecoderConfig& aDest, JSContext* aCx,
    const VideoDecoderConfig& aConfig) {
  MOZ_ASSERT(IsValid(aConfig));

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
    auto r = CloneBuffer(aCx, aConfig.mDescription.Value());
    if (r.isErr()) {
      return Err(r.unwrapErr());
    }
    aDest.mDescription.Construct(r.unwrap());
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

// https://w3c.github.io/webcodecs/#clone-configuration
static Result<VideoDecoderConfig, nsresult> CloneConfiguration(
    const GlobalObject& aGlobal, const VideoDecoderConfig& aConfig) {
  VideoDecoderConfig c;
  auto r = CloneConfiguration(c, aGlobal.Context(), aConfig);
  if (r.isErr()) {
    return Err(r.unwrapErr());
  }
  return c;
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
  // TODO: Implement this.
  return Nothing();
}

static VideoColorSpaceInit GuessColorSpace(layers::Image* aImage) {
  // TODO: Implement this.
  return {};
}

// https://w3c.github.io/webcodecs/#create-a-videoframe
static RefPtr<VideoFrame> CreateVideoFrame(
    nsIGlobalObject* aGlobalObject, const VideoData* aData, int64_t aTimestamp,
    uint64_t aDuration, const uint32_t* mDisplayAspectWidth,
    const uint32_t* mDisplayAspectHeight,
    const VideoColorSpaceInit& aColorSpace) {
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT(aData);
  MOZ_ASSERT((!!mDisplayAspectWidth) == (!!mDisplayAspectHeight));

  Maybe<VideoPixelFormat> format = GuessPixelFormat(aData->mImage.get());
  gfx::IntSize displaySize = aData->mDisplay;
  if (mDisplayAspectWidth && mDisplayAspectHeight) {
    // TODO: Calculate displaySize
  }

  // TODO: Allow constructing VideoFrame without pixel format
  return MakeRefPtr<VideoFrame>(
      aGlobalObject, aData->mImage,
      format ? format.ref() : VideoPixelFormat::EndGuard_,
      aData->mImage->GetSize(), aData->mImage->GetPictureRect(), displaySize,
      Some(aDuration), aTimestamp, aColorSpace);
}

static nsTArray<RefPtr<VideoFrame>> DecodedDataToVideoFrames(
    nsIGlobalObject* aGlobalObject, nsTArray<RefPtr<MediaData>>&& aData,
    VideoDecoderConfig& aConfig) {
  nsTArray<RefPtr<VideoFrame>> frames;
  for (RefPtr<MediaData>& data : aData) {
    MOZ_RELEASE_ASSERT(data->mType == MediaData::Type::VIDEO_DATA);
    RefPtr<const VideoData> d(data->As<const VideoData>());
    VideoColorSpaceInit colorSpace = GuessColorSpace(d->mImage.get());
    frames.AppendElement(CreateVideoFrame(
        aGlobalObject, d.get(), d->mTime.ToMicroseconds(),
        static_cast<uint64_t>(d->mDuration.ToMicroseconds()),
        aConfig.mDisplayAspectWidth.WasPassed()
            ? &aConfig.mDisplayAspectWidth.Value()
            : nullptr,
        aConfig.mDisplayAspectHeight.WasPassed()
            ? &aConfig.mDisplayAspectHeight.Value()
            : nullptr,
        aConfig.mColorSpace.WasPassed() ? aConfig.mColorSpace.Value()
                                        : colorSpace));
  }
  return frames;
}

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
      mDecodeMessageCounter(0) {
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

  if (!IsValid(aConfig)) {
    aRv.ThrowTypeError("Invalid VideoDecoderConfig");
    return;
  }

  if (mState == CodecState::Closed) {
    aRv.ThrowInvalidStateError("The codec is no longer usable");
    return;
  }

  // Clone a VideoDecoderConfig as the active decoder config.
  AutoJSAPI jsapi;
  if (!jsapi.Init(GetParentObject())) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  UniquePtr<VideoDecoderConfig> config(new VideoDecoderConfig());
  auto c = CloneConfiguration(*config, jsapi.cx(), aConfig);
  if (c.isErr()) {
    aRv.Throw(c.unwrapErr());
    return;
  }
  MOZ_ASSERT(IsValid(*config));

  mState = CodecState::Configured;
  mKeyChunkRequired = true;
  mDecodeMessageCounter = 0;

  mControlMessageQueue.emplace(AsVariant(ConfigureMessage(std::move(config))));
  LOG("VideoDecoder %p enqueues %s", this,
      mControlMessageQueue.back().as<ConfigureMessage>().mTitle.get());
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
  mControlMessageQueue.emplace(AsVariant(DecodeMessage(
      ++mDecodeMessageCounter, MakeUnique<DecodeMessage::ChunkData>(aChunk))));
  LOGV("VideoDecoder %p enqueues %s", this,
       mControlMessageQueue.back().as<DecodeMessage>().mTitle.get());
  ProcessControlMessageQueue();
}

already_AddRefed<Promise> VideoDecoder::Flush(ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
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

  if (!IsValid(aConfig)) {
    p->MaybeRejectWithTypeError("Invalid VideoDecoderConfig");
    return p.forget();
  }

  // TODO: Move the following works to another thread to unblock the current
  // thread, as what spec suggests.

  auto r = CloneConfiguration(aGlobal, aConfig);
  if (r.isErr()) {
    nsresult e = r.unwrapErr();
    LOGE("Failed to clone VideoDecoderConfig. Error: 0x%08" PRIx32,
         static_cast<uint32_t>(e));
    p->MaybeRejectWithTypeError("Failed to clone VideoDecoderConfig");
    aRv.Throw(e);
    return p.forget();
  }
  VideoDecoderSupport s;
  s.mConfig.Construct(r.unwrap());
  s.mSupported.Construct(CanDecode(aConfig));

  p->MaybeResolve(s);
  return p.forget();
}

// https://w3c.github.io/webcodecs/#reset-videodecoder
Result<Ok, nsresult> VideoDecoder::Reset(const nsresult& aResult) {
  AssertIsOnOwningThread();

  if (mState == CodecState::Closed) {
    return Err(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  mState = CodecState::Unconfigured;
  mDecodeMessageCounter = 0;

  CancelPendingControlMessages();
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

  MOZ_TRY(Reset(aResult));
  mState = CodecState::Closed;
  if (aResult != NS_ERROR_DOM_ABORT_ERR) {
    LOGE("VideoDecoder %p Close on error: 0x%08" PRIx32, this,
         static_cast<uint32_t>(aResult));
    // TODO: Schedule the error callback instead of calling it directly.
    ReportError(aResult);
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

  auto task = [self = RefPtr{this}, result = aResult] MOZ_CAN_RUN_SCRIPT {
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

void VideoDecoder::ProcessControlMessageQueue() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);

  while (!mMessageQueueBlocked && !mControlMessageQueue.empty()) {
    auto& msg = mControlMessageQueue.front();
    if (msg.is<ConfigureMessage>()) {
      if (ProcessConfigureMessage(msg) ==
          MessageProcessedResult::NotProcessed) {
        break;
      }
    } else if (msg.is<DecodeMessage>()) {
      if (ProcessDecodeMessage(msg) == MessageProcessedResult::NotProcessed) {
        break;
      }
    } else {
      MOZ_ASSERT_UNREACHABLE("Unknown message type!");
    }
  }
}

void VideoDecoder::CancelPendingControlMessages() {
  AssertIsOnOwningThread();

  // Cancel the message that is being processed.
  if (mProcessingMessage) {
    if (mProcessingMessage->is<ConfigureMessage>()) {
      auto& msg = mProcessingMessage->as<ConfigureMessage>();
      LOG("VideoDecoder %p cancels current %s", this, msg.mTitle.get());
      MOZ_ASSERT(msg.mRequest.Exists());
      msg.mRequest.Disconnect();
    } else if (mProcessingMessage->is<DecodeMessage>()) {
      auto& msg = mProcessingMessage->as<DecodeMessage>();
      LOG("VideoDecoder %p cancels current %s", this, msg.mTitle.get());
      MOZ_ASSERT(msg.mRequest.Exists());
      msg.mRequest.Disconnect();
    } else {
      MOZ_ASSERT_UNREACHABLE("Unknown message type!");
    }

    mProcessingMessage.reset();
  }

  // Clear the message queue.
  while (!mControlMessageQueue.empty()) {
    if (mControlMessageQueue.front().is<ConfigureMessage>()) {
      auto& msg = mControlMessageQueue.front().as<ConfigureMessage>();
      LOG("VideoDecoder %p cancels pending %s", this, msg.mTitle.get());
      MOZ_ASSERT(!msg.mRequest.Exists());
    } else if (mControlMessageQueue.front().is<DecodeMessage>()) {
      auto& msg = mControlMessageQueue.front().as<DecodeMessage>();
      LOG("VideoDecoder %p cancels pending %s", this, msg.mTitle.get());
      MOZ_ASSERT(!msg.mRequest.Exists());
    } else {
      MOZ_ASSERT_UNREACHABLE("Unknown message type!");
    }

    mControlMessageQueue.pop();
  }
}

ConfigureMessage::ConfigureMessage(UniquePtr<VideoDecoderConfig>&& aConfig)
    : mTitle(nsPrintfCString("%s-configuration",
                             NS_ConvertUTF16toUTF8(aConfig->mCodec).get())),
      mConfig(std::move(aConfig)) {}

VideoDecoder::MessageProcessedResult VideoDecoder::ProcessConfigureMessage(
    ControlMessage& aMessage) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(aMessage.is<ConfigureMessage>());

  if (mProcessingMessage) {
    const char* current =
        mProcessingMessage->is<ConfigureMessage>()
            ? mProcessingMessage->as<ConfigureMessage>().mTitle.get()
            : mProcessingMessage->as<DecodeMessage>().mTitle.get();
    const char* incoming = aMessage.as<ConfigureMessage>().mTitle.get();

    LOG("VideoDecoder %p is processing %s. Defer %s", this, current, incoming);
    return MessageProcessedResult::NotProcessed;
  }

  mProcessingMessage.emplace(std::move(aMessage));
  mControlMessageQueue.pop();

  auto& msg = mProcessingMessage->as<ConfigureMessage>();
  LOG("VideoDecoder %p starts processing %s", this, msg.mTitle.get());

  DestroyDecoderAgentIfAny();

  auto i = CreateVideoInfo(*(msg.mConfig));
  if (!CanDecode(*(msg.mConfig)) || i.isErr() ||
      !CreateDecoderAgent(std::move(msg.mConfig), i.unwrap())) {
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
  bool lowLatency = mActiveConfig->mOptimizeForLatency.WasPassed() &&
                    mActiveConfig->mOptimizeForLatency.Value();
  mAgent->Configure(preferSW, lowLatency)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr{this}, id = mAgent->mId](
                 const DecoderAgent::ConfigurePromise::ResolveOrRejectValue&
                     aResult) MOZ_CAN_RUN_SCRIPT {
               MOZ_ASSERT(self->mProcessingMessage);
               MOZ_ASSERT(self->mProcessingMessage->is<ConfigureMessage>());
               MOZ_ASSERT(self->mState == CodecState::Configured);
               MOZ_ASSERT(self->mAgent);
               MOZ_ASSERT(id == self->mAgent->mId);
               MOZ_ASSERT(self->mActiveConfig);

               auto& msg = self->mProcessingMessage->as<ConfigureMessage>();
               LOG("VideoDecoder %p, DecodeAgent #%d %s has been %s. now "
                   "unblocks message-queue-processing",
                   self.get(), id, msg.mTitle.get(),
                   aResult.IsResolve() ? "resolved" : "rejected");

               msg.mRequest.Complete();
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
      ->Track(msg.mRequest);

  return MessageProcessedResult::Processed;
}

struct DecodeMessage::ChunkData {
  explicit ChunkData(EncodedVideoChunk& aChunk)
      : mBuffer(aChunk.Data(), static_cast<size_t>(aChunk.ByteLength())),
        mIsKey(aChunk.Type() == EncodedVideoChunkType::Key),
        mTimestamp(aChunk.Timestamp()),
        mDuration(aChunk.GetDuration().IsNull()
                      ? Nothing()
                      : Some(aChunk.GetDuration().Value())) {
    LOGV("Create %zu-byte ChunkData from %u-byte EncodedVideoChunk",
         mBuffer ? mBuffer.Size() : 0, aChunk.ByteLength());
  }

  RefPtr<MediaRawData> IntoMediaRawData(
      const RefPtr<MediaByteBuffer>& aExtraData,
      const VideoDecoderConfig& aConfig) {
    if (!mBuffer) {
      LOGE("Chunk is empty!");
      return nullptr;
    }

    RefPtr<MediaRawData> sample(new MediaRawData(std::move(mBuffer)));
    sample->mKeyframe = mIsKey;
    sample->mTime = TimeUnit::FromMicroseconds(mTimestamp);
    sample->mTimecode = TimeUnit::FromMicroseconds(mTimestamp);

    if (mDuration) {
      CheckedInt64 duration(*mDuration);
      if (!duration.isValid()) {
        LOGE("Chunk's duration exceeds TimeUnit's limit");
        return nullptr;
      }
      sample->mDuration = TimeUnit::FromMicroseconds(duration.value());
    }

    // aExtraData is either provided by Configure() or a default one created for
    // the decoder creation. If it's created for decoder creation only, we don't
    // set it to sample.
    if (aConfig.mDescription.WasPassed() && aExtraData) {
      sample->mExtraData = aExtraData;
    }

    LOGV("Input chunk converted to %zu-byte MediaRawData - time: %" PRIi64
         "us, timecode: %" PRIi64 "us, duration: %" PRIi64
         "us, key-frame: %s, has extra data: %s",
         sample->Size(), sample->mTime.ToMicroseconds(),
         sample->mTimecode.ToMicroseconds(), sample->mDuration.ToMicroseconds(),
         sample->mKeyframe ? "yes" : "no", sample->mExtraData ? "yes" : "no");

    return sample.forget();
  }

  AlignedByteBuffer mBuffer;
  bool mIsKey;
  int64_t mTimestamp;
  Maybe<uint64_t> mDuration;
};

DecodeMessage::DecodeMessage(Id aId, UniquePtr<ChunkData>&& aData)
    : mTitle(nsPrintfCString("decode #%zu", aId)),
      mId(aId),
      mData(std::move(aData)) {}

VideoDecoder::MessageProcessedResult VideoDecoder::ProcessDecodeMessage(
    ControlMessage& aMessage) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(aMessage.is<DecodeMessage>());

  if (mProcessingMessage) {
    const char* current =
        mProcessingMessage->is<ConfigureMessage>()
            ? mProcessingMessage->as<ConfigureMessage>().mTitle.get()
            : mProcessingMessage->as<DecodeMessage>().mTitle.get();
    const char* incoming = aMessage.as<DecodeMessage>().mTitle.get();

    LOGV("VideoDecoder %p is processing %s. Defer %s", this, current, incoming);
    return MessageProcessedResult::NotProcessed;
  }

  mProcessingMessage.emplace(std::move(aMessage));
  mControlMessageQueue.pop();

  auto& msg = mProcessingMessage->as<DecodeMessage>();
  LOGV("VideoDecoder %p starts processing %s", this, msg.mTitle.get());

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

  if (!msg.mData) {
    LOGE("VideoDecoder %p, no data for %s", this, msg.mTitle.get());
    return closeOnError();
  }

  MOZ_ASSERT(mActiveConfig);
  RefPtr<MediaRawData> data = msg.mData->IntoMediaRawData(
      mAgent->mInfo->GetAsVideoInfo()->mExtraData, *mActiveConfig);
  if (!data) {
    LOGE("VideoDecoder %p, data for %s is invalid", this, msg.mTitle.get());
    return closeOnError();
  }

  mAgent->Decode(data.get())
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this}, id = mAgent->mId](
              DecoderAgent::DecodePromise::ResolveOrRejectValue&& aResult)
              MOZ_CAN_RUN_SCRIPT {
                MOZ_ASSERT(self->mProcessingMessage);
                MOZ_ASSERT(self->mProcessingMessage->is<DecodeMessage>());
                MOZ_ASSERT(self->mState == CodecState::Configured);
                MOZ_ASSERT(self->mAgent);
                MOZ_ASSERT(id == self->mAgent->mId);
                MOZ_ASSERT(self->mActiveConfig);

                auto& msg = self->mProcessingMessage->as<DecodeMessage>();
                LOGV("VideoDecoder %p, DecodeAgent #%d %s has been %s",
                     self.get(), id, msg.mTitle.get(),
                     aResult.IsResolve() ? "resolved" : "rejected");

                nsCString title = msg.mTitle;

                msg.mRequest.Complete();
                self->mProcessingMessage.reset();

                if (aResult.IsReject()) {
                  // The spec asks to queue a task to run close-VideoDecoder
                  // with an EncodingError so we log the exact error here.
                  const MediaResult& error = aResult.RejectValue();
                  LOGE("VideoDecoder %p, DecodeAgent #%d %s failed: %s",
                       self.get(), id, title.get(), error.Description().get());
                  self->ScheduleClose(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
                  return;  // No further process
                }

                MOZ_ASSERT(aResult.IsResolve());
                nsTArray<RefPtr<MediaData>> data =
                    std::move(aResult.ResolveValue());
                if (data.IsEmpty()) {
                  LOGV("VideoDecoder %p got no data for %s", self.get(),
                       title.get());
                } else {
                  LOGV(
                      "VideoDecoder %p, schedule %zu decoded data output for "
                      "%s",
                      self.get(), data.Length(), title.get());
                  self->ScheduleOutputVideoFrames(std::move(data), title);
                }

                self->ProcessControlMessageQueue();
              })
      ->Track(msg.mRequest);

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
bool VideoDecoder::CreateDecoderAgent(UniquePtr<VideoDecoderConfig>&& aConfig,
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

  mAgent = DecoderAgent::Create(std::move(aInfo));
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
