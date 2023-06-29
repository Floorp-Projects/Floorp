/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VideoDecoder.h"
#include "mozilla/dom/VideoDecoderBinding.h"

#include "DecoderTraits.h"
#include "MediaContainerType.h"
#include "VideoUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebCodecsUtils.h"
#include "nsReadableUtils.h"

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

#ifdef LOGE
#  undef LOGE
#endif  // LOGE
#define LOGE(msg, ...) LOG_INTERNAL(Error, msg, ##__VA_ARGS__)

NS_IMPL_CYCLE_COLLECTION_INHERITED(VideoDecoder, DOMEventTargetHelper)
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

// https://w3c.github.io/webcodecs/#check-configuration-support
static bool CanDecode(const VideoDecoderConfig& aConfig) {
  // TODO: Instead of calling CanHandleContainerType with the guessed the
  // containers, DecoderTraits should provide an API to tell if a codec is
  // decodable or not.
  const auto codec = NS_ConvertUTF16toUTF8(aConfig.mCodec);
  for (const nsCString& container : GuessContainers(aConfig.mCodec)) {
    nsPrintfCString mime("video/%s; codecs=%s", container.get(), codec.get());
    if (aConfig.mCodedWidth.WasPassed()) {
      mime.Append(nsPrintfCString("; width=%d", aConfig.mCodedWidth.Value()));
    }
    if (aConfig.mCodedHeight.WasPassed()) {
      mime.Append(nsPrintfCString("; height=%d", aConfig.mCodedHeight.Value()));
    }
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

// https://w3c.github.io/webcodecs/#clone-configuration
static Result<VideoDecoderConfig, nsresult> CloneConfiguration(
    const GlobalObject& aGlobal, const VideoDecoderConfig& aConfig) {
  MOZ_ASSERT(IsValid(aConfig));

  VideoDecoderConfig c;
  c.mCodec = aConfig.mCodec;
  if (aConfig.mCodedHeight.WasPassed()) {
    c.mCodedHeight.Construct(aConfig.mCodedHeight.Value());
  }
  if (aConfig.mCodedWidth.WasPassed()) {
    c.mCodedWidth.Construct(aConfig.mCodedWidth.Value());
  }
  if (aConfig.mColorSpace.WasPassed()) {
    c.mColorSpace.Construct(aConfig.mColorSpace.Value());
  }
  if (aConfig.mDescription.WasPassed()) {
    auto r = CloneBuffer(aGlobal.Context(), aConfig.mDescription.Value());
    if (r.isErr()) {
      return Err(r.unwrapErr());
    }
    c.mDescription.Construct(r.unwrap());
  }
  if (aConfig.mDisplayAspectHeight.WasPassed()) {
    c.mDisplayAspectHeight.Construct(aConfig.mDisplayAspectHeight.Value());
  }
  if (aConfig.mDisplayAspectWidth.WasPassed()) {
    c.mDisplayAspectWidth.Construct(aConfig.mDisplayAspectWidth.Value());
  }
  c.mHardwareAcceleration = aConfig.mHardwareAcceleration;
  if (aConfig.mOptimizeForLatency.WasPassed()) {
    c.mOptimizeForLatency.Construct(aConfig.mOptimizeForLatency.Value());
  }
  return c;
}

VideoDecoder::VideoDecoder(nsIGlobalObject* aParent,
                           RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
                           RefPtr<VideoFrameOutputCallback>&& aOutputCallback)
    : DOMEventTargetHelper(aParent),
      mErrorCallback(std::move(aErrorCallback)),
      mOutputCallback(std::move(aOutputCallback)),
      mState(CodecState::Unconfigured) {
  MOZ_ASSERT(mErrorCallback);
  MOZ_ASSERT(mOutputCallback);
  LOG("VideoDecoder %p ctor", this);
}

VideoDecoder::~VideoDecoder() { LOG("VideoDecoder %p dtor", this); }

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

uint32_t VideoDecoder::DecodeQueueSize() const { return 0; }

already_AddRefed<EventHandlerNonNull> VideoDecoder::GetOndequeue() const {
  return nullptr;
}

void VideoDecoder::SetOndequeue(EventHandlerNonNull* arg) {}

void VideoDecoder::Configure(const VideoDecoderConfig& config,
                             ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

void VideoDecoder::Decode(EncodedVideoChunk& chunk, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
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

void VideoDecoder::Close(ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
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

  return Ok();
}

#undef LOG
#undef LOGE
#undef LOG_INTERNAL

}  // namespace mozilla::dom
