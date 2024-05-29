/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AudioEncoder.h"
#include "EncoderTraits.h"
#include "mozilla/dom/AudioEncoderBinding.h"

#include "EncoderConfig.h"
#include "EncoderTypes.h"
#include "MediaData.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/AudioDataBinding.h"
#include "mozilla/dom/EncodedAudioChunk.h"
#include "mozilla/dom/EncodedAudioChunkBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebCodecsUtils.h"
#include "EncoderConfig.h"

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

NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioEncoder, DOMEventTargetHelper,
                                   mErrorCallback, mOutputCallback)
NS_IMPL_ADDREF_INHERITED(AudioEncoder, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AudioEncoder, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioEncoder)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/*
 * Below are helper classes
 */
AudioEncoderConfigInternal::AudioEncoderConfigInternal(
    const nsAString& aCodec, Maybe<uint32_t> aSampleRate,
    Maybe<uint32_t> aNumberOfChannels, Maybe<uint32_t> aBitrate,
    BitrateMode aBitrateMode)
    : mCodec(aCodec),
      mSampleRate(aSampleRate),
      mNumberOfChannels(aNumberOfChannels),
      mBitrate(aBitrate),
      mBitrateMode(aBitrateMode) {}

AudioEncoderConfigInternal::AudioEncoderConfigInternal(
    const AudioEncoderConfig& aConfig)
    : AudioEncoderConfigInternal(
          aConfig.mCodec, OptionalToMaybe(aConfig.mSampleRate),
          OptionalToMaybe(aConfig.mNumberOfChannels),
          OptionalToMaybe(aConfig.mBitrate), aConfig.mBitrateMode) {
  DebugOnly<nsCString> errorMessage;
  if (aConfig.mCodec.EqualsLiteral("opus") && aConfig.mOpus.WasPassed()) {
    // All values are in range at this point, the config is known valid.
    OpusSpecific specific;
    if (aConfig.mOpus.Value().mComplexity.WasPassed()) {
      specific.mComplexity = aConfig.mOpus.Value().mComplexity.Value();
    } else {
      // https://w3c.github.io/webcodecs/opus_codec_registration.html#dom-opusencoderconfig-complexity
      // If no value is specificied, the default value is platform-specific:
      // User Agents SHOULD set a default of 5 for mobile platforms, and a
      // default of 9 for all other platforms.
      if (IsOnAndroid()) {
        specific.mComplexity = 5;
      } else {
        specific.mComplexity = 9;
      }
    }
    specific.mApplication = OpusSpecific::Application::Unspecified;
    specific.mFrameDuration = aConfig.mOpus.Value().mFrameDuration;
    specific.mPacketLossPerc = aConfig.mOpus.Value().mPacketlossperc;
    specific.mUseDTX = aConfig.mOpus.Value().mUsedtx;
    specific.mUseInBandFEC = aConfig.mOpus.Value().mUseinbandfec;
    mSpecific.emplace(specific);
  }
  MOZ_ASSERT(AudioEncoderTraits::Validate(aConfig, errorMessage));
}

AudioEncoderConfigInternal::AudioEncoderConfigInternal(
    const AudioEncoderConfigInternal& aConfig)
    : AudioEncoderConfigInternal(aConfig.mCodec, aConfig.mSampleRate,
                                 aConfig.mNumberOfChannels, aConfig.mBitrate,
                                 aConfig.mBitrateMode) {}

void AudioEncoderConfigInternal::SetSpecific(
    const EncoderConfig::CodecSpecific& aSpecific) {
  mSpecific.emplace(aSpecific);
}

/*
 * The followings are helpers for AudioEncoder methods
 */

static void CloneConfiguration(RootedDictionary<AudioEncoderConfig>& aDest,
                               JSContext* aCx,
                               const AudioEncoderConfig& aConfig) {
  aDest.mCodec = aConfig.mCodec;

  if (aConfig.mNumberOfChannels.WasPassed()) {
    aDest.mNumberOfChannels.Construct(aConfig.mNumberOfChannels.Value());
  }
  if (aConfig.mSampleRate.WasPassed()) {
    aDest.mSampleRate.Construct(aConfig.mSampleRate.Value());
  }
  if (aConfig.mBitrate.WasPassed()) {
    aDest.mBitrate.Construct(aConfig.mBitrate.Value());
  }
  if (aConfig.mOpus.WasPassed()) {
    aDest.mOpus.Construct(aConfig.mOpus.Value());
    // Handle the default value manually since it's different on mobile
    if (!aConfig.mOpus.Value().mComplexity.WasPassed()) {
      if (IsOnAndroid()) {
        aDest.mOpus.Value().mComplexity.Construct(5);
      } else {
        aDest.mOpus.Value().mComplexity.Construct(9);
      }
    }
  }
  aDest.mBitrateMode = aConfig.mBitrateMode;
}

