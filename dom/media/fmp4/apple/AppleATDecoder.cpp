/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <AudioToolbox/AudioToolbox.h>
#include "AppleUtils.h"
#include "MP4Reader.h"
#include "MP4Decoder.h"
#include "mozilla/RefPtr.h"
#include "mp4_demuxer/Adts.h"
#include "mp4_demuxer/DecoderData.h"
#include "nsIThread.h"
#include "AppleATDecoder.h"
#include "prlog.h"

#ifdef PR_LOGGING
PRLogModuleInfo* GetAppleMediaLog();
#define LOG(...) PR_LOG(GetAppleMediaLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

namespace mozilla {

AppleATDecoder::AppleATDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                               MediaTaskQueue* aAudioTaskQueue,
                               MediaDataDecoderCallback* aCallback)
  : mConfig(aConfig)
  , mTaskQueue(aAudioTaskQueue)
  , mCallback(aCallback)
  , mConverter(nullptr)
  , mStream(nullptr)
  , mCurrentAudioTimestamp(-1)
  , mNextAudioTimestamp(-1)
  , mSamplePosition(0)
  , mSizeDecoded(0)
  , mLastError(noErr)
{
  MOZ_COUNT_CTOR(AppleATDecoder);
  LOG("Creating Apple AudioToolbox decoder");
  LOG("Audio Decoder configuration: %s %d Hz %d channels %d bits per channel",
      mConfig.mime_type,
      mConfig.samples_per_second,
      mConfig.channel_count,
      mConfig.bits_per_sample);

  if (!strcmp(aConfig.mime_type, "audio/mpeg")) {
    mFileType = kAudioFileMP3Type;
  } else if (!strcmp(aConfig.mime_type, "audio/mp4a-latm")) {
    mFileType = kAudioFileAAC_ADTSType;
  } else {
    mFileType = 0;
  }
}

AppleATDecoder::~AppleATDecoder()
{
  MOZ_COUNT_DTOR(AppleATDecoder);
  MOZ_ASSERT(!mConverter);
  MOZ_ASSERT(!mStream);
}

static void
_MetadataCallback(void* aDecoder,
                  AudioFileStreamID aStream,
                  AudioFileStreamPropertyID aProperty,
                  UInt32* aFlags)
{
  LOG("AppleATDecoder metadata callback");
  AppleATDecoder* decoder = static_cast<AppleATDecoder*>(aDecoder);
  decoder->MetadataCallback(aStream, aProperty, aFlags);
}

static void
_SampleCallback(void* aDecoder,
                UInt32 aNumBytes,
                UInt32 aNumPackets,
                const void* aData,
                AudioStreamPacketDescription* aPackets)
{
  LOG("AppleATDecoder sample callback %u bytes %u packets",
      aNumBytes, aNumPackets);
  AppleATDecoder* decoder = static_cast<AppleATDecoder*>(aDecoder);
  decoder->SampleCallback(aNumBytes, aNumPackets, aData, aPackets);
}

