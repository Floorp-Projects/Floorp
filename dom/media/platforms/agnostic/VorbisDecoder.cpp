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

  AutoTArray<unsigned char*,4> headers;
  AutoTArray<size_t,4> headerLens;
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

  AudioConfig::ChannelLayout layout(mVorbisDsp.vi->channels);
  if (!layout.IsValid()) {
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
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
  mTaskQueue->Dispatch(NewRunnableMethod<RefPtr<MediaRawData>>(
                         this, &VorbisDataDecoder::Decode,
                         RefPtr<MediaRawData>(aSample)));

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

  if (!mLastFrameTime || mLastFrameTime.ref() != aSample->mTime) {
    // We are starting a new block.
    mFrames = 0;
    mLastFrameTime = Some(aSample->mTime);
  }

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
                                    AlignedAudioBuffer(),
                                    mVorbisDsp.vi->channels,
                                    mVorbisDsp.vi->rate));
  }
  while (frames > 0) {
    uint32_t channels = mVorbisDsp.vi->channels;
    uint32_t rate = mVorbisDsp.vi->rate;
    AlignedAudioBuffer buffer(frames*channels);
    if (!buffer) {
      return -1;
    }
    for (uint32_t j = 0; j < channels; ++j) {
      VorbisPCMValue* channel = pcm[j];
      for (uint32_t i = 0; i < uint32_t(frames); ++i) {
        buffer[i*channels + j] = MOZ_CONVERT_VORBIS_SAMPLE(channel[i]);
      }
    }

    CheckedInt64 duration = FramesToUsecs(frames, rate);
    if (!duration.isValid()) {
      NS_WARNING("Int overflow converting WebM audio duration");
      return -1;
    }
    CheckedInt64 total_duration = FramesToUsecs(mFrames, rate);
    if (!total_duration.isValid()) {
      NS_WARNING("Int overflow converting WebM audio total_duration");
      return -1;
    }

    CheckedInt64 time = total_duration + aTstampUsecs;
    if (!time.isValid()) {
      NS_WARNING("Int overflow adding total_duration and aTstampUsecs");
      return -1;
    };

    if (!mAudioConverter) {
      AudioConfig in(AudioConfig::ChannelLayout(channels, VorbisLayout(channels)),
                     rate);
      AudioConfig out(channels, rate);
      if (!in.IsValid() || !out.IsValid()) {
       return -1;
      }
      mAudioConverter = MakeUnique<AudioConverter>(in, out);
    }
    MOZ_ASSERT(mAudioConverter->CanWorkInPlace());
    AudioSampleBuffer data(Move(buffer));
    data = mAudioConverter->Process(Move(data));

    aTotalFrames += frames;
    mCallback->Output(new AudioData(aOffset,
                                    time.value(),
                                    duration.value(),
                                    frames,
                                    data.Forget(),
                                    channels,
                                    rate));
    mFrames += frames;
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
  mTaskQueue->Dispatch(NewRunnableMethod(this, &VorbisDataDecoder::DoDrain));
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
  mLastFrameTime.reset();
  return NS_OK;
}

/* static */
bool
VorbisDataDecoder::IsVorbis(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("audio/webm; codecs=vorbis") ||
         aMimeType.EqualsLiteral("audio/ogg; codecs=vorbis");
}

/* static */ const AudioConfig::Channel*
VorbisDataDecoder::VorbisLayout(uint32_t aChannels)
{
  // From https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html
  // Section 4.3.9.
  typedef AudioConfig::Channel Channel;

  switch (aChannels) {
    case 1: // the stream is monophonic
    {
      static const Channel config[] = { AudioConfig::CHANNEL_MONO };
      return config;
    }
    case 2: // the stream is stereo. channel order: left, right
    {
      static const Channel config[] = { AudioConfig::CHANNEL_LEFT, AudioConfig::CHANNEL_RIGHT };
      return config;
    }
    case 3: // the stream is a 1d-surround encoding. channel order: left, center, right
    {
      static const Channel config[] = { AudioConfig::CHANNEL_LEFT, AudioConfig::CHANNEL_CENTER, AudioConfig::CHANNEL_RIGHT };
      return config;
    }
    case 4: // the stream is quadraphonic surround. channel order: front left, front right, rear left, rear right
    {
      static const Channel config[] = { AudioConfig::CHANNEL_LEFT, AudioConfig::CHANNEL_RIGHT, AudioConfig::CHANNEL_LS, AudioConfig::CHANNEL_RS };
      return config;
    }
    case 5: // the stream is five-channel surround. channel order: front left, center, front right, rear left, rear right
    {
      static const Channel config[] = { AudioConfig::CHANNEL_LEFT, AudioConfig::CHANNEL_CENTER, AudioConfig::CHANNEL_RIGHT, AudioConfig::CHANNEL_LS, AudioConfig::CHANNEL_RS };
      return config;
    }
    case 6: // the stream is 5.1 surround. channel order: front left, center, front right, rear left, rear right, LFE
    {
      static const Channel config[] = { AudioConfig::CHANNEL_LEFT, AudioConfig::CHANNEL_CENTER, AudioConfig::CHANNEL_RIGHT, AudioConfig::CHANNEL_LS, AudioConfig::CHANNEL_RS, AudioConfig::CHANNEL_LFE };
      return config;
    }
    case 7: // surround. channel order: front left, center, front right, side left, side right, rear center, LFE
    {
      static const Channel config[] = { AudioConfig::CHANNEL_LEFT, AudioConfig::CHANNEL_CENTER, AudioConfig::CHANNEL_RIGHT, AudioConfig::CHANNEL_LS, AudioConfig::CHANNEL_RS, AudioConfig::CHANNEL_RCENTER, AudioConfig::CHANNEL_LFE };
      return config;
    }
    case 8: // the stream is 7.1 surround. channel order: front left, center, front right, side left, side right, rear left, rear right, LFE
    {
      static const Channel config[] = { AudioConfig::CHANNEL_LEFT, AudioConfig::CHANNEL_CENTER, AudioConfig::CHANNEL_RIGHT, AudioConfig::CHANNEL_LS, AudioConfig::CHANNEL_RS, AudioConfig::CHANNEL_RLS, AudioConfig::CHANNEL_RRS, AudioConfig::CHANNEL_LFE };
      return config;
    }
    default:
      return nullptr;
  }
}

} // namespace mozilla
#undef LOG
