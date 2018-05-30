/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VorbisDecoder.h"
#include "VorbisUtils.h"
#include "XiphExtradata.h"

#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"
#include "VideoUtils.h"

#undef LOG
#define LOG(type, msg) MOZ_LOG(sPDMLog, type, msg)

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

VorbisDataDecoder::VorbisDataDecoder(const CreateDecoderParams& aParams)
  : mInfo(aParams.AudioConfig())
  , mTaskQueue(aParams.mTaskQueue)
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

RefPtr<ShutdownPromise>
VorbisDataDecoder::Shutdown()
{
  RefPtr<VorbisDataDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self]() {
    return ShutdownPromise::CreateAndResolve(true, __func__);
  });
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
    return InitPromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("Could not get vorbis header.")),
      __func__);
  }
  for (size_t i = 0; i < headers.Length(); i++) {
    if (NS_FAILED(DecodeHeader(headers[i], headerLens[i]))) {
      return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Could not decode vorbis header.")),
        __func__);
    }
  }

  MOZ_ASSERT(mPacketCount == 3);

  int r = vorbis_synthesis_init(&mVorbisDsp, &mVorbisInfo);
  if (r) {
    return InitPromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("Systhesis init fail.")),
      __func__);
  }

  r = vorbis_block_init(&mVorbisDsp, &mVorbisBlock);
  if (r) {
    return InitPromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("Block init fail.")),
      __func__);
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
    return InitPromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("Invalid audio layout.")),
      __func__);
  }

  return InitPromise::CreateAndResolve(TrackInfo::kAudioTrack, __func__);
}

nsresult
VorbisDataDecoder::DecodeHeader(const unsigned char* aData, size_t aLength)
{
  bool bos = mPacketCount == 0;
  ogg_packet pkt =
    InitVorbisPacket(aData, aLength, bos, false, 0, mPacketCount++);
  MOZ_ASSERT(mPacketCount <= 3);

  int r = vorbis_synthesis_headerin(&mVorbisInfo,
                                    &mVorbisComment,
                                    &pkt);
  return r == 0 ? NS_OK : NS_ERROR_FAILURE;
}

RefPtr<MediaDataDecoder::DecodePromise>
VorbisDataDecoder::Decode(MediaRawData* aSample)
{
  return InvokeAsync<MediaRawData*>(mTaskQueue, this, __func__,
                                    &VorbisDataDecoder::ProcessDecode, aSample);
}

RefPtr<MediaDataDecoder::DecodePromise>
VorbisDataDecoder::ProcessDecode(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  const unsigned char* aData = aSample->Data();
  size_t aLength = aSample->Size();
  int64_t aOffset = aSample->mOffset;

  MOZ_ASSERT(mPacketCount >= 3);

  if (!mLastFrameTime ||
      mLastFrameTime.ref() != aSample->mTime.ToMicroseconds()) {
    // We are starting a new block.
    mFrames = 0;
    mLastFrameTime = Some(aSample->mTime.ToMicroseconds());
  }

  ogg_packet pkt = InitVorbisPacket(
    aData, aLength, false, aSample->mEOS,
    aSample->mTimecode.ToMicroseconds(), mPacketCount++);

  int err = vorbis_synthesis(&mVorbisBlock, &pkt);
  if (err) {
    return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                  RESULT_DETAIL("vorbis_synthesis:%d", err)),
      __func__);
  }

  err = vorbis_synthesis_blockin(&mVorbisDsp, &mVorbisBlock);
  if (err) {
    return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                  RESULT_DETAIL("vorbis_synthesis_blockin:%d", err)),
      __func__);
  }

  VorbisPCMValue** pcm = 0;
  int32_t frames = vorbis_synthesis_pcmout(&mVorbisDsp, &pcm);
  if (frames == 0) {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  }

  DecodedData results;
  while (frames > 0) {
    uint32_t channels = mVorbisDsp.vi->channels;
    uint32_t rate = mVorbisDsp.vi->rate;
    AlignedAudioBuffer buffer(frames*channels);
    if (!buffer) {
      return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__), __func__);
    }
    for (uint32_t j = 0; j < channels; ++j) {
      VorbisPCMValue* channel = pcm[j];
      for (uint32_t i = 0; i < uint32_t(frames); ++i) {
        buffer[i*channels + j] = MOZ_CONVERT_VORBIS_SAMPLE(channel[i]);
      }
    }

    auto duration = FramesToTimeUnit(frames, rate);
    if (!duration.IsValid()) {
      return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                    RESULT_DETAIL("Overflow converting audio duration")),
        __func__);
    }
    auto total_duration = FramesToTimeUnit(mFrames, rate);
    if (!total_duration.IsValid()) {
      return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                    RESULT_DETAIL("Overflow converting audio total_duration")),
        __func__);
    }

    auto time = total_duration + aSample->mTime;
    if (!time.IsValid()) {
      return DecodePromise::CreateAndReject(
        MediaResult(
          NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
          RESULT_DETAIL("Overflow adding total_duration and aSample->mTime")),
        __func__);
    };

    if (!mAudioConverter) {
      const AudioConfig::ChannelLayout layout =
        AudioConfig::ChannelLayout(channels, VorbisLayout(channels));
      AudioConfig in(layout, channels, rate);
      AudioConfig out(
        AudioConfig::ChannelLayout::SMPTEDefault(layout), channels, rate);
      mAudioConverter = MakeUnique<AudioConverter>(in, out);
    }
    MOZ_ASSERT(mAudioConverter->CanWorkInPlace());
    AudioSampleBuffer data(std::move(buffer));
    data = mAudioConverter->Process(std::move(data));

    results.AppendElement(
      new AudioData(aOffset,
                    time,
                    duration,
                    frames,
                    data.Forget(),
                    channels,
                    rate,
                    mAudioConverter->OutputConfig().Layout().Map()));
    mFrames += frames;
    err = vorbis_synthesis_read(&mVorbisDsp, frames);
    if (err) {
      return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                    RESULT_DETAIL("vorbis_synthesis_read:%d", err)),
        __func__);
    }

    frames = vorbis_synthesis_pcmout(&mVorbisDsp, &pcm);
  }
  return DecodePromise::CreateAndResolve(std::move(results), __func__);
}

