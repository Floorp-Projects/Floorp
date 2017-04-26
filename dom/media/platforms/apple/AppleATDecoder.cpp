/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleUtils.h"
#include "MP4Decoder.h"
#include "mp4_demuxer/Adts.h"
#include "MediaInfo.h"
#include "AppleATDecoder.h"
#include "mozilla/Logging.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/UniquePtr.h"

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#define FourCC2Str(n) ((char[5]){(char)(n >> 24), (char)(n >> 16), (char)(n >> 8), (char)(n), 0})

namespace mozilla {

AppleATDecoder::AppleATDecoder(const AudioInfo& aConfig,
                               TaskQueue* aTaskQueue)
  : mConfig(aConfig)
  , mFileStreamError(false)
  , mTaskQueue(aTaskQueue)
  , mConverter(nullptr)
  , mStream(nullptr)
  , mParsedFramesForAACMagicCookie(0)
  , mErrored(false)
{
  MOZ_COUNT_CTOR(AppleATDecoder);
  LOG("Creating Apple AudioToolbox decoder");
  LOG("Audio Decoder configuration: %s %d Hz %d channels %d bits per channel",
      mConfig.mMimeType.get(),
      mConfig.mRate,
      mConfig.mChannels,
      mConfig.mBitDepth);

  if (mConfig.mMimeType.EqualsLiteral("audio/mpeg")) {
    mFormatID = kAudioFormatMPEGLayer3;
  } else if (mConfig.mMimeType.EqualsLiteral("audio/mp4a-latm")) {
    mFormatID = kAudioFormatMPEG4AAC;
  } else {
    mFormatID = 0;
  }
}

AppleATDecoder::~AppleATDecoder()
{
  MOZ_COUNT_DTOR(AppleATDecoder);
  MOZ_ASSERT(!mConverter);
}

RefPtr<MediaDataDecoder::InitPromise>
AppleATDecoder::Init()
{
  if (!mFormatID) {
    NS_ERROR("Non recognised format");
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }

  return InitPromise::CreateAndResolve(TrackType::kAudioTrack, __func__);
}

RefPtr<MediaDataDecoder::DecodePromise>
AppleATDecoder::Decode(MediaRawData* aSample)
{
  LOG("mp4 input sample %p %lld us %lld pts%s %llu bytes audio", aSample,
      aSample->mDuration.ToMicroseconds(), aSample->mTime.ToMicroseconds(),
      aSample->mKeyframe ? " keyframe" : "",
      (unsigned long long)aSample->Size());
  RefPtr<AppleATDecoder> self = this;
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(mTaskQueue, __func__, [self, this, sample] {
    return ProcessDecode(sample);
  });
}

RefPtr<MediaDataDecoder::FlushPromise>
AppleATDecoder::ProcessFlush()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
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

RefPtr<MediaDataDecoder::FlushPromise>
AppleATDecoder::Flush()
{
  LOG("Flushing AudioToolbox AAC decoder");
  return InvokeAsync(mTaskQueue, this, __func__, &AppleATDecoder::ProcessFlush);
}

RefPtr<MediaDataDecoder::DecodePromise>
AppleATDecoder::Drain()
{
  LOG("Draining AudioToolbox AAC decoder");
  RefPtr<AppleATDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [] {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  });
}

RefPtr<ShutdownPromise>
AppleATDecoder::Shutdown()
{
  RefPtr<AppleATDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
    ProcessShutdown();
    return ShutdownPromise::CreateAndResolve(true, __func__);
  });
}

void
AppleATDecoder::ProcessShutdown()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

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

struct PassthroughUserData {
  UInt32 mChannels;
  UInt32 mDataSize;
  const void* mData;
  AudioStreamPacketDescription mPacket;
};

// Error value we pass through the decoder to signal that nothing
// has gone wrong during decoding and we're done processing the packet.
const uint32_t kNoMoreDataErr = 'MOAR';