static bool IsAudioEncodeSupported(const nsAString& aCodec) {
  LOG("IsEncodeSupported: %s", NS_ConvertUTF16toUTF8(aCodec).get());

  return aCodec.EqualsLiteral("opus") || aCodec.EqualsLiteral("vorbis");
}

static bool CanEncode(const RefPtr<AudioEncoderConfigInternal>& aConfig,
                      nsCString& aErrorMessage) {
  auto parsedCodecString =
      ParseCodecString(aConfig->mCodec).valueOr(EmptyString());
  // TODO: Enable WebCodecs on Android (Bug 1840508)
  if (IsOnAndroid()) {
    return false;
  }
  if (!IsAudioEncodeSupported(parsedCodecString)) {
    return false;
  }

  if (aConfig->mNumberOfChannels.value() > 256) {
    aErrorMessage.AppendPrintf(
        "Invalid number of channels, supported range is between 1 and 256");
    return false;
  }

  // Somewhat arbitrarily chosen, but reflects real-life and what the rest of
  // Gecko does.
  if (aConfig->mSampleRate.value() < 3000 ||
      aConfig->mSampleRate.value() > 384000) {
    aErrorMessage.AppendPrintf(
        "Invalid sample-rate of %d, supported range is 3000Hz to 384000Hz",
        aConfig->mSampleRate.value());
    return false;
  }

  return EncoderSupport::Supports(aConfig);
}

nsCString AudioEncoderConfigInternal::ToString() const {
  nsCString rv;

  rv.AppendPrintf("AudioEncoderConfigInternal: %s",
                  NS_ConvertUTF16toUTF8(mCodec).get());
  if (mSampleRate) {
    rv.AppendPrintf(" %" PRIu32 "Hz", mSampleRate.value());
  }
  if (mNumberOfChannels) {
    rv.AppendPrintf(" %" PRIu32 "ch", mNumberOfChannels.value());
  }
  if (mBitrate) {
    rv.AppendPrintf(" %" PRIu32 "bps", mBitrate.value());
  }
  rv.AppendPrintf(" (%s)", mBitrateMode == mozilla::dom::BitrateMode::Constant
                               ? "CRB"
                               : "VBR");

  return rv;
}

EncoderConfig AudioEncoderConfigInternal::ToEncoderConfig() const {
  const mozilla::BitrateMode bitrateMode =
      mBitrateMode == mozilla::dom::BitrateMode::Constant
          ? mozilla::BitrateMode::Constant
          : mozilla::BitrateMode::Variable;

  CodecType type = CodecType::Opus;
  Maybe<EncoderConfig::CodecSpecific> specific;
  if (mCodec.EqualsLiteral("opus")) {
    type = CodecType::Opus;
    MOZ_ASSERT(mSpecific.isNothing() || mSpecific->is<OpusSpecific>());
    specific = mSpecific;
  } else if (mCodec.EqualsLiteral("vorbis")) {
    type = CodecType::Vorbis;
  } else if (mCodec.EqualsLiteral("flac")) {
    type = CodecType::Flac;
  } else if (StringBeginsWith(mCodec, u"pcm-"_ns)) {
    type = CodecType::PCM;
  } else if (mCodec.EqualsLiteral("ulaw")) {
    type = CodecType::PCM;
  } else if (mCodec.EqualsLiteral("alaw")) {
    type = CodecType::PCM;
  } else if (StringBeginsWith(mCodec, u"mp4a."_ns)) {
    type = CodecType::AAC;
  }

  // This should have been checked ahead of time -- we can't encode without
  // knowing the sample-rate or the channel count at the very least.
  MOZ_ASSERT(mSampleRate.value());
  MOZ_ASSERT(mNumberOfChannels.value());

  return EncoderConfig(type, mNumberOfChannels.value(), bitrateMode,
                       AssertedCast<uint32_t>(mSampleRate.value()),
                       mBitrate.valueOr(0), specific);
}

bool AudioEncoderConfigInternal::Equals(
    const AudioEncoderConfigInternal& aOther) const {
  return false;
}

bool AudioEncoderConfigInternal::CanReconfigure(
    const AudioEncoderConfigInternal& aOther) const {
  return false;
}

already_AddRefed<WebCodecsConfigurationChangeList>
AudioEncoderConfigInternal::Diff(
    const AudioEncoderConfigInternal& aOther) const {
  return MakeRefPtr<WebCodecsConfigurationChangeList>().forget();
}

