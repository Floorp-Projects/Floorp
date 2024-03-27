/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AudioDecoder.h"
#include "mozilla/dom/AudioDecoderBinding.h"

#include "DecoderTraits.h"
#include "MediaContainerType.h"
#include "MediaData.h"
#include "VideoUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/Try.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/AudioDataBinding.h"
#include "mozilla/dom/EncodedAudioChunk.h"
#include "mozilla/dom/EncodedAudioChunkBinding.h"
#include "mozilla/dom/ImageUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebCodecsUtils.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"

extern mozilla::LazyLogModule gWebCodecsLog;

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

NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioDecoder, DOMEventTargetHelper,
                                   mErrorCallback, mOutputCallback)
NS_IMPL_ADDREF_INHERITED(AudioDecoder, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AudioDecoder, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioDecoder)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/*
 * Below are helper classes
 */

AudioDecoderConfigInternal::AudioDecoderConfigInternal(
    const nsAString& aCodec, uint32_t aSampleRate, uint32_t aNumberOfChannels,
    Maybe<RefPtr<MediaByteBuffer>>&& aDescription)
    : mCodec(aCodec),
      mSampleRate(aSampleRate),
      mNumberOfChannels(aNumberOfChannels),
      mDescription(std::move(aDescription)) {}

/*static*/
UniquePtr<AudioDecoderConfigInternal> AudioDecoderConfigInternal::Create(
    const AudioDecoderConfig& aConfig) {
  nsCString errorMessage;
  if (!AudioDecoderTraits::Validate(aConfig, errorMessage)) {
    LOGE("Failed to create AudioDecoderConfigInternal: %s", errorMessage.get());
    return nullptr;
  }

  Maybe<RefPtr<MediaByteBuffer>> description;
  if (aConfig.mDescription.WasPassed()) {
    auto rv = GetExtraDataFromArrayBuffer(aConfig.mDescription.Value());
    if (rv.isErr()) {  // Invalid description data.
      nsCString error;
      GetErrorName(rv.unwrapErr(), error);
      LOGE(
          "Failed to create AudioDecoderConfigInternal due to invalid "
          "description data. Error: %s",
          error.get());
      return nullptr;
    }
    description.emplace(rv.unwrap());
  }

  return UniquePtr<AudioDecoderConfigInternal>(new AudioDecoderConfigInternal(
      aConfig.mCodec, aConfig.mSampleRate, aConfig.mNumberOfChannels,
      std::move(description)));
}

/*
 * The followings are helpers for AudioDecoder methods
 */

struct AudioMIMECreateParam {
  explicit AudioMIMECreateParam(const AudioDecoderConfigInternal& aConfig)
      : mParsedCodec(ParseCodecString(aConfig.mCodec).valueOr(EmptyString())) {}
  explicit AudioMIMECreateParam(const AudioDecoderConfig& aConfig)
      : mParsedCodec(ParseCodecString(aConfig.mCodec).valueOr(EmptyString())) {}

  const nsString mParsedCodec;
};

// Map between WebCodecs pcm types as strings and codec numbers
// All other codecs
static nsTArray<nsCString> GuessMIMETypes(const AudioMIMECreateParam& aParam) {
  nsCString codec = NS_ConvertUTF16toUTF8(aParam.mParsedCodec);
  nsTArray<nsCString> types;
  for (const nsCString& container : GuessContainers(aParam.mParsedCodec)) {
    codec = ConvertCodecName(container, codec);
    nsPrintfCString mime("audio/%s; codecs=%s", container.get(), codec.get());
    types.AppendElement(mime);
  }
  return types;
}

