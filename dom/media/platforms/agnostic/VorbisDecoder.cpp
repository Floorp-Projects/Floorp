/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VorbisDecoder.h"
#include "VorbisUtils.h"
#include "XiphExtradata.h"

#include "mozilla/PodOperations.h"
#include "nsAutoPtr.h"

#undef LOG
extern mozilla::LogModule* GetPDMLog();
#define LOG(type, msg) MOZ_LOG(GetPDMLog(), type, msg)

namespace mozilla {

ogg_packet InitVorbisPacket(const unsigned char* aData, size_t aLength,
                         bool aBOS, bool aEOS,
                         int64_t aGranulepos, int64_t aPacketNo)
{
  ogg_packet packet;
  packet.packet = const_cast<unsigned char*>(aData);
  packet.bytes = aLength;
  packet.b_o_s = aBOS;
  packet.e_o_s = aEOS;
  packet.granulepos = aGranulepos;
  packet.packetno = aPacketNo;
  return packet;
}

VorbisDataDecoder::VorbisDataDecoder(const AudioInfo& aConfig,
                                     FlushableTaskQueue* aTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
  : mInfo(aConfig)
  , mTaskQueue(aTaskQueue)
  , mCallback(aCallback)
  , mPacketCount(0)
  , mFrames(0)
{
  // Zero these member vars to avoid crashes in Vorbis clear functions when
  // destructor is called before |Init|.
  PodZero(&mVorbisBlock);
  PodZero(&mVorbisDsp);
  PodZero(&mVorbisInfo);
  PodZero(&mVorbisComment);
}

VorbisDataDecoder::~VorbisDataDecoder()
{
  vorbis_block_clear(&mVorbisBlock);
  vorbis_dsp_clear(&mVorbisDsp);
  vorbis_info_clear(&mVorbisInfo);
  vorbis_comment_clear(&mVorbisComment);
}

nsresult
VorbisDataDecoder::Shutdown()
{
  //mReader = nullptr;
  return NS_OK;
}

RefPtr<MediaDataDecoder::InitPromise>
VorbisDataDecoder::Init()
{
  vorbis_info_init(&mVorbisInfo);
  vorbis_comment_init(&mVorbisComment);
  PodZero(&mVorbisDsp);
  PodZero(&mVorbisBlock);

  nsAutoTArray<unsigned char*,4> headers;
  nsAutoTArray<size_t,4> headerLens;
  if (!XiphExtradataToHeaders(headers, headerLens,
                              mInfo.mCodecSpecificConfig->Elements(),
                              mInfo.mCodecSpecificConfig->Length())) {
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
  }
  for (size_t i = 0; i < headers.Length(); i++) {
    if (NS_FAILED(DecodeHeader(headers[i], headerLens[i]))) {
      return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
    }
  }

  MOZ_ASSERT(mPacketCount == 3);

  int r = vorbis_synthesis_init(&mVorbisDsp, &mVorbisInfo);
  if (r) {
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
  }

  r = vorbis_block_init(&mVorbisDsp, &mVorbisBlock);
  if (r) {
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
  }

  if (mInfo.mRate != (uint32_t)mVorbisDsp.vi->rate) {
    LOG(LogLevel::Warning,
        ("Invalid Vorbis header: container and codec rate do not match!"));
  }
  if (mInfo.mChannels != (uint32_t)mVorbisDsp.vi->channels) {
    LOG(LogLevel::Warning,
        ("Invalid Vorbis header: container and codec channels do not match!"));
  }

  return InitPromise::CreateAndResolve(TrackInfo::kAudioTrack, __func__);
}

nsresult
VorbisDataDecoder::DecodeHeader(const unsigned char* aData, size_t aLength)
{
  bool bos = mPacketCount == 0;
  ogg_packet pkt = InitVorbisPacket(aData, aLength, bos, false, 0, mPacketCount++);
  MOZ_ASSERT(mPacketCount <= 3);

  int r = vorbis_synthesis_headerin(&mVorbisInfo,
                                    &mVorbisComment,
                                    &pkt);
  return r == 0 ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
VorbisDataDecoder::Input(MediaRawData* aSample)
{
  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableMethodWithArg<RefPtr<MediaRawData>>(
      this, &VorbisDataDecoder::Decode,
      RefPtr<MediaRawData>(aSample)));
  mTaskQueue->Dispatch(runnable.forget());

  return NS_OK;
}

void
VorbisDataDecoder::Decode(MediaRawData* aSample)
{
  if (DoDecode(aSample) == -1) {
    mCallback->Error();
  } else if (mTaskQueue->IsEmpty()) {
    mCallback->InputExhausted();
  }
}

int
VorbisDataDecoder::DoDecode(MediaRawData* aSample)
{
  const unsigned char* aData = aSample->Data();
  size_t aLength = aSample->Size();
  int64_t aOffset = aSample->mOffset;
  uint64_t aTstampUsecs = aSample->mTime;
  int64_t aTotalFrames = 0;

  MOZ_ASSERT(mPacketCount >= 3);

  ogg_packet pkt = InitVorbisPacket(aData, aLength, false, false, -1, mPacketCount++);
  bool first_packet = mPacketCount == 4;

  if (vorbis_synthesis(&mVorbisBlock, &pkt) != 0) {
    return -1;
  }

  if (vorbis_synthesis_blockin(&mVorbisDsp,
                               &mVorbisBlock) != 0) {
    return -1;
  }

  VorbisPCMValue** pcm = 0;
  int32_t frames = vorbis_synthesis_pcmout(&mVorbisDsp, &pcm);
  // If the first packet of audio in the media produces no data, we
  // still need to produce an AudioData for it so that the correct media
  // start time is calculated.  Otherwise we'd end up with a media start
  // time derived from the timecode of the first packet that produced
  // data.
  if (frames == 0 && first_packet) {
    mCallback->Output(new AudioData(aOffset,
                                    aTstampUsecs,
                                    0,
                                    0,
                                    nullptr,
                                    mVorbisDsp.vi->channels,
                                    mVorbisDsp.vi->rate));
  }
  while (frames > 0) {
    uint32_t channels = mVorbisDsp.vi->channels;
    auto buffer = MakeUnique<AudioDataValue[]>(frames*channels);
    for (uint32_t j = 0; j < channels; ++j) {
      VorbisPCMValue* channel = pcm[j];
      for (uint32_t i = 0; i < uint32_t(frames); ++i) {
        buffer[i*channels + j] = MOZ_CONVERT_VORBIS_SAMPLE(channel[i]);
      }
    }

    CheckedInt64 duration = FramesToUsecs(frames, mVorbisDsp.vi->rate);
    if (!duration.isValid()) {
      NS_WARNING("Int overflow converting WebM audio duration");
      return -1;
    }
    CheckedInt64 total_duration = FramesToUsecs(aTotalFrames,
                                                mVorbisDsp.vi->rate);
    if (!total_duration.isValid()) {
      NS_WARNING("Int overflow converting WebM audio total_duration");
      return -1;
    }

    CheckedInt64 time = total_duration + aTstampUsecs;
    if (!time.isValid()) {
      NS_WARNING("Int overflow adding total_duration and aTstampUsecs");
      return -1;
    };

    aTotalFrames += frames;
    mCallback->Output(new AudioData(aOffset,
                                    time.value(),
                                    duration.value(),
                                    frames,
                                    Move(buffer),
                                    mVorbisDsp.vi->channels,
                                    mVorbisDsp.vi->rate));
    mFrames += aTotalFrames;
    if (vorbis_synthesis_read(&mVorbisDsp, frames) != 0) {
      return -1;
    }

    frames = vorbis_synthesis_pcmout(&mVorbisDsp, &pcm);
  }

  return aTotalFrames > 0 ? 1 : 0;
}

void
VorbisDataDecoder::DoDrain()
{
  mCallback->DrainComplete();
}

nsresult
VorbisDataDecoder::Drain()
{
  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableMethod(this, &VorbisDataDecoder::DoDrain));
  mTaskQueue->Dispatch(runnable.forget());
  return NS_OK;
}

nsresult
VorbisDataDecoder::Flush()
{
  mTaskQueue->Flush();
  // Ignore failed results from vorbis_synthesis_restart. They
  // aren't fatal and it fails when ResetDecode is called at a
  // time when no vorbis data has been read.
  vorbis_synthesis_restart(&mVorbisDsp);
  mFrames = 0;
  return NS_OK;
}

/* static */
bool
VorbisDataDecoder::IsVorbis(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("audio/webm; codecs=vorbis") ||
         aMimeType.EqualsLiteral("audio/ogg; codecs=vorbis");
}


} // namespace mozilla
#undef LOG