static OSStatus
_PassthroughInputDataCallback(AudioConverterRef aAudioConverter,
                              UInt32* aNumDataPackets /* in/out */,
                              AudioBufferList* aData /* in/out */,
                              AudioStreamPacketDescription** aPacketDesc,
                              void* aUserData)
{
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

RefPtr<MediaDataDecoder::DecodePromise>
AppleATDecoder::ProcessDecode(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

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

  return DecodePromise::CreateAndResolve(Move(mDecodedSamples), __func__);
}

MediaResult
AppleATDecoder::DecodeSample(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  // Array containing the queued decoded audio frames, about to be output.
  nsTArray<AudioDataValue> outputData;
  UInt32 channels = mOutputFormat.mChannelsPerFrame;
  // Pick a multiple of the frame size close to a power of two
  // for efficient allocation.
  const uint32_t MAX_AUDIO_FRAMES = 128;
  const uint32_t maxDecodedSamples = MAX_AUDIO_FRAMES * channels;

  // Descriptions for _decompressed_ audio packets. ignored.
  auto packets = MakeUnique<AudioStreamPacketDescription[]>(MAX_AUDIO_FRAMES);

  // This API insists on having packets spoon-fed to it from a callback.
  // This structure exists only to pass our state.
  PassthroughUserData userData =
    { channels, (UInt32)aSample->Size(), aSample->Data() };

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

    OSStatus rv = AudioConverterFillComplexBuffer(mConverter,
                                                  _PassthroughInputDataCallback,
                                                  &userData,
                                                  &numFrames /* in/out */,
                                                  &decBuffer,
                                                  packets.get());

    if (rv && rv != kNoMoreDataErr) {
      LOG("Error decoding audio sample: %d\n", static_cast<int>(rv));
      return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                         RESULT_DETAIL("Error decoding audio sample: %d @ %lld",
                                       static_cast<int>(rv), aSample->mTime.ToMicroseconds()));
    }

    if (numFrames) {
      outputData.AppendElements(decoded.get(), numFrames * channels);
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
  media::TimeUnit duration = FramesToTimeUnit(numFrames, rate);
  if (!duration.IsValid()) {
    NS_WARNING("Invalid count of accumulated audio samples");
    return MediaResult(
      NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
      RESULT_DETAIL(
        "Invalid count of accumulated audio samples: num:%llu rate:%d",
        uint64_t(numFrames), rate));
  }

#ifdef LOG_SAMPLE_DECODE
  LOG("pushed audio at time %lfs; duration %lfs\n",
      (double)aSample->mTime / USECS_PER_S,
      duration.ToSeconds());
#endif

  AudioSampleBuffer data(outputData.Elements(), outputData.Length());
  if (!data.Data()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (mChannelLayout && !mAudioConverter) {
    AudioConfig in(*mChannelLayout.get(), rate);
    AudioConfig out(channels, rate);
    if (!in.IsValid() || !out.IsValid()) {
      return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                         RESULT_DETAIL("Invalid audio config"));
    }
    mAudioConverter = MakeUnique<AudioConverter>(in, out);
  }
  if (mAudioConverter) {
    MOZ_ASSERT(mAudioConverter->CanWorkInPlace());
    data = mAudioConverter->Process(Move(data));
  }

  RefPtr<AudioData> audio = new AudioData(aSample->mOffset,
                                          aSample->mTime,
                                          duration,
                                          numFrames,
                                          data.Forget(),
                                          channels,
                                          rate);
  mDecodedSamples.AppendElement(Move(audio));
  return NS_OK;
}

MediaResult
AppleATDecoder::GetInputAudioDescription(AudioStreamBasicDescription& aDesc,
                                         const nsTArray<uint8_t>& aExtraData)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

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
  OSStatus rv = AudioFormatGetProperty(kAudioFormatProperty_FormatInfo,
                                       0,
                                       NULL,
                                       &inputFormatSize,
                                       &aDesc);
  if (NS_WARN_IF(rv)) {
    return MediaResult(
      NS_ERROR_FAILURE,
      RESULT_DETAIL("Unable to get format info:%lld", int64_t(rv)));
  }

  // If any of the methods below fail, we will return the default format as
  // created using kAudioFormatProperty_FormatInfo above.
  rv = AudioFormatGetPropertyInfo(kAudioFormatProperty_FormatList,
                                  sizeof(formatInfo),
                                  &formatInfo,
                                  &formatListSize);
  if (rv || (formatListSize % sizeof(AudioFormatListItem))) {
    return NS_OK;
  }
  size_t listCount = formatListSize / sizeof(AudioFormatListItem);
  auto formatList = MakeUnique<AudioFormatListItem[]>(listCount);

  rv = AudioFormatGetProperty(kAudioFormatProperty_FormatList,
                              sizeof(formatInfo),
                              &formatInfo,
                              &formatListSize,
                              formatList.get());
  if (rv) {
    return NS_OK;
  }
  LOG("found %" PRIuSIZE " available audio stream(s)",
      formatListSize / sizeof(AudioFormatListItem));
  // Get the index number of the first playable format.
  // This index number will be for the highest quality layer the platform
  // is capable of playing.
  UInt32 itemIndex;
  UInt32 indexSize = sizeof(itemIndex);
  rv = AudioFormatGetProperty(kAudioFormatProperty_FirstPlayableFormatFromList,
                              formatListSize,
                              formatList.get(),
                              &indexSize,
                              &itemIndex);
  if (rv) {
    return NS_OK;
  }

  aDesc = formatList[itemIndex].mASBD;

  return NS_OK;
}

