/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleATDecoder.h"
#include "Adts.h"
#include "AppleUtils.h"
#include "MP4Decoder.h"
#include "MediaInfo.h"
#include "VideoUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

#define LOG(...) DDMOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, __VA_ARGS__)
#define LOGEX(_this, ...) \
  DDMOZ_LOGEX(_this, sPDMLog, mozilla::LogLevel::Debug, __VA_ARGS__)
#define FourCC2Str(n) \
  ((char[5]){(char)(n >> 24), (char)(n >> 16), (char)(n >> 8), (char)(n), 0})

namespace mozilla {

AppleATDecoder::AppleATDecoder(const AudioInfo& aConfig)
    : mConfig(aConfig),
      mFileStreamError(false),
      mConverter(nullptr),
      mOutputFormat(),
      mStream(nullptr),
      mParsedFramesForAACMagicCookie(0),
      mErrored(false) {
  MOZ_COUNT_CTOR(AppleATDecoder);
  LOG("Creating Apple AudioToolbox decoder");
  LOG("Audio Decoder configuration: %s %d Hz %d channels %d bits per channel",
      mConfig.mMimeType.get(), mConfig.mRate, mConfig.mChannels,
      mConfig.mBitDepth);

  if (mConfig.mMimeType.EqualsLiteral("audio/mpeg")) {
    mFormatID = kAudioFormatMPEGLayer3;
  } else if (mConfig.mMimeType.EqualsLiteral("audio/mp4a-latm")) {
    mFormatID = kAudioFormatMPEG4AAC;
    if (aConfig.mCodecSpecificConfig.is<AacCodecSpecificData>()) {
      const AacCodecSpecificData& aacCodecSpecificData =
          aConfig.mCodecSpecificConfig.as<AacCodecSpecificData>();
      mEncoderDelay = aacCodecSpecificData.mEncoderDelayFrames;
      mTotalMediaFrames = aacCodecSpecificData.mMediaFrameCount;
      LOG("AppleATDecoder (aac), found encoder delay (%" PRIu32
          ") and total frame count (%" PRIu64 ") in codec-specific side data",
          mEncoderDelay, mTotalMediaFrames);
    }
  } else {
    mFormatID = 0;
  }
}

AppleATDecoder::~AppleATDecoder() {
  MOZ_COUNT_DTOR(AppleATDecoder);
  MOZ_ASSERT(!mConverter);
}

RefPtr<MediaDataDecoder::InitPromise> AppleATDecoder::Init() {
  if (!mFormatID) {
    return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Non recognised format")),
        __func__);
  }
  mThread = GetCurrentSerialEventTarget();

  return InitPromise::CreateAndResolve(TrackType::kAudioTrack, __func__);
}

