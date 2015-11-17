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

extern mozilla::LogModule* GetPDMLog();
#define LOG(...) MOZ_LOG(GetPDMLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))
#define FourCC2Str(n) ((char[5]){(char)(n >> 24), (char)(n >> 16), (char)(n >> 8), (char)(n), 0})

namespace mozilla {

AppleATDecoder::AppleATDecoder(const AudioInfo& aConfig,
                               FlushableTaskQueue* aAudioTaskQueue,
                               MediaDataDecoderCallback* aCallback)
  : mConfig(aConfig)
  , mFileStreamError(false)
  , mTaskQueue(aAudioTaskQueue)
  , mCallback(aCallback)
  , mConverter(nullptr)
  , mStream(nullptr)
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
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
  }

  return InitPromise::CreateAndResolve(TrackType::kAudioTrack, __func__);
}

nsresult
AppleATDecoder::Input(MediaRawData* aSample)
{
  LOG("mp4 input sample %p %lld us %lld pts%s %llu bytes audio",
      aSample,
      aSample->mDuration,
      aSample->mTime,
      aSample->mKeyframe ? " keyframe" : "",
      (unsigned long long)aSample->Size());

  // Queue a task to perform the actual decoding on a separate thread.
  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableMethodWithArg<RefPtr<MediaRawData>>(
        this,
        &AppleATDecoder::SubmitSample,
        RefPtr<MediaRawData>(aSample));
  mTaskQueue->Dispatch(runnable.forget());

  return NS_OK;
}