/* static */
bool AudioEncoderTraits::IsSupported(
    const AudioEncoderConfigInternal& aConfig) {
  nsCString errorMessage;
  bool canEncode =
      CanEncode(MakeRefPtr<AudioEncoderConfigInternal>(aConfig), errorMessage);
  if (!canEncode) {
    LOGE("Can't encode configuration %s: %s", aConfig.ToString().get(),
         errorMessage.get());
  }
  return canEncode;
}

// https://w3c.github.io/webcodecs/#valid-audioencoderconfig
/* static */
bool AudioEncoderTraits::Validate(const AudioEncoderConfig& aConfig,
                                  nsCString& aErrorMessage) {
  Maybe<nsString> codec = ParseCodecString(aConfig.mCodec);
  if (!codec || codec->IsEmpty()) {
    LOGE("Validating AudioEncoderConfig: invalid codec string");
    return false;
  }

  if (!aConfig.mNumberOfChannels.WasPassed()) {
    aErrorMessage.AppendPrintf("Channel count required");
    return false;
  }
  if (aConfig.mNumberOfChannels.Value() == 0) {
    aErrorMessage.AppendPrintf(
        "Invalid number of channels, supported range is between 1 and 256");
    return false;
  }
  if (!aConfig.mSampleRate.WasPassed()) {
    aErrorMessage.AppendPrintf("Sample-rate required");
    return false;
  }
  if (aConfig.mSampleRate.Value() == 0) {
    aErrorMessage.AppendPrintf("Invalid sample-rate of 0");
    return false;
  }

  if (aConfig.mBitrate.WasPassed() &&
      aConfig.mBitrate.Value() > std::numeric_limits<int>::max()) {
    aErrorMessage.AppendPrintf("Invalid config: bitrate value too large");
    return false;
  }

  if (codec->EqualsLiteral("opus")) {
    // This comes from
    // https://w3c.github.io/webcodecs/opus_codec_registration.html#opus-encoder-config
    if (aConfig.mBitrate.WasPassed() && (aConfig.mBitrate.Value() < 6000 ||
                                         aConfig.mBitrate.Value() > 510000)) {
      aErrorMessage.AppendPrintf(
          "Invalid config: bitrate value outside of [6k, 510k] for opus");
      return false;
    }
    if (aConfig.mOpus.WasPassed()) {
      // Verify value ranges
      const std::array validFrameDurationUs = {2500,  5000,  10000,
                                               20000, 40000, 60000};
      if (std::find(validFrameDurationUs.begin(), validFrameDurationUs.end(),
                    aConfig.mOpus.Value().mFrameDuration) ==
          validFrameDurationUs.end()) {
        aErrorMessage.AppendPrintf("Invalid config: invalid frame duration");
        return false;
      }
      if (aConfig.mOpus.Value().mComplexity.WasPassed() &&
          aConfig.mOpus.Value().mComplexity.Value() > 10) {
        aErrorMessage.AppendPrintf(
            "Invalid config: Opus complexity greater than 10");
        return false;
      }
      if (aConfig.mOpus.Value().mPacketlossperc > 100) {
        aErrorMessage.AppendPrintf(
            "Invalid config: Opus packet loss percentage greater than 100");
        return false;
      }
    }
  }

  return true;
}

/* static */
RefPtr<AudioEncoderConfigInternal> AudioEncoderTraits::CreateConfigInternal(
    const AudioEncoderConfig& aConfig) {
  nsCString errorMessage;
  if (!AudioEncoderTraits::Validate(aConfig, errorMessage)) {
    return nullptr;
  }
  return MakeRefPtr<AudioEncoderConfigInternal>(aConfig);
}

/* static */
RefPtr<mozilla::AudioData> AudioEncoderTraits::CreateInputInternal(
    const dom::AudioData& aInput,
    const dom::VideoEncoderEncodeOptions& /* unused */) {
  return aInput.ToAudioData();
}

/*
 * Below are AudioEncoder implementation
 */

AudioEncoder::AudioEncoder(
    nsIGlobalObject* aParent, RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
    RefPtr<EncodedAudioChunkOutputCallback>&& aOutputCallback)
    : EncoderTemplate(aParent, std::move(aErrorCallback),
                      std::move(aOutputCallback)) {
  MOZ_ASSERT(mErrorCallback);
  MOZ_ASSERT(mOutputCallback);
  LOG("AudioEncoder %p ctor", this);
}

