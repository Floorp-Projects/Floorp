/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WAVDecoder.h"
#include "AudioSampleFormat.h"
#include "nsAutoPtr.h"

using mp4_demuxer::ByteReader;

namespace mozilla {

WaveDataDecoder::WaveDataDecoder(const AudioInfo& aConfig,
                                 FlushableTaskQueue* aTaskQueue,
                                 MediaDataDecoderCallback* aCallback)
  : mInfo(aConfig)
  , mTaskQueue(aTaskQueue)
  , mCallback(aCallback)
  , mFrames(0)
{
}

nsresult
WaveDataDecoder::Shutdown()
{
  return NS_OK;
}

RefPtr<MediaDataDecoder::InitPromise>
WaveDataDecoder::Init()
{
  return InitPromise::CreateAndResolve(TrackInfo::kAudioTrack, __func__);
}

nsresult
WaveDataDecoder::Input(MediaRawData* aSample)
{
  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableMethodWithArg<RefPtr<MediaRawData>>(
      this, &WaveDataDecoder::Decode,
      RefPtr<MediaRawData>(aSample)));
  mTaskQueue->Dispatch(runnable.forget());

  return NS_OK;
}

void
WaveDataDecoder::Decode(MediaRawData* aSample)
{
  if (!DoDecode(aSample)) {
    mCallback->Error();
  } else if (mTaskQueue->IsEmpty()) {
    mCallback->InputExhausted();
  }
}

bool
WaveDataDecoder::DoDecode(MediaRawData* aSample)
{
  size_t aLength = aSample->Size();
  ByteReader aReader = ByteReader(aSample->Data(), aLength);
  int64_t aOffset = aSample->mOffset;
  uint64_t aTstampUsecs = aSample->mTime;

  int32_t frames = aLength * 8 / mInfo.mBitDepth / mInfo.mChannels;

  auto buffer = MakeUnique<AudioDataValue[]>(frames * mInfo.mChannels);
  for (int i = 0; i < frames; ++i) {
    for (unsigned int j = 0; j < mInfo.mChannels; ++j) {
      if (mInfo.mBitDepth == 8) {
        uint8_t v = aReader.ReadU8();
        buffer[i * mInfo.mChannels + j] =
            UInt8bitToAudioSample<AudioDataValue>(v);
      } else if (mInfo.mBitDepth == 16) {
        int16_t v = aReader.ReadLE16();
        buffer[i * mInfo.mChannels + j] =
            IntegerToAudioSample<AudioDataValue>(v);
      } else if (mInfo.mBitDepth == 24) {
        int32_t v = aReader.ReadLE24();
        buffer[i * mInfo.mChannels + j] =
            Int24bitToAudioSample<AudioDataValue>(v);
      }
    }
  }

  aReader.DiscardRemaining();

  int64_t duration = frames / mInfo.mRate;

  mCallback->Output(new AudioData(aOffset,
                                  aTstampUsecs,
                                  duration,
                                  frames,
                                  Move(buffer),
                                  mInfo.mChannels,
                                  mInfo.mRate));
  mFrames += frames;

  return true;
}

void
WaveDataDecoder::DoDrain()
{
  mCallback->DrainComplete();
}

nsresult
WaveDataDecoder::Drain()
{
  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableMethod(this, &WaveDataDecoder::DoDrain));
  mTaskQueue->Dispatch(runnable.forget());
  return NS_OK;
}

nsresult
WaveDataDecoder::Flush()
{
  mTaskQueue->Flush();
  mFrames = 0;
  return NS_OK;
}

/* static */
bool
WaveDataDecoder::IsWave(const nsACString& aMimeType)
{
  // Some WebAudio uses "audio/x-wav",
  // WAVdemuxer uses "audio/wave; codecs=aNum".
  return aMimeType.EqualsLiteral("audio/x-wav") ||
         aMimeType.EqualsLiteral("audio/wave; codecs=1") ||
         aMimeType.EqualsLiteral("audio/wave; codecs=65534");
}

} // namespace mozilla
#undef LOG