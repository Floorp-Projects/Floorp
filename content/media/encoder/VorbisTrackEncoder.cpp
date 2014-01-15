/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VorbisTrackEncoder.h"
#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>
#include "WebMWriter.h"

// One actually used: Encoding using a VBR quality mode. The usable range is -.1
// (lowest quality, smallest file) to 1. (highest quality, largest file).
// Example quality mode .4: 44kHz stereo coupled, roughly 128kbps VBR
// ret = vorbis_encode_init_vbr(&vi,2,44100,.4);
static const float BASE_QUALITY = 0.4f;

namespace mozilla {

#undef LOG
#ifdef PR_LOGGING
PRLogModuleInfo* gVorbisTrackEncoderLog;
#define VORBISLOG(msg, ...) PR_LOG(gVorbisTrackEncoderLog, PR_LOG_DEBUG, \
                             (msg, ##__VA_ARGS__))
#else
#define VORBISLOG(msg, ...)
#endif

VorbisTrackEncoder::VorbisTrackEncoder()
  : AudioTrackEncoder()
{
  MOZ_COUNT_CTOR(VorbisTrackEncoder);
#ifdef PR_LOGGING
  if (!gVorbisTrackEncoderLog) {
    gVorbisTrackEncoderLog = PR_NewLogModule("VorbisTrackEncoder");
  }
#endif
}

VorbisTrackEncoder::~VorbisTrackEncoder()
{
  MOZ_COUNT_DTOR(VorbisTrackEncoder);
  if (mInitialized) {
    vorbis_block_clear(&mVorbisBlock);
    vorbis_dsp_clear(&mVorbisDsp);
    vorbis_info_clear(&mVorbisInfo);
  }
}

nsresult
VorbisTrackEncoder::Init(int aChannels, int aSamplingRate)
{
  if (aChannels <= 0 || aChannels > 8) {
    VORBISLOG("aChannels <= 0 || aChannels > 8");
    return NS_ERROR_INVALID_ARG;
  }

  // This monitor is used to wake up other methods that are waiting for encoder
  // to be completely initialized.
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  mChannels = aChannels;
  mSamplingRate = aSamplingRate;

  int ret = 0;
  vorbis_info_init(&mVorbisInfo);

  ret = vorbis_encode_init_vbr(&mVorbisInfo, mChannels, mSamplingRate,
                               BASE_QUALITY);

  mInitialized = (ret == 0);

  if (mInitialized) {
    // Set up the analysis state and auxiliary encoding storage
    vorbis_analysis_init(&mVorbisDsp, &mVorbisInfo);
    vorbis_block_init(&mVorbisDsp, &mVorbisBlock);
  }

  mon.NotifyAll();

  return ret == 0 ? NS_OK : NS_ERROR_FAILURE;
}

void VorbisTrackEncoder::WriteLacing(nsTArray<uint8_t> *aOutput, int32_t aLacing)
{
  while (aLacing > 255) {
    aLacing -= 255;
    aOutput->AppendElement(255);
  }
  aOutput->AppendElement((uint8_t)aLacing);
}

already_AddRefed<TrackMetadataBase>
VorbisTrackEncoder::GetMetadata()
{
  {
    // Wait if encoder is not initialized.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    while (!mCanceled && !mInitialized) {
      mon.Wait();
    }
  }

  if (mCanceled || mEncodingComplete) {
    return nullptr;
  }

  // Vorbis codec specific data
  // http://matroska.org/technical/specs/codecid/index.html
  nsRefPtr<VorbisMetadata> meta = new VorbisMetadata();
  meta->mBitDepth = 32; // float for desktop
  meta->mChannels = mChannels;
  meta->mSamplingFrequency = mSamplingRate;
  ogg_packet header;
  ogg_packet header_comm;
  ogg_packet header_code;
  // Add comment
  vorbis_comment vorbisComment;
  vorbis_comment_init(&vorbisComment);
  vorbis_comment_add_tag(&vorbisComment, "ENCODER",
    NS_LITERAL_CSTRING("Mozilla VorbisTrackEncoder " MOZ_APP_UA_VERSION).get());
  vorbis_analysis_headerout(&mVorbisDsp, &vorbisComment,
                            &header,&header_comm, &header_code);
  vorbis_comment_clear(&vorbisComment);
  // number of distinct packets - 1
  meta->mData.AppendElement(2);
  // Xiph-style lacing header.bytes, header_comm.bytes
  WriteLacing(&(meta->mData), header.bytes);
  WriteLacing(&(meta->mData), header_comm.bytes);

  // Append the three packets
  meta->mData.AppendElements(header.packet, header.bytes);
  meta->mData.AppendElements(header_comm.packet, header_comm.bytes);
  meta->mData.AppendElements(header_code.packet, header_code.bytes);

  return meta.forget();
}

void
VorbisTrackEncoder::GetEncodedFrames(EncodedFrameContainer& aData)
{
  // vorbis does some data preanalysis, then divvies up blocks for
  // more involved (potentially parallel) processing. Get a single
  // block for encoding now.
  while (vorbis_analysis_blockout(&mVorbisDsp, &mVorbisBlock) == 1) {
    ogg_packet oggPacket;
    if (vorbis_analysis(&mVorbisBlock, &oggPacket) == 0) {
      VORBISLOG("vorbis_analysis_blockout block size %d", oggPacket.bytes);
      EncodedFrame* audiodata = new EncodedFrame();
      audiodata->SetFrameType(EncodedFrame::AUDIO_FRAME);
      nsTArray<uint8_t> frameData;
      frameData.AppendElements(oggPacket.packet, oggPacket.bytes);
      audiodata->SetFrameData(&frameData);
      aData.AppendEncodedFrame(audiodata);
    }
  }
}

nsresult
VorbisTrackEncoder::GetEncodedTrack(EncodedFrameContainer& aData)
{
  if (mEosSetInEncoder) {
    return NS_OK;
  }

  nsAutoPtr<AudioSegment> sourceSegment;
  sourceSegment = new AudioSegment();
  {
    // Move all the samples from mRawSegment to sourceSegment. We only hold
    // the monitor in this block.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // Wait if mEncoder is not initialized, or when not enough raw data, but is
    // not the end of stream nor is being canceled.
    while (!mCanceled && mRawSegment.GetDuration() < GetPacketDuration() &&
           !mEndOfStream) {
      mon.Wait();
    }
    VORBISLOG("GetEncodedTrack passes wait, duration is %lld\n",
      mRawSegment.GetDuration());
    if (mCanceled || mEncodingComplete) {
      return NS_ERROR_FAILURE;
    }

    sourceSegment->AppendFrom(&mRawSegment);
  }

  if (mEndOfStream && (sourceSegment->GetDuration() == 0)
      && !mEosSetInEncoder) {
    mEncodingComplete = true;
    mEosSetInEncoder = true;
    VORBISLOG("[Vorbis] Done encoding.");
    vorbis_analysis_wrote(&mVorbisDsp, 0);
    GetEncodedFrames(aData);

    return NS_OK;
  }

  // Start encoding data.
  AudioSegment::ChunkIterator iter(*sourceSegment);

  AudioDataValue **vorbisBuffer =
    vorbis_analysis_buffer(&mVorbisDsp, (int)sourceSegment->GetDuration());

  int framesCopied = 0;
  nsAutoTArray<AudioDataValue, 9600> interleavedPcm;
  nsAutoTArray<AudioDataValue, 9600> nonInterleavedPcm;
  interleavedPcm.SetLength(sourceSegment->GetDuration() * mChannels);
  nonInterleavedPcm.SetLength(sourceSegment->GetDuration() * mChannels);
  while (!iter.IsEnded()) {
    AudioChunk chunk = *iter;
    int frameToCopy = chunk.GetDuration();
    if (!chunk.IsNull()) {
      InterleaveTrackData(chunk, frameToCopy, mChannels,
                          interleavedPcm.Elements() + framesCopied * mChannels);
    } else { // empty data
      memset(interleavedPcm.Elements() + framesCopied * mChannels, 0,
             frameToCopy * mChannels * sizeof(AudioDataValue));
    }
    framesCopied += frameToCopy;
    iter.Next();
  }
  // De-interleave the interleavedPcm.
  DeInterleaveTrackData(interleavedPcm.Elements(), framesCopied, mChannels,
                        nonInterleavedPcm.Elements());
  // Copy the nonInterleavedPcm to vorbis buffer.
  for(uint8_t i = 0; i < mChannels; ++i) {
    memcpy(vorbisBuffer[i], nonInterleavedPcm.Elements() + framesCopied * i,
           framesCopied * sizeof(AudioDataValue));
  }

  // Now the vorbisBuffer contain the all data in non-interleaved.
  // Tell the library how much we actually submitted.
  vorbis_analysis_wrote(&mVorbisDsp, framesCopied);
  VORBISLOG("vorbis_analysis_wrote framesCopied %d\n", framesCopied);
  GetEncodedFrames(aData);

  return NS_OK;
}

} // namespace mozilla