AudioEncoder::~AudioEncoder() {
  LOG("AudioEncoder %p dtor", this);
  Unused << ResetInternal(NS_ERROR_DOM_ABORT_ERR);
}

JSObject* AudioEncoder::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnOwningThread();

  return AudioEncoder_Binding::Wrap(aCx, this, aGivenProto);
}

// https://w3c.github.io/webcodecs/#dom-audioencoder-audioencoder
/* static */
already_AddRefed<AudioEncoder> AudioEncoder::Constructor(
    const GlobalObject& aGlobal, const AudioEncoderInit& aInit,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return MakeAndAddRef<AudioEncoder>(
      global.get(), RefPtr<WebCodecsErrorCallback>(aInit.mError),
      RefPtr<EncodedAudioChunkOutputCallback>(aInit.mOutput));
}

// https://w3c.github.io/webcodecs/#dom-audioencoder-isconfigsupported
/* static */
already_AddRefed<Promise> AudioEncoder::IsConfigSupported(
    const GlobalObject& aGlobal, const AudioEncoderConfig& aConfig,
    ErrorResult& aRv) {
  LOG("AudioEncoder::IsConfigSupported, config: %s",
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
  if (!AudioEncoderTraits::Validate(aConfig, errorMessage)) {
    p->MaybeRejectWithTypeError(errorMessage);
    return p.forget();
  }

  // TODO: Move the following works to another thread to unblock the current
  // thread, as what spec suggests.

  RootedDictionary<AudioEncoderConfig> config(aGlobal.Context());
  CloneConfiguration(config, aGlobal.Context(), aConfig);

  bool supportedAudioCodec = IsSupportedAudioCodec(aConfig.mCodec);
  auto configInternal = MakeRefPtr<AudioEncoderConfigInternal>(aConfig);
  bool canEncode = CanEncode(configInternal, errorMessage);
  if (!canEncode) {
    LOG("CanEncode failed: %s", errorMessage.get());
  }
  RootedDictionary<AudioEncoderSupport> s(aGlobal.Context());
  s.mConfig.Construct(std::move(config));
  s.mSupported.Construct(supportedAudioCodec && canEncode);

  p->MaybeResolve(s);
  return p.forget();
}

RefPtr<EncodedAudioChunk> AudioEncoder::EncodedDataToOutputType(
    nsIGlobalObject* aGlobalObject, const RefPtr<MediaRawData>& aData) {
  AssertIsOnOwningThread();

  // Package into an EncodedAudioChunk
  auto buffer =
      MakeRefPtr<MediaAlignedByteBuffer>(aData->Data(), aData->Size());
  auto encoded = MakeRefPtr<EncodedAudioChunk>(
      aGlobalObject, buffer.forget(), EncodedAudioChunkType::Key,
      aData->mTime.ToMicroseconds(),
      aData->mDuration.IsZero() ? Nothing()
                                : Some(aData->mDuration.ToMicroseconds()));
  return encoded;
}

void AudioEncoder::EncoderConfigToDecoderConfig(
    JSContext* aCx, const RefPtr<MediaRawData>& aRawData,
    const AudioEncoderConfigInternal& aSrcConfig,
    AudioDecoderConfig& aDestConfig) const {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aSrcConfig.mSampleRate.isSome());
  MOZ_ASSERT(aSrcConfig.mNumberOfChannels.isSome());

  uint32_t sampleRate = aSrcConfig.mSampleRate.value();
  uint32_t channelCount = aSrcConfig.mNumberOfChannels.value();
  // Check if the encoder had to modify the settings because of codec
  // constraints. e.g. FFmpegAudioEncoder can encode any sample-rate, but if the
  // codec is Opus, then it will resample the audio one of the specific rates
  // supported by the encoder.
  if (aRawData->mConfig) {
    sampleRate = aRawData->mConfig->mSampleRate;
    channelCount = aRawData->mConfig->mNumberOfChannels;
  }

  aDestConfig.mCodec = aSrcConfig.mCodec;
  aDestConfig.mNumberOfChannels = channelCount;
  aDestConfig.mSampleRate = sampleRate;

  if (aRawData->mExtraData && !aRawData->mExtraData->IsEmpty()) {
    Span<const uint8_t> description(aRawData->mExtraData->Elements(),
                                    aRawData->mExtraData->Length());
    if (!CopyExtradataToDescription(aCx, description,
                                    aDestConfig.mDescription.Construct())) {
      LOGE("Failed to copy extra data");
    }
  }
}

#undef LOG
#undef LOGW
#undef LOGE
#undef LOGV
#undef LOG_INTERNAL

}  // namespace mozilla::dom