// https://w3c.github.io/webcodecs/#check-configuration-support
template <typename Config>
static bool CanDecodeAudio(const Config& aConfig) {
  auto param = AudioMIMECreateParam(aConfig);
  if (!IsSupportedAudioCodec(param.mParsedCodec)) {
    return false;
  }
  if (IsOnAndroid() && IsAACCodecString(param.mParsedCodec)) {
    return false;
  }
  // TODO: Instead of calling CanHandleContainerType with the guessed the
  // containers, DecoderTraits should provide an API to tell if a codec is
  // decodable or not.
  for (const nsCString& mime : GuessMIMETypes(param)) {
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
    const AudioDecoderConfigInternal& aConfig) {
  // TODO: Instead of calling GetTracksInfo with the guessed containers,
  // DecoderTraits should provide an API to create the TrackInfo directly.
  for (const nsCString& mime : GuessMIMETypes(AudioMIMECreateParam(aConfig))) {
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

static Result<Ok, nsresult> CloneConfiguration(
    RootedDictionary<AudioDecoderConfig>& aDest, JSContext* aCx,
    const AudioDecoderConfig& aConfig, ErrorResult& aRv) {
  aDest.mCodec = aConfig.mCodec;
  if (aConfig.mDescription.WasPassed()) {
    aDest.mDescription.Construct();
    MOZ_TRY(CloneBuffer(aCx, aDest.mDescription.Value(),
                        aConfig.mDescription.Value(), aRv));
  }

  aDest.mNumberOfChannels = aConfig.mNumberOfChannels;
  aDest.mSampleRate = aConfig.mSampleRate;

  return Ok();
}

// https://w3c.github.io/webcodecs/#create-a-audiodata
static RefPtr<AudioData> CreateAudioData(nsIGlobalObject* aGlobalObject,
                                         mozilla::AudioData* aData) {
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT(aData);

  auto buf = aData->MoveableData();
  // TODO: Ensure buf.Length() is a multiple of aData->mChannels and put it into
  // AssertedCast<uint32_t> (sinze return type of buf.Length() is size_t).
  uint32_t frames = buf.Length() / aData->mChannels;
  RefPtr<AudioDataResource> resource = AudioDataResource::Create(Span{
      reinterpret_cast<uint8_t*>(buf.Data()), buf.Length() * sizeof(float)});
  return MakeRefPtr<AudioData>(aGlobalObject, resource.forget(),
                               aData->mTime.ToMicroseconds(), aData->mChannels,
                               frames, AssertedCast<float>(aData->mRate),
                               mozilla::dom::AudioSampleFormat::F32);
}

/* static */
bool AudioDecoderTraits::IsSupported(
    const AudioDecoderConfigInternal& aConfig) {
  return CanDecodeAudio(aConfig);
}

/* static */
Result<UniquePtr<TrackInfo>, nsresult> AudioDecoderTraits::CreateTrackInfo(
    const AudioDecoderConfigInternal& aConfig) {
  LOG("Create a AudioInfo from %s config",
      NS_ConvertUTF16toUTF8(aConfig.mCodec).get());

  nsTArray<UniquePtr<TrackInfo>> tracks = GetTracksInfo(aConfig);
  if (tracks.Length() != 1 || tracks[0]->GetType() != TrackInfo::kAudioTrack) {
    LOGE("Failed to get TrackInfo");
    return Err(NS_ERROR_INVALID_ARG);
  }

  UniquePtr<TrackInfo> track(std::move(tracks[0]));
  AudioInfo* ai = track->GetAsAudioInfo();
  if (!ai) {
    LOGE("Failed to get AudioInfo");
    return Err(NS_ERROR_INVALID_ARG);
  }

  if (aConfig.mDescription.isSome()) {
    RefPtr<MediaByteBuffer> buf;
    buf = aConfig.mDescription.value();
    if (buf) {
      LOG("The given config has %zu bytes of description data", buf->Length());
      ai->mCodecSpecificConfig =
          AudioCodecSpecificVariant{AudioCodecSpecificBinaryBlob{buf}};
    }
  }

  ai->mChannels = aConfig.mNumberOfChannels;
  ai->mRate = aConfig.mSampleRate;

  LOG("Created AudioInfo %s (%" PRIu32 "ch %" PRIu32
      "Hz - with extra-data: %s)",
      NS_ConvertUTF16toUTF8(aConfig.mCodec).get(), ai->mChannels, ai->mChannels,
      aConfig.mDescription.isSome() ? "yes" : "no");

  return track;
}

// https://w3c.github.io/webcodecs/#valid-audiodecoderconfig
/* static */
bool AudioDecoderTraits::Validate(const AudioDecoderConfig& aConfig,
                                  nsCString& aErrorMessage) {
  Maybe<nsString> codec = ParseCodecString(aConfig.mCodec);
  if (!codec || codec->IsEmpty()) {
    LOGE("Validating AudioDecoderConfig: invalid codec string");

    aErrorMessage.AppendPrintf("Invalid codec string %s",
                               NS_ConvertUTF16toUTF8(aConfig.mCodec).get());
    return false;
  }

  LOG("Validating AudioDecoderConfig: codec: %s %uch %uHz %s extradata",
      NS_ConvertUTF16toUTF8(codec.value()).get(), aConfig.mNumberOfChannels,
      aConfig.mSampleRate, aConfig.mDescription.WasPassed() ? "w/" : "no");

  if (aConfig.mNumberOfChannels == 0) {
    aErrorMessage.AppendPrintf("Invalid number of channels of %u",
                               aConfig.mNumberOfChannels);
    return false;
  }

  if (aConfig.mSampleRate == 0) {
    aErrorMessage.AppendPrintf("Invalid sample-rate of %u",
                               aConfig.mNumberOfChannels);
    return false;
  }

  bool detached =
      aConfig.mDescription.WasPassed() &&
      (aConfig.mDescription.Value().IsArrayBuffer()
           ? JS::ArrayBuffer::fromObject(
                 aConfig.mDescription.Value().GetAsArrayBuffer().Obj())
                 .isDetached()
           : JS::ArrayBufferView::fromObject(
                 aConfig.mDescription.Value().GetAsArrayBufferView().Obj())
                 .isDetached());

  if (detached) {
    LOGE("description is detached.");
    return false;
  }

  return true;
}

/* static */
UniquePtr<AudioDecoderConfigInternal> AudioDecoderTraits::CreateConfigInternal(
    const AudioDecoderConfig& aConfig) {
  return AudioDecoderConfigInternal::Create(aConfig);
}

/* static */
bool AudioDecoderTraits::IsKeyChunk(const EncodedAudioChunk& aInput) {
  return aInput.Type() == EncodedAudioChunkType::Key;
}

/* static */
UniquePtr<EncodedAudioChunkData> AudioDecoderTraits::CreateInputInternal(
    const EncodedAudioChunk& aInput) {
  return aInput.Clone();
}

/*
 * Below are AudioDecoder implementation
 */

AudioDecoder::AudioDecoder(nsIGlobalObject* aParent,
                           RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
                           RefPtr<AudioDataOutputCallback>&& aOutputCallback)
    : DecoderTemplate(aParent, std::move(aErrorCallback),
                      std::move(aOutputCallback)) {
  MOZ_ASSERT(mErrorCallback);
  MOZ_ASSERT(mOutputCallback);
  LOG("AudioDecoder %p ctor", this);
}

AudioDecoder::~AudioDecoder() {
  LOG("AudioDecoder %p dtor", this);
  Unused << ResetInternal(NS_ERROR_DOM_ABORT_ERR);
}

JSObject* AudioDecoder::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnOwningThread();

  return AudioDecoder_Binding::Wrap(aCx, this, aGivenProto);
}

// https://w3c.github.io/webcodecs/#dom-audiodecoder-audiodecoder
/* static */
already_AddRefed<AudioDecoder> AudioDecoder::Constructor(
    const GlobalObject& aGlobal, const AudioDecoderInit& aInit,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return MakeAndAddRef<AudioDecoder>(
      global.get(), RefPtr<WebCodecsErrorCallback>(aInit.mError),
      RefPtr<AudioDataOutputCallback>(aInit.mOutput));
}

// https://w3c.github.io/webcodecs/#dom-audiodecoder-isconfigsupported
/* static */
already_AddRefed<Promise> AudioDecoder::IsConfigSupported(
    const GlobalObject& aGlobal, const AudioDecoderConfig& aConfig,
    ErrorResult& aRv) {
  LOG("AudioDecoder::IsConfigSupported, config: %s",
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

  nsCString errorMessage;
  if (!AudioDecoderTraits::Validate(aConfig, errorMessage)) {
    p->MaybeRejectWithTypeError(errorMessage);
    return p.forget();
  }

  RootedDictionary<AudioDecoderConfig> config(aGlobal.Context());
  auto r = CloneConfiguration(config, aGlobal.Context(), aConfig, aRv);
  if (r.isErr()) {
    // This can only be an OOM: all members to clone are known to be valid
    // because this is check by ::Validate above.
    MOZ_ASSERT(r.inspectErr() == NS_ERROR_OUT_OF_MEMORY &&
               aRv.ErrorCodeIs(NS_ERROR_OUT_OF_MEMORY));
    return p.forget();
  }

  bool canDecode = CanDecodeAudio(config);
  RootedDictionary<AudioDecoderSupport> s(aGlobal.Context());
  s.mConfig.Construct(std::move(config));
  s.mSupported.Construct(canDecode);

  p->MaybeResolve(s);
  return p.forget();
}

already_AddRefed<MediaRawData> AudioDecoder::InputDataToMediaRawData(
    UniquePtr<EncodedAudioChunkData>&& aData, TrackInfo& aInfo,
    const AudioDecoderConfigInternal& aConfig) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aInfo.GetAsAudioInfo());

  if (!aData) {
    LOGE("No data for conversion");
    return nullptr;
  }

  RefPtr<MediaRawData> sample = aData->TakeData();
  if (!sample) {
    LOGE("Take no data for conversion");
    return nullptr;
  }

  LOGV(
      "EncodedAudioChunkData %p converted to %zu-byte MediaRawData - time: "
      "%" PRIi64 "us, timecode: %" PRIi64 "us, duration: %" PRIi64
      "us, key-frame: %s",
      aData.get(), sample->Size(), sample->mTime.ToMicroseconds(),
      sample->mTimecode.ToMicroseconds(), sample->mDuration.ToMicroseconds(),
      sample->mKeyframe ? "yes" : "no");

  return sample.forget();
}

nsTArray<RefPtr<AudioData>> AudioDecoder::DecodedDataToOutputType(
    nsIGlobalObject* aGlobalObject, const nsTArray<RefPtr<MediaData>>&& aData,
    AudioDecoderConfigInternal& aConfig) {
  AssertIsOnOwningThread();

  nsTArray<RefPtr<AudioData>> frames;
  for (const RefPtr<MediaData>& data : aData) {
    MOZ_RELEASE_ASSERT(data->mType == MediaData::Type::AUDIO_DATA);
    RefPtr<mozilla::AudioData> d(data->As<mozilla::AudioData>());
    frames.AppendElement(CreateAudioData(aGlobalObject, d.get()));
  }
  return frames;
}

#undef LOG
#undef LOGW
#undef LOGE
#undef LOGV
#undef LOG_INTERNAL

}  // namespace mozilla::dom