RefPtr<MediaDataDecoder::DecodePromise>
VorbisDataDecoder::Drain()
{
  return InvokeAsync(mTaskQueue, __func__, [] {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  });
}

RefPtr<MediaDataDecoder::FlushPromise>
VorbisDataDecoder::Flush()
{
  RefPtr<VorbisDataDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self]() {
    // Ignore failed results from vorbis_synthesis_restart. They
    // aren't fatal and it fails when ResetDecode is called at a
    // time when no vorbis data has been read.
    vorbis_synthesis_restart(&self->mVorbisDsp);
    self->mLastFrameTime.reset();
    return FlushPromise::CreateAndResolve(true, __func__);
  });
}

/* static */
bool
VorbisDataDecoder::IsVorbis(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("audio/vorbis");
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
      static const Channel config[] = { AudioConfig::CHANNEL_FRONT_CENTER };
      return config;
    }
    case 2: // the stream is stereo. channel order: left, right
    {
      static const Channel config[] = { AudioConfig::CHANNEL_FRONT_LEFT,
                                        AudioConfig::CHANNEL_FRONT_RIGHT };
      return config;
    }
    case 3: // the stream is a 1d-surround encoding. channel order: left,
            // center, right
    {
      static const Channel config[] = { AudioConfig::CHANNEL_FRONT_LEFT,
                                        AudioConfig::CHANNEL_FRONT_CENTER,
                                        AudioConfig::CHANNEL_FRONT_RIGHT };
      return config;
    }
    case 4: // the stream is quadraphonic surround. channel order: front left,
            // front right, rear left, rear right
    {
      static const Channel config[] = { AudioConfig::CHANNEL_FRONT_LEFT,
                                        AudioConfig::CHANNEL_FRONT_RIGHT,
                                        AudioConfig::CHANNEL_BACK_LEFT,
                                        AudioConfig::CHANNEL_BACK_RIGHT };
      return config;
    }
    case 5: // the stream is five-channel surround. channel order: front left,
            // center, front right, rear left, rear right
    {
      static const Channel config[] = { AudioConfig::CHANNEL_FRONT_LEFT,
                                        AudioConfig::CHANNEL_FRONT_CENTER,
                                        AudioConfig::CHANNEL_FRONT_RIGHT,
                                        AudioConfig::CHANNEL_BACK_LEFT,
                                        AudioConfig::CHANNEL_BACK_RIGHT };
      return config;
    }
    case 6: // the stream is 5.1 surround. channel order: front left, center,
            // front right, rear left, rear right, LFE
    {
      static const Channel config[] = {
        AudioConfig::CHANNEL_FRONT_LEFT,  AudioConfig::CHANNEL_FRONT_CENTER,
        AudioConfig::CHANNEL_FRONT_RIGHT, AudioConfig::CHANNEL_BACK_LEFT,
        AudioConfig::CHANNEL_BACK_RIGHT,  AudioConfig::CHANNEL_LFE
      };
      return config;
    }
    case 7: // surround. channel order: front left, center, front right, side
            // left, side right, rear center, LFE
    {
      static const Channel config[] = {
        AudioConfig::CHANNEL_FRONT_LEFT,  AudioConfig::CHANNEL_FRONT_CENTER,
        AudioConfig::CHANNEL_FRONT_RIGHT, AudioConfig::CHANNEL_SIDE_LEFT,
        AudioConfig::CHANNEL_SIDE_RIGHT,  AudioConfig::CHANNEL_BACK_CENTER,
        AudioConfig::CHANNEL_LFE
      };
      return config;
    }
    case 8: // the stream is 7.1 surround. channel order: front left, center,
            // front right, side left, side right, rear left, rear right, LFE
    {
      static const Channel config[] = {
        AudioConfig::CHANNEL_FRONT_LEFT,  AudioConfig::CHANNEL_FRONT_CENTER,
        AudioConfig::CHANNEL_FRONT_RIGHT, AudioConfig::CHANNEL_SIDE_LEFT,
        AudioConfig::CHANNEL_SIDE_RIGHT,  AudioConfig::CHANNEL_BACK_LEFT,
        AudioConfig::CHANNEL_BACK_RIGHT,  AudioConfig::CHANNEL_LFE
      };
      return config;
    }
    default:
      return nullptr;
  }
}

} // namespace mozilla
#undef LOG