AudioConfig::Channel
ConvertChannelLabel(AudioChannelLabel id)
{
  switch (id) {
    case kAudioChannelLabel_Mono:
      return AudioConfig::CHANNEL_MONO;
    case kAudioChannelLabel_Left:
      return AudioConfig::CHANNEL_LEFT;
    case kAudioChannelLabel_Right:
      return AudioConfig::CHANNEL_RIGHT;
    case kAudioChannelLabel_Center:
      return AudioConfig::CHANNEL_CENTER;
    case kAudioChannelLabel_LFEScreen:
      return AudioConfig::CHANNEL_LFE;
    case kAudioChannelLabel_LeftSurround:
      return AudioConfig::CHANNEL_LS;
    case kAudioChannelLabel_RightSurround:
      return AudioConfig::CHANNEL_RS;
    case kAudioChannelLabel_CenterSurround:
      return AudioConfig::CHANNEL_RCENTER;
    case kAudioChannelLabel_RearSurroundLeft:
      return AudioConfig::CHANNEL_RLS;
    case kAudioChannelLabel_RearSurroundRight:
      return AudioConfig::CHANNEL_RRS;
    default:
      return AudioConfig::CHANNEL_INVALID;
  }
}

// Will set mChannelLayout if a channel layout could properly be identified
// and is supported.
nsresult
AppleATDecoder::SetupChannelLayout()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  // Determine the channel layout.
  UInt32 propertySize;
  UInt32 size;
  OSStatus status =
    AudioConverterGetPropertyInfo(mConverter,
                                  kAudioConverterOutputChannelLayout,
                                  &propertySize, NULL);
  if (status || !propertySize) {
    LOG("Couldn't get channel layout property (%s)", FourCC2Str(status));
    return NS_ERROR_FAILURE;
  }

  auto data = MakeUnique<uint8_t[]>(propertySize);
  size = propertySize;
  status =
    AudioConverterGetProperty(mConverter, kAudioConverterInputChannelLayout,
                              &size, data.get());
  if (status || size != propertySize) {
    LOG("Couldn't get channel layout property (%s)",
        FourCC2Str(status));
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
      status =
        AudioFormatGetPropertyInfo(property,
                                   sizeof(UInt32), &layout->mChannelBitmap,
                                   &propertySize);
    } else {
      status =
        AudioFormatGetPropertyInfo(property,
                                   sizeof(AudioChannelLayoutTag), &tag,
                                   &propertySize);
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
      status = AudioFormatGetProperty(property,
                                      sizeof(UInt32), &layout->mChannelBitmap,
                                      &size, layout);
    } else {
      status = AudioFormatGetProperty(property,
                                      sizeof(AudioChannelLayoutTag), &tag,
                                      &size, layout);
    }
    if (status || size != propertySize) {
      LOG("Couldn't get channel layout property (%s:%s)",
          FourCC2Str(property), FourCC2Str(status));
      return NS_ERROR_FAILURE;
    }
    // We have retrieved the channel layout from the tag or bitmap.
    // We can now directly use the channel descriptions.
    layout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
  }

  if (layout->mNumberChannelDescriptions > MAX_AUDIO_CHANNELS ||
      layout->mNumberChannelDescriptions != mOutputFormat.mChannelsPerFrame) {
    LOG("Nonsensical channel layout or not matching the original channel number");
    return NS_ERROR_FAILURE;
  }

  AudioConfig::Channel channels[MAX_AUDIO_CHANNELS];
  for (uint32_t i = 0; i < layout->mNumberChannelDescriptions; i++) {
    AudioChannelLabel id = layout->mChannelDescriptions[i].mChannelLabel;
    AudioConfig::Channel channel = ConvertChannelLabel(id);
    channels[i] = channel;
  }
  mChannelLayout =
    MakeUnique<AudioConfig::ChannelLayout>(mOutputFormat.mChannelsPerFrame,
                                           channels);
  return NS_OK;
}