nsresult
AppleATDecoder::Init()
{
  if (!mFileType) {
    NS_ERROR("Non recognised format");
    return NS_ERROR_FAILURE;
  }
  LOG("Initializing Apple AudioToolbox decoder");
  OSStatus rv = AudioFileStreamOpen(this,
                                    _MetadataCallback,
                                    _SampleCallback,
                                    mFileType,
                                    &mStream);
  if (rv) {
    NS_ERROR("Couldn't open AudioFileStream");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
AppleATDecoder::Input(mp4_demuxer::MP4Sample* aSample)
{
  LOG("mp4 input sample %p %lld us %lld pts%s %llu bytes audio",
      aSample,
      aSample->duration,
      aSample->composition_timestamp,
      aSample->is_sync_point ? " keyframe" : "",
      (unsigned long long)aSample->size);

  // Queue a task to perform the actual decoding on a separate thread.
  mTaskQueue->Dispatch(
      NS_NewRunnableMethodWithArg<nsAutoPtr<mp4_demuxer::MP4Sample>>(
        this,
        &AppleATDecoder::SubmitSample,
        nsAutoPtr<mp4_demuxer::MP4Sample>(aSample)));

  return NS_OK;
}

nsresult
AppleATDecoder::Flush()
{
  LOG("Flushing AudioToolbox AAC decoder");
  mTaskQueue->Flush();
  OSStatus rv = AudioConverterReset(mConverter);
  if (rv) {
    LOG("Error %d resetting AudioConverter", rv);
    return NS_ERROR_FAILURE;
  }
  // Notify our task queue of the coming input discontinuity.
  mTaskQueue->Dispatch(
      NS_NewRunnableMethod(this, &AppleATDecoder::SignalFlush));
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
  OSStatus rv1 = AudioConverterDispose(mConverter);
  if (rv1) {
    LOG("error %d disposing of AudioConverter", rv1);
  } else {
    mConverter = nullptr;
  }

  OSStatus rv2 = AudioFileStreamClose(mStream);
  if (rv2) {
    LOG("error %d closing AudioFileStream", rv2);
  } else {
    mStream = nullptr;
  }

  return (rv1 && rv2) ? NS_OK : NS_ERROR_FAILURE;
}

void
AppleATDecoder::MetadataCallback(AudioFileStreamID aFileStream,
                                 AudioFileStreamPropertyID aPropertyID,
                                 UInt32* aFlags)
{
  if (aPropertyID == kAudioFileStreamProperty_ReadyToProducePackets) {
    SetupDecoder();
  }
}

struct PassthroughUserData {
  AppleATDecoder* mDecoder;
  UInt32 mNumPackets;
  UInt32 mDataSize;
  const void* mData;
  AudioStreamPacketDescription* mPacketDesc;
  bool mDone;
};

// Error value we pass through the decoder to signal that nothing
// has gone wrong during decoding, but more data is needed.
const uint32_t kNeedMoreData = 'MOAR';

static OSStatus
_PassthroughInputDataCallback(AudioConverterRef aAudioConverter,
                              UInt32* aNumDataPackets /* in/out */,
                              AudioBufferList* aData /* in/out */,
                              AudioStreamPacketDescription** aPacketDesc,
                              void* aUserData)
{
  PassthroughUserData* userData = (PassthroughUserData*)aUserData;
  if (userData->mDone) {
    // We make sure this callback is run _once_, with all the data we received
    // from |AudioFileStreamParseBytes|. When we return an error, the decoder
    // simply passes the return value on to the calling method,
    // |SampleCallback|; and flushes all of the audio frames it had
    // buffered. It does not change the decoder's state.
    LOG("requested too much data; returning\n");
    *aNumDataPackets = 0;
    return kNeedMoreData;
  }

  userData->mDone = true;

  LOG("AudioConverter wants %u packets of audio data\n", *aNumDataPackets);

  *aNumDataPackets = userData->mNumPackets;
  *aPacketDesc = userData->mPacketDesc;

  aData->mBuffers[0].mNumberChannels = userData->mDecoder->mConfig.channel_count;
  aData->mBuffers[0].mDataByteSize = userData->mDataSize;
  aData->mBuffers[0].mData = const_cast<void*>(userData->mData);

  return noErr;
}

void
AppleATDecoder::SampleCallback(uint32_t aNumBytes,
                               uint32_t aNumPackets,
                               const void* aData,
                               AudioStreamPacketDescription* aPackets)
{
  // Pick a multiple of the frame size close to a power of two
  // for efficient allocation.
  const uint32_t MAX_AUDIO_FRAMES = 128;
  const uint32_t maxDecodedSamples = MAX_AUDIO_FRAMES * mConfig.channel_count;

  // Descriptions for _decompressed_ audio packets. ignored.
  nsAutoArrayPtr<AudioStreamPacketDescription>
      packets(new AudioStreamPacketDescription[MAX_AUDIO_FRAMES]);

  // This API insists on having packets spoon-fed to it from a callback.
  // This structure exists only to pass our state and the result of the
  // parser on to the callback above.
  PassthroughUserData userData =
      { this, aNumPackets, aNumBytes, aData, aPackets, false };

  // Decompressed audio buffer
  nsAutoArrayPtr<AudioDataValue> decoded(new AudioDataValue[maxDecodedSamples]);

  do {
    AudioBufferList decBuffer;
    decBuffer.mNumberBuffers = 1;
    decBuffer.mBuffers[0].mNumberChannels = mOutputFormat.mChannelsPerFrame;
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

    if (rv && rv != kNeedMoreData) {
      LOG("Error decoding audio stream: %d\n", rv);
      mLastError = rv;
      break;
    }

    mOutputData.AppendElements(decoded.get(),
                               numFrames * mConfig.channel_count);

    if (rv == kNeedMoreData) {
      // No error; we just need more data.
      LOG("FillComplexBuffer out of data\n");
      break;
    }
    LOG("%d frames decoded", numFrames);
  } while (true);

  mSizeDecoded += aNumBytes;
}

void
AppleATDecoder::SetupDecoder()
{
  LOG("Setting up Apple AudioToolbox decoder.");

  AudioStreamBasicDescription inputFormat;
  nsresult rv = AppleUtils::GetRichestDecodableFormat(mStream, inputFormat);
  if (NS_FAILED(rv)) {
    mCallback->Error();
    return;
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
#else
# error Unknown audio sample type
#endif
  // Set up the decoder so it gives us one sample per frame
  mOutputFormat.mFramesPerPacket = 1;
  mOutputFormat.mBytesPerPacket = mOutputFormat.mBytesPerFrame
        = mOutputFormat.mChannelsPerFrame * mOutputFormat.mBitsPerChannel / 8;

  OSStatus status =
    AudioConverterNew(&inputFormat, &mOutputFormat, &mConverter);
  if (status) {
    LOG("Error %d constructing AudioConverter", rv);
    mConverter = nullptr;
    mCallback->Error();
  }
}

void
AppleATDecoder::SubmitSample(nsAutoPtr<mp4_demuxer::MP4Sample> aSample)
{
  // Prepend ADTS header to AAC audio.
  if (!strcmp(mConfig.mime_type, "audio/mp4a-latm")) {
    bool rv = mp4_demuxer::Adts::ConvertSample(mConfig.channel_count,
                                               mConfig.frequency_index,
                                               mConfig.aac_profile,
                                               aSample);
    if (!rv) {
      NS_ERROR("Failed to apply ADTS header");
      mCallback->Error();
      return;
    }
  }

  const Microseconds fuzz = 5;
  CheckedInt<Microseconds> upperFuzz = mNextAudioTimestamp + fuzz;
  CheckedInt<Microseconds> lowerFuzz = mNextAudioTimestamp - fuzz;
  bool discontinuity =
    !mNextAudioTimestamp.isValid() || mNextAudioTimestamp.value() < 0 ||
    !upperFuzz.isValid() || lowerFuzz.value() < 0 ||
    upperFuzz.value() < aSample->composition_timestamp ||
    lowerFuzz.value() > aSample->composition_timestamp;

  if (discontinuity) {
    LOG("Discontinuity detected, expected %lld got %lld\n",
        mNextAudioTimestamp.value(), aSample->composition_timestamp);
    mCurrentAudioTimestamp = aSample->composition_timestamp;
    mSamplePosition = aSample->byte_offset;
  }

  uint32_t flags = discontinuity ? kAudioFileStreamParseFlag_Discontinuity : 0;

  OSStatus rv = AudioFileStreamParseBytes(mStream,
                                          aSample->size,
                                          aSample->data,
                                          flags);

  if (!mOutputData.IsEmpty()) {
    int rate = mOutputFormat.mSampleRate;
    int channels = mOutputFormat.mChannelsPerFrame;
    size_t numFrames = mOutputData.Length() / channels;
    CheckedInt<Microseconds> duration = FramesToUsecs(numFrames, rate);
    if (!duration.isValid()) {
      NS_ERROR("Invalid count of accumulated audio samples");
      mCallback->Error();
      return;
    }

    LOG("pushed audio at time %lfs; duration %lfs\n",
        (double)mCurrentAudioTimestamp.value() / USECS_PER_S,
        (double)duration.value() / USECS_PER_S);

    nsAutoArrayPtr<AudioDataValue>
      data(new AudioDataValue[mOutputData.Length()]);
    PodCopy(data.get(), &mOutputData[0], mOutputData.Length());
    mOutputData.Clear();
    AudioData* audio = new AudioData(mSamplePosition,
                                     mCurrentAudioTimestamp.value(),
                                     duration.value(),
                                     numFrames,
                                     data.forget(),
                                     channels,
                                     rate);
    mCallback->Output(audio);
    mCurrentAudioTimestamp += duration.value();
    if (!mCurrentAudioTimestamp.isValid()) {
      NS_ERROR("Invalid count of accumulated audio samples");
      mCallback->Error();
      return;
    }
    mSamplePosition += mSizeDecoded;
    mSizeDecoded = 0;
  }

  // This is the timestamp of the next sample we should be receiving
  mNextAudioTimestamp =
    CheckedInt<Microseconds>(aSample->composition_timestamp) + aSample->duration;

  if (rv != noErr) {
    LOG("Error %d parsing audio data", rv);
    mCallback->Error();
    return;
  }
  if (mLastError != noErr) {
    LOG("Error %d during decoding", mLastError);
    mCallback->Error();
    mLastError = noErr;
    return;
  }

  if (mTaskQueue->IsEmpty()) {
    mCallback->InputExhausted();
  }
}

void
AppleATDecoder::SignalFlush()
{
  mOutputData.Clear();
  mNextAudioTimestamp = -1;
  mSizeDecoded = 0;
}

} // namespace mozilla