RefPtr<MediaDataDecoder::FlushPromise> AppleATDecoder::Flush() {
  MOZ_ASSERT(mThread->IsOnCurrentThread());
  LOG("Flushing AudioToolbox AAC decoder");
  mQueuedSamples.Clear();
  mDecodedSamples.Clear();

  if (mConverter) {
    OSStatus rv = AudioConverterReset(mConverter);
    if (rv) {
      LOG("Error %d resetting AudioConverter", static_cast<int>(rv));
    }
  }
  if (mErrored) {
    mParsedFramesForAACMagicCookie = 0;
    mMagicCookie.Clear();
    ProcessShutdown();
    mErrored = false;
  }
  return FlushPromise::CreateAndResolve(true, __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> AppleATDecoder::Drain() {
  MOZ_ASSERT(mThread->IsOnCurrentThread());
  LOG("Draining AudioToolbox AAC decoder");
  return DecodePromise::CreateAndResolve(DecodedData(), __func__);
}

RefPtr<ShutdownPromise> AppleATDecoder::Shutdown() {
  // mThread may not be set if Init hasn't been called first.
  MOZ_ASSERT(!mThread || mThread->IsOnCurrentThread());
  ProcessShutdown();
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

void AppleATDecoder::ProcessShutdown() {
  // mThread may not be set if Init hasn't been called first.
  MOZ_ASSERT(!mThread || mThread->IsOnCurrentThread());

  if (mStream) {
    OSStatus rv = AudioFileStreamClose(mStream);
    if (rv) {
      LOG("error %d disposing of AudioFileStream", static_cast<int>(rv));
      return;
    }
    mStream = nullptr;
  }

  if (mConverter) {
    LOG("Shutdown: Apple AudioToolbox AAC decoder");
    OSStatus rv = AudioConverterDispose(mConverter);
    if (rv) {
      LOG("error %d disposing of AudioConverter", static_cast<int>(rv));
    }
    mConverter = nullptr;
  }
}

nsCString AppleATDecoder::GetCodecName() const {
  switch (mFormatID) {
    case kAudioFormatMPEGLayer3:
      return "mp3"_ns;
    case kAudioFormatMPEG4AAC:
      return "aac"_ns;
    default:
      return "unknown"_ns;
  }
}

struct PassthroughUserData {
  UInt32 mChannels;
  UInt32 mDataSize;
  const void* mData;
  AudioStreamPacketDescription mPacket;
};

// Error value we pass through the decoder to signal that nothing
// has gone wrong during decoding and we're done processing the packet.
const uint32_t kNoMoreDataErr = 'MOAR';

static OSStatus _PassthroughInputDataCallback(
    AudioConverterRef aAudioConverter, UInt32* aNumDataPackets /* in/out */,
    AudioBufferList* aData /* in/out */,
    AudioStreamPacketDescription** aPacketDesc, void* aUserData) {
  PassthroughUserData* userData = (PassthroughUserData*)aUserData;
  if (!userData->mDataSize) {
    *aNumDataPackets = 0;
    return kNoMoreDataErr;
  }

  if (aPacketDesc) {
    userData->mPacket.mStartOffset = 0;
    userData->mPacket.mVariableFramesInPacket = 0;
    userData->mPacket.mDataByteSize = userData->mDataSize;
    *aPacketDesc = &userData->mPacket;
  }

  aData->mBuffers[0].mNumberChannels = userData->mChannels;
  aData->mBuffers[0].mDataByteSize = userData->mDataSize;
  aData->mBuffers[0].mData = const_cast<void*>(userData->mData);

  // No more data to provide following this run.
  userData->mDataSize = 0;

  return noErr;
}

RefPtr<MediaDataDecoder::DecodePromise> AppleATDecoder::Decode(
    MediaRawData* aSample) {
  MOZ_ASSERT(mThread->IsOnCurrentThread());
  LOG("mp4 input sample pts=%s duration=%s %s %llu bytes audio",
      aSample->mTime.ToString().get(), aSample->GetEndTime().ToString().get(),
      aSample->mKeyframe ? " keyframe" : "",
      (unsigned long long)aSample->Size());

  MediaResult rv = NS_OK;
  if (!mConverter) {
    rv = SetupDecoder(aSample);
    if (rv != NS_OK && rv != NS_ERROR_NOT_INITIALIZED) {
      return DecodePromise::CreateAndReject(rv, __func__);
    }
  }

  mQueuedSamples.AppendElement(aSample);

  if (rv == NS_OK) {
    for (size_t i = 0; i < mQueuedSamples.Length(); i++) {
      rv = DecodeSample(mQueuedSamples[i]);
      if (NS_FAILED(rv)) {
        mErrored = true;
        return DecodePromise::CreateAndReject(rv, __func__);
      }
    }
    mQueuedSamples.Clear();
  }

  DecodedData results = std::move(mDecodedSamples);
  mDecodedSamples = DecodedData();
  return DecodePromise::CreateAndResolve(std::move(results), __func__);
}

MediaResult AppleATDecoder::DecodeSample(MediaRawData* aSample) {
  MOZ_ASSERT(mThread->IsOnCurrentThread());

  // Array containing the queued decoded audio frames, about to be output.
  nsTArray<AudioDataValue> outputData;
  UInt32 channels = mOutputFormat.mChannelsPerFrame;
  // Pick a multiple of the frame size close to a power of two
  // for efficient allocation. We're mainly using this decoder to decode AAC,
  // that has packets of 1024 audio frames.
  const uint32_t MAX_AUDIO_FRAMES = 1024;
  const uint32_t maxDecodedSamples = MAX_AUDIO_FRAMES * channels;

  // Descriptions for _decompressed_ audio packets. ignored.
  auto packets = MakeUnique<AudioStreamPacketDescription[]>(MAX_AUDIO_FRAMES);

  // This API insists on having packets spoon-fed to it from a callback.
  // This structure exists only to pass our state.
  PassthroughUserData userData = {channels, (UInt32)aSample->Size(),
                                  aSample->Data()};

  // Decompressed audio buffer
  AlignedAudioBuffer decoded(maxDecodedSamples);
  if (!decoded) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  do {
    AudioBufferList decBuffer;
    decBuffer.mNumberBuffers = 1;
    decBuffer.mBuffers[0].mNumberChannels = channels;
    decBuffer.mBuffers[0].mDataByteSize =
        maxDecodedSamples * sizeof(AudioDataValue);
    decBuffer.mBuffers[0].mData = decoded.get();

    // in: the max number of packets we can handle from the decoder.
    // out: the number of packets the decoder is actually returning.
    UInt32 numFrames = MAX_AUDIO_FRAMES;

    OSStatus rv = AudioConverterFillComplexBuffer(
        mConverter, _PassthroughInputDataCallback, &userData,
        &numFrames /* in/out */, &decBuffer, packets.get());

    if (rv && rv != kNoMoreDataErr) {
      LOG("Error decoding audio sample: %d\n", static_cast<int>(rv));
      return MediaResult(
          NS_ERROR_DOM_MEDIA_DECODE_ERR,
          RESULT_DETAIL("Error decoding audio sample: %d @ %s",
                        static_cast<int>(rv), aSample->mTime.ToString().get()));
    }

    if (numFrames) {
      AudioDataValue* outputFrames = decoded.get();
      outputData.AppendElements(outputFrames, numFrames * channels);
    }

    if (rv == kNoMoreDataErr) {
      break;
    }
  } while (true);

  if (outputData.IsEmpty()) {
    return NS_OK;
  }

  size_t numFrames = outputData.Length() / channels;
  int rate = mOutputFormat.mSampleRate;
  media::TimeUnit duration(numFrames, rate);
  if (!duration.IsValid()) {
    NS_WARNING("Invalid count of accumulated audio samples");
    return MediaResult(
        NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
        RESULT_DETAIL(
            "Invalid count of accumulated audio samples: num:%llu rate:%d",
            uint64_t(numFrames), rate));
  }

  LOG("Decoded audio packet [%s, %s] (duration: %s)\n",
      aSample->mTime.ToString().get(), aSample->GetEndTime().ToString().get(),
      duration.ToString().get());

  AudioSampleBuffer data(outputData.Elements(), outputData.Length());
  if (!data.Data()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (mChannelLayout && !mAudioConverter) {
    AudioConfig in(*mChannelLayout, channels, rate);
    AudioConfig out(AudioConfig::ChannelLayout::SMPTEDefault(*mChannelLayout),
                    channels, rate);
    mAudioConverter = MakeUnique<AudioConverter>(in, out);
  }
  if (mAudioConverter && mChannelLayout && mChannelLayout->IsValid()) {
    MOZ_ASSERT(mAudioConverter->CanWorkInPlace());
    data = mAudioConverter->Process(std::move(data));
  }

  RefPtr<AudioData> audio = new AudioData(
      aSample->mOffset, aSample->mTime, data.Forget(), channels, rate,
      mChannelLayout && mChannelLayout->IsValid()
          ? mChannelLayout->Map()
          : AudioConfig::ChannelLayout::UNKNOWN_MAP);
  MOZ_DIAGNOSTIC_ASSERT(duration == audio->mDuration, "must be equal");
  mDecodedSamples.AppendElement(std::move(audio));
  return NS_OK;
}

MediaResult AppleATDecoder::GetInputAudioDescription(
    AudioStreamBasicDescription& aDesc, const nsTArray<uint8_t>& aExtraData) {
  MOZ_ASSERT(mThread->IsOnCurrentThread());

  // Request the properties from CoreAudio using the codec magic cookie
  AudioFormatInfo formatInfo;
  PodZero(&formatInfo.mASBD);
  formatInfo.mASBD.mFormatID = mFormatID;
  if (mFormatID == kAudioFormatMPEG4AAC) {
    formatInfo.mASBD.mFormatFlags = mConfig.mExtendedProfile;
  }
  formatInfo.mMagicCookieSize = aExtraData.Length();
  formatInfo.mMagicCookie = aExtraData.Elements();

  UInt32 formatListSize;
  // Attempt to retrieve the default format using
  // kAudioFormatProperty_FormatInfo method.
  // This method only retrieves the FramesPerPacket information required
  // by the decoder, which depends on the codec type and profile.
  aDesc.mFormatID = mFormatID;
  aDesc.mChannelsPerFrame = mConfig.mChannels;
  aDesc.mSampleRate = mConfig.mRate;
  UInt32 inputFormatSize = sizeof(aDesc);
  OSStatus rv = AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, 0, NULL,
                                       &inputFormatSize, &aDesc);
  if (NS_WARN_IF(rv)) {
    return MediaResult(
        NS_ERROR_FAILURE,
        RESULT_DETAIL("Unable to get format info:%d", int32_t(rv)));
  }

  // If any of the methods below fail, we will return the default format as
  // created using kAudioFormatProperty_FormatInfo above.
  rv = AudioFormatGetPropertyInfo(kAudioFormatProperty_FormatList,
                                  sizeof(formatInfo), &formatInfo,
                                  &formatListSize);
  if (rv || (formatListSize % sizeof(AudioFormatListItem))) {
    return NS_OK;
  }
  size_t listCount = formatListSize / sizeof(AudioFormatListItem);
  auto formatList = MakeUnique<AudioFormatListItem[]>(listCount);

  rv = AudioFormatGetProperty(kAudioFormatProperty_FormatList,
                              sizeof(formatInfo), &formatInfo, &formatListSize,
                              formatList.get());
  if (rv) {
    return NS_OK;
  }
  LOG("found %zu available audio stream(s)",
      formatListSize / sizeof(AudioFormatListItem));
  // Get the index number of the first playable format.
  // This index number will be for the highest quality layer the platform
  // is capable of playing.
  UInt32 itemIndex;
  UInt32 indexSize = sizeof(itemIndex);
  rv = AudioFormatGetProperty(kAudioFormatProperty_FirstPlayableFormatFromList,
                              formatListSize, formatList.get(), &indexSize,
                              &itemIndex);
  if (rv) {
    return NS_OK;
  }

  aDesc = formatList[itemIndex].mASBD;

  return NS_OK;
}

AudioConfig::Channel ConvertChannelLabel(AudioChannelLabel id) {
  switch (id) {
    case kAudioChannelLabel_Left:
      return AudioConfig::CHANNEL_FRONT_LEFT;
    case kAudioChannelLabel_Right:
      return AudioConfig::CHANNEL_FRONT_RIGHT;
    case kAudioChannelLabel_Mono:
    case kAudioChannelLabel_Center:
      return AudioConfig::CHANNEL_FRONT_CENTER;
    case kAudioChannelLabel_LFEScreen:
      return AudioConfig::CHANNEL_LFE;
    case kAudioChannelLabel_LeftSurround:
      return AudioConfig::CHANNEL_SIDE_LEFT;
    case kAudioChannelLabel_RightSurround:
      return AudioConfig::CHANNEL_SIDE_RIGHT;
    case kAudioChannelLabel_CenterSurround:
      return AudioConfig::CHANNEL_BACK_CENTER;
    case kAudioChannelLabel_RearSurroundLeft:
      return AudioConfig::CHANNEL_BACK_LEFT;
    case kAudioChannelLabel_RearSurroundRight:
      return AudioConfig::CHANNEL_BACK_RIGHT;
    default:
      return AudioConfig::CHANNEL_INVALID;
  }
}

// Will set mChannelLayout if a channel layout could properly be identified
// and is supported.
nsresult AppleATDecoder::SetupChannelLayout() {
  MOZ_ASSERT(mThread->IsOnCurrentThread());

  // Determine the channel layout.
  UInt32 propertySize;
  UInt32 size;
  OSStatus status = AudioConverterGetPropertyInfo(
      mConverter, kAudioConverterOutputChannelLayout, &propertySize, NULL);
  if (status || !propertySize) {
    LOG("Couldn't get channel layout property (%s)", FourCC2Str(status));
    return NS_ERROR_FAILURE;
  }

  auto data = MakeUnique<uint8_t[]>(propertySize);
  size = propertySize;
  status = AudioConverterGetProperty(
      mConverter, kAudioConverterInputChannelLayout, &size, data.get());
  if (status || size != propertySize) {
    LOG("Couldn't get channel layout property (%s)", FourCC2Str(status));
    return NS_ERROR_FAILURE;
  }

  AudioChannelLayout* layout =
      reinterpret_cast<AudioChannelLayout*>(data.get());
  AudioChannelLayoutTag tag = layout->mChannelLayoutTag;

  // if tag is kAudioChannelLayoutTag_UseChannelDescriptions then the structure
  // directly contains the the channel layout mapping.
  // If tag is kAudioChannelLayoutTag_UseChannelBitmap then the layout will
  // be defined via the bitmap and can be retrieved using
  // kAudioFormatProperty_ChannelLayoutForBitmap property.
  // Otherwise the tag itself describes the layout.
  if (tag != kAudioChannelLayoutTag_UseChannelDescriptions) {
    AudioFormatPropertyID property =
        tag == kAudioChannelLayoutTag_UseChannelBitmap
            ? kAudioFormatProperty_ChannelLayoutForBitmap
            : kAudioFormatProperty_ChannelLayoutForTag;

    if (property == kAudioFormatProperty_ChannelLayoutForBitmap) {
      status = AudioFormatGetPropertyInfo(
          property, sizeof(UInt32), &layout->mChannelBitmap, &propertySize);
    } else {
      status = AudioFormatGetPropertyInfo(
          property, sizeof(AudioChannelLayoutTag), &tag, &propertySize);
    }
    if (status || !propertySize) {
      LOG("Couldn't get channel layout property info (%s:%s)",
          FourCC2Str(property), FourCC2Str(status));
      return NS_ERROR_FAILURE;
    }
    data = MakeUnique<uint8_t[]>(propertySize);
    layout = reinterpret_cast<AudioChannelLayout*>(data.get());
    size = propertySize;

    if (property == kAudioFormatProperty_ChannelLayoutForBitmap) {
      status = AudioFormatGetProperty(property, sizeof(UInt32),
                                      &layout->mChannelBitmap, &size, layout);
    } else {
      status = AudioFormatGetProperty(property, sizeof(AudioChannelLayoutTag),
                                      &tag, &size, layout);
    }
    if (status || size != propertySize) {
      LOG("Couldn't get channel layout property (%s:%s)", FourCC2Str(property),
          FourCC2Str(status));
      return NS_ERROR_FAILURE;
    }
    // We have retrieved the channel layout from the tag or bitmap.
    // We can now directly use the channel descriptions.
    layout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
  }

  if (layout->mNumberChannelDescriptions != mOutputFormat.mChannelsPerFrame) {
    LOG("Not matching the original channel number");
    return NS_ERROR_FAILURE;
  }

  AutoTArray<AudioConfig::Channel, 8> channels;
  channels.SetLength(layout->mNumberChannelDescriptions);
  for (uint32_t i = 0; i < layout->mNumberChannelDescriptions; i++) {
    AudioChannelLabel id = layout->mChannelDescriptions[i].mChannelLabel;
    AudioConfig::Channel channel = ConvertChannelLabel(id);
    channels[i] = channel;
  }
  mChannelLayout = MakeUnique<AudioConfig::ChannelLayout>(
      mOutputFormat.mChannelsPerFrame, channels.Elements());
  return NS_OK;
}

MediaResult AppleATDecoder::SetupDecoder(MediaRawData* aSample) {
  MOZ_ASSERT(mThread->IsOnCurrentThread());
  static const uint32_t MAX_FRAMES = 2;

  if (mFormatID == kAudioFormatMPEG4AAC && mConfig.mExtendedProfile == 2 &&
      mParsedFramesForAACMagicCookie < MAX_FRAMES) {
    // Check for implicit SBR signalling if stream is AAC-LC
    // This will provide us with an updated magic cookie for use with
    // GetInputAudioDescription.
    if (NS_SUCCEEDED(GetImplicitAACMagicCookie(aSample)) &&
        !mMagicCookie.Length()) {
      // nothing found yet, will try again later
      mParsedFramesForAACMagicCookie++;
      return NS_ERROR_NOT_INITIALIZED;
    }
    // An error occurred, fallback to using default stream description
  }

  LOG("Initializing Apple AudioToolbox decoder");

  // Should we try and use magic cookie data from the AAC data? We do this if
  // - We have an AAC config &
  // - We do not aleady have magic cookie data.
  // Otherwise we just use the existing cookie (which may be empty).
  bool shouldUseAacMagicCookie =
      mConfig.mCodecSpecificConfig.is<AacCodecSpecificData>() &&
      mMagicCookie.IsEmpty();

  nsTArray<uint8_t>& magicCookie =
      shouldUseAacMagicCookie
          ? *mConfig.mCodecSpecificConfig.as<AacCodecSpecificData>()
                 .mEsDescriptorBinaryBlob
          : mMagicCookie;
  AudioStreamBasicDescription inputFormat;
  PodZero(&inputFormat);

  MediaResult rv = GetInputAudioDescription(inputFormat, magicCookie);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // Fill in the output format manually.
  PodZero(&mOutputFormat);
  mOutputFormat.mFormatID = kAudioFormatLinearPCM;
  mOutputFormat.mSampleRate = inputFormat.mSampleRate;
  mOutputFormat.mChannelsPerFrame = inputFormat.mChannelsPerFrame;
  mOutputFormat.mBitsPerChannel = 32;
  mOutputFormat.mFormatFlags = kLinearPCMFormatFlagIsFloat | 0;
  // Set up the decoder so it gives us one sample per frame
  mOutputFormat.mFramesPerPacket = 1;
  mOutputFormat.mBytesPerPacket = mOutputFormat.mBytesPerFrame =
      mOutputFormat.mChannelsPerFrame * mOutputFormat.mBitsPerChannel / 8;

  OSStatus status =
      AudioConverterNew(&inputFormat, &mOutputFormat, &mConverter);
  if (status) {
    LOG("Error %d constructing AudioConverter", int(status));
    mConverter = nullptr;
    return MediaResult(
        NS_ERROR_FAILURE,
        RESULT_DETAIL("Error constructing AudioConverter:%d", int32_t(status)));
  }

  if (magicCookie.Length() && mFormatID == kAudioFormatMPEG4AAC) {
    status = AudioConverterSetProperty(
        mConverter, kAudioConverterDecompressionMagicCookie,
        magicCookie.Length(), magicCookie.Elements());
    if (status) {
      LOG("Error setting AudioConverter AAC cookie:%d", int32_t(status));
      ProcessShutdown();
      return MediaResult(
          NS_ERROR_FAILURE,
          RESULT_DETAIL("Error setting AudioConverter AAC cookie:%d",
                        int32_t(status)));
    }
  }

  if (NS_FAILED(SetupChannelLayout())) {
    NS_WARNING("Couldn't retrieve channel layout, will use default layout");
  }

  return NS_OK;
}

static void _MetadataCallback(void* aAppleATDecoder, AudioFileStreamID aStream,
                              AudioFileStreamPropertyID aProperty,
                              UInt32* aFlags) {
  AppleATDecoder* decoder = static_cast<AppleATDecoder*>(aAppleATDecoder);
  MOZ_RELEASE_ASSERT(decoder->mThread->IsOnCurrentThread());

  LOGEX(decoder, "MetadataCallback receiving: '%s'", FourCC2Str(aProperty));
  if (aProperty == kAudioFileStreamProperty_MagicCookieData) {
    UInt32 size;
    Boolean writeable;
    OSStatus rv =
        AudioFileStreamGetPropertyInfo(aStream, aProperty, &size, &writeable);
    if (rv) {
      LOGEX(decoder, "Couldn't get property info for '%s' (%s)",
            FourCC2Str(aProperty), FourCC2Str(rv));
      decoder->mFileStreamError = true;
      return;
    }
    auto data = MakeUnique<uint8_t[]>(size);
    rv = AudioFileStreamGetProperty(aStream, aProperty, &size, data.get());
    if (rv) {
      LOGEX(decoder, "Couldn't get property '%s' (%s)", FourCC2Str(aProperty),
            FourCC2Str(rv));
      decoder->mFileStreamError = true;
      return;
    }
    decoder->mMagicCookie.AppendElements(data.get(), size);
  }
}

static void _SampleCallback(void* aSBR, UInt32 aNumBytes, UInt32 aNumPackets,
                            const void* aData,
                            AudioStreamPacketDescription* aPackets) {}

nsresult AppleATDecoder::GetImplicitAACMagicCookie(
    const MediaRawData* aSample) {
  MOZ_ASSERT(mThread->IsOnCurrentThread());

  // Prepend ADTS header to AAC audio.
  RefPtr<MediaRawData> adtssample(aSample->Clone());
  if (!adtssample) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  int8_t frequency_index = Adts::GetFrequencyIndex(mConfig.mRate);

  bool rv = Adts::ConvertSample(mConfig.mChannels, frequency_index,
                                mConfig.mProfile, adtssample);
  if (!rv) {
    NS_WARNING("Failed to apply ADTS header");
    return NS_ERROR_FAILURE;
  }
  if (!mStream) {
    OSStatus rv = AudioFileStreamOpen(this, _MetadataCallback, _SampleCallback,
                                      kAudioFileAAC_ADTSType, &mStream);
    if (rv) {
      NS_WARNING("Couldn't open AudioFileStream");
      return NS_ERROR_FAILURE;
    }
  }

  OSStatus status = AudioFileStreamParseBytes(
      mStream, adtssample->Size(), adtssample->Data(), 0 /* discontinuity */);
  if (status) {
    NS_WARNING("Couldn't parse sample");
  }

  if (status || mFileStreamError || mMagicCookie.Length()) {
    // We have decoded a magic cookie or an error occurred as such
    // we won't need the stream any longer.
    AudioFileStreamClose(mStream);
    mStream = nullptr;
  }

  return (mFileStreamError || status) ? NS_ERROR_FAILURE : NS_OK;
}

}  // namespace mozilla

#undef LOG
#undef LOGEX