nsresult
AppleATDecoder::Flush()
{
  LOG("Flushing AudioToolbox AAC decoder");
  mTaskQueue->Flush();
  mQueuedSamples.Clear();
  OSStatus rv = AudioConverterReset(mConverter);
  if (rv) {
    LOG("Error %d resetting AudioConverter", rv);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
AppleATDecoder::Drain()
{
  LOG("Draining AudioToolbox AAC decoder");
  mTaskQueue->AwaitIdle();
  mCallback->DrainComplete();
  return Flush();
}

nsresult
AppleATDecoder::Shutdown()
{
  LOG("Shutdown: Apple AudioToolbox AAC decoder");
  mQueuedSamples.Clear();
  OSStatus rv = AudioConverterDispose(mConverter);
  if (rv) {
    LOG("error %d disposing of AudioConverter", rv);
    return NS_ERROR_FAILURE;
  }
  mConverter = nullptr;

  if (mStream) {
    rv = AudioFileStreamClose(mStream);
    if (rv) {
      LOG("error %d disposing of AudioFileStream", rv);
      return NS_ERROR_FAILURE;
    }
    mStream = nullptr;
  }
  return NS_OK;
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

void
AppleATDecoder::SubmitSample(MediaRawData* aSample)
{
  nsresult rv = NS_OK;
  if (!mConverter) {
    rv = SetupDecoder(aSample);
    if (rv != NS_OK && rv != NS_ERROR_NOT_INITIALIZED) {
      mCallback->Error();
      return;
    }
  }

  mQueuedSamples.AppendElement(aSample);

  if (rv == NS_OK) {
    for (size_t i = 0; i < mQueuedSamples.Length(); i++) {
      if (NS_FAILED(DecodeSample(mQueuedSamples[i]))) {
        mQueuedSamples.Clear();
        mCallback->Error();
        return;
      }
    }
    mQueuedSamples.Clear();
  }

  if (mTaskQueue->IsEmpty()) {
    mCallback->InputExhausted();
  }
}

nsresult
AppleATDecoder::DecodeSample(MediaRawData* aSample)
{
  // Array containing the queued decoded audio frames, about to be output.
  nsTArray<AudioDataValue> outputData;
  UInt32 channels = mOutputFormat.mChannelsPerFrame;
  // Pick a multiple of the frame size close to a power of two
  // for efficient allocation.
  const uint32_t MAX_AUDIO_FRAMES = 128;
  const uint32_t maxDecodedSamples = MAX_AUDIO_FRAMES * channels;

  // Descriptions for _decompressed_ audio packets. ignored.
  nsAutoArrayPtr<AudioStreamPacketDescription>
    packets(new AudioStreamPacketDescription[MAX_AUDIO_FRAMES]);

  // This API insists on having packets spoon-fed to it from a callback.
  // This structure exists only to pass our state.
  PassthroughUserData userData =
    { channels, (UInt32)aSample->Size(), aSample->Data() };

  // Decompressed audio buffer
  nsAutoArrayPtr<AudioDataValue> decoded(new AudioDataValue[maxDecodedSamples]);

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
      LOG("Error decoding audio stream: %d\n", rv);
      return NS_ERROR_FAILURE;
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
    return NS_ERROR_FAILURE;
  }

#ifdef LOG_SAMPLE_DECODE
  LOG("pushed audio at time %lfs; duration %lfs\n",
      (double)aSample->mTime / USECS_PER_S,
      duration.ToSeconds());
#endif

  auto data = MakeUnique<AudioDataValue[]>(outputData.Length());
  PodCopy(data.get(), &outputData[0], outputData.Length());
  RefPtr<AudioData> audio = new AudioData(aSample->mOffset,
                                          aSample->mTime,
                                          duration.ToMicroseconds(),
                                          numFrames,
                                          Move(data),
                                          channels,
                                          rate);
  mCallback->Output(audio);
  return NS_OK;
}

nsresult
AppleATDecoder::GetInputAudioDescription(AudioStreamBasicDescription& aDesc,
                                         const nsTArray<uint8_t>& aExtraData)
{
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
    return NS_ERROR_FAILURE;
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
  nsAutoArrayPtr<AudioFormatListItem> formatList(
    new AudioFormatListItem[listCount]);

  rv = AudioFormatGetProperty(kAudioFormatProperty_FormatList,
                              sizeof(formatInfo),
                              &formatInfo,
                              &formatListSize,
                              formatList);
  if (rv) {
    return NS_OK;
  }
  LOG("found %u available audio stream(s)",
      formatListSize / sizeof(AudioFormatListItem));
  // Get the index number of the first playable format.
  // This index number will be for the highest quality layer the platform
  // is capable of playing.
  UInt32 itemIndex;
  UInt32 indexSize = sizeof(itemIndex);
  rv = AudioFormatGetProperty(kAudioFormatProperty_FirstPlayableFormatFromList,
                              formatListSize,
                              formatList,
                              &indexSize,
                              &itemIndex);
  if (rv) {
    return NS_OK;
  }

  aDesc = formatList[itemIndex].mASBD;

  return NS_OK;
}

nsresult
AppleATDecoder::SetupDecoder(MediaRawData* aSample)
{
  if (mFormatID == kAudioFormatMPEG4AAC &&
      mConfig.mExtendedProfile == 2) {
    // Check for implicit SBR signalling if stream is AAC-LC
    // This will provide us with an updated magic cookie for use with
    // GetInputAudioDescription.
    if (NS_SUCCEEDED(GetImplicitAACMagicCookie(aSample)) &&
        !mMagicCookie.Length()) {
      // nothing found yet, will try again later
      return NS_ERROR_NOT_INITIALIZED;
    }
    // An error occurred, fallback to using default stream description
  }

  LOG("Initializing Apple AudioToolbox decoder");

  AudioStreamBasicDescription inputFormat;
  PodZero(&inputFormat);
  nsresult rv =
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
    LOG("Error %d constructing AudioConverter", status);
    mConverter = nullptr;
    return NS_ERROR_FAILURE;
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
    nsAutoArrayPtr<uint8_t> data(new uint8_t[size]);
    rv = AudioFileStreamGetProperty(aStream, aProperty,
                                    &size, data);
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