MediaResult
AppleATDecoder::SetupDecoder(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  static const uint32_t MAX_FRAMES = 2;

  if (mFormatID == kAudioFormatMPEG4AAC &&
      mConfig.mExtendedProfile == 2 &&
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

  AudioStreamBasicDescription inputFormat;
  PodZero(&inputFormat);
  MediaResult rv =
    GetInputAudioDescription(inputFormat,
                             mMagicCookie.Length() ?
                                 mMagicCookie : *mConfig.mExtraData);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // Fill in the output format manually.
  PodZero(&mOutputFormat);
  mOutputFormat.mFormatID = kAudioFormatLinearPCM;
  mOutputFormat.mSampleRate = inputFormat.mSampleRate;
  mOutputFormat.mChannelsPerFrame = inputFormat.mChannelsPerFrame;
#if defined(MOZ_SAMPLE_TYPE_FLOAT32)
  mOutputFormat.mBitsPerChannel = 32;
  mOutputFormat.mFormatFlags =
    kLinearPCMFormatFlagIsFloat |
    0;
#elif defined(MOZ_SAMPLE_TYPE_S16)
  mOutputFormat.mBitsPerChannel = 16;
  mOutputFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | 0;
#else
# error Unknown audio sample type
#endif
  // Set up the decoder so it gives us one sample per frame
  mOutputFormat.mFramesPerPacket = 1;
  mOutputFormat.mBytesPerPacket = mOutputFormat.mBytesPerFrame
        = mOutputFormat.mChannelsPerFrame * mOutputFormat.mBitsPerChannel / 8;

  OSStatus status = AudioConverterNew(&inputFormat, &mOutputFormat, &mConverter);
  if (status) {
    LOG("Error %d constructing AudioConverter", static_cast<int>(status));
    mConverter = nullptr;
    return MediaResult(
      NS_ERROR_FAILURE,
      RESULT_DETAIL("Error constructing AudioConverter:%lld", int64_t(status)));
  }

  if (NS_FAILED(SetupChannelLayout())) {
    NS_WARNING("Couldn't retrieve channel layout, will use default layout");
  }

  return NS_OK;
}

static void
_MetadataCallback(void* aAppleATDecoder,
                  AudioFileStreamID aStream,
                  AudioFileStreamPropertyID aProperty,
                  UInt32* aFlags)
{
  AppleATDecoder* decoder = static_cast<AppleATDecoder*>(aAppleATDecoder);
  MOZ_RELEASE_ASSERT(decoder->mTaskQueue->IsCurrentThreadIn());

  LOG("MetadataCallback receiving: '%s'", FourCC2Str(aProperty));
  if (aProperty == kAudioFileStreamProperty_MagicCookieData) {
    UInt32 size;
    Boolean writeable;
    OSStatus rv = AudioFileStreamGetPropertyInfo(aStream,
                                                 aProperty,
                                                 &size,
                                                 &writeable);
    if (rv) {
      LOG("Couldn't get property info for '%s' (%s)",
          FourCC2Str(aProperty), FourCC2Str(rv));
      decoder->mFileStreamError = true;
      return;
    }
    auto data = MakeUnique<uint8_t[]>(size);
    rv = AudioFileStreamGetProperty(aStream, aProperty,
                                    &size, data.get());
    if (rv) {
      LOG("Couldn't get property '%s' (%s)",
          FourCC2Str(aProperty), FourCC2Str(rv));
      decoder->mFileStreamError = true;
      return;
    }
    decoder->mMagicCookie.AppendElements(data.get(), size);
  }
}

static void
_SampleCallback(void* aSBR,
                UInt32 aNumBytes,
                UInt32 aNumPackets,
                const void* aData,
                AudioStreamPacketDescription* aPackets)
{
}

nsresult
AppleATDecoder::GetImplicitAACMagicCookie(const MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  // Prepend ADTS header to AAC audio.
  RefPtr<MediaRawData> adtssample(aSample->Clone());
  if (!adtssample) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  int8_t frequency_index =
    mp4_demuxer::Adts::GetFrequencyIndex(mConfig.mRate);

  bool rv = mp4_demuxer::Adts::ConvertSample(mConfig.mChannels,
                                             frequency_index,
                                             mConfig.mProfile,
                                             adtssample);
  if (!rv) {
    NS_WARNING("Failed to apply ADTS header");
    return NS_ERROR_FAILURE;
  }
  if (!mStream) {
    OSStatus rv = AudioFileStreamOpen(this,
                                      _MetadataCallback,
                                      _SampleCallback,
                                      kAudioFileAAC_ADTSType,
                                      &mStream);
    if (rv) {
      NS_WARNING("Couldn't open AudioFileStream");
      return NS_ERROR_FAILURE;
    }
  }

  OSStatus status = AudioFileStreamParseBytes(mStream,
                                              adtssample->Size(),
                                              adtssample->Data(),
                                              0 /* discontinuity */);
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

} // namespace mozilla
