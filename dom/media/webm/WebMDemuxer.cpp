/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "MediaDecoderStateMachine.h"
#include "AbstractMediaDecoder.h"
#include "MediaResource.h"
#include "WebMDemuxer.h"
#include "WebMBufferedParser.h"
#include "gfx2DGlue.h"
#include "mozilla/Endian.h"
#include "mozilla/Preferences.h"
#include "mozilla/SharedThreadPool.h"
#include "MediaDataDemuxer.h"
#include "nsAutoRef.h"
#include "NesteggPacketHolder.h"
#include "XiphExtradata.h"

#include <algorithm>
#include <stdint.h>

#define VPX_DONT_DEFINE_STDINT_TYPES
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"

#define WEBM_DEBUG(arg, ...) MOZ_LOG(gWebMDemuxerLog, mozilla::LogLevel::Debug, ("WebMDemuxer(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

using namespace gfx;

PRLogModuleInfo* gWebMDemuxerLog = nullptr;
extern PRLogModuleInfo* gNesteggLog;

// How far ahead will we look when searching future keyframe. In microseconds.
// This value is based on what appears to be a reasonable value as most webm
// files encountered appear to have keyframes located < 4s.
#define MAX_LOOK_AHEAD 10000000

// Functions for reading and seeking using WebMDemuxer required for
// nestegg_io. The 'user data' passed to these functions is the
// demuxer.
static int webmdemux_read(void* aBuffer, size_t aLength, void* aUserData)
{
  MOZ_ASSERT(aUserData);
  MOZ_ASSERT(aLength < UINT32_MAX);
  WebMDemuxer* demuxer = reinterpret_cast<WebMDemuxer*>(aUserData);
  uint32_t count = aLength;
  if (demuxer->IsMediaSource()) {
    int64_t length = demuxer->GetEndDataOffset();
    int64_t position = demuxer->GetResource()->Tell();
    MOZ_ASSERT(position <= demuxer->GetResource()->GetLength());
    MOZ_ASSERT(position <= length);
    if (length >= 0 && count + position > length) {
      count = length - position;
    }
    MOZ_ASSERT(count <= aLength);
  }
  uint32_t bytes = 0;
  nsresult rv =
    demuxer->GetResource()->Read(static_cast<char*>(aBuffer), count, &bytes);
  bool eof = bytes < aLength;
  return NS_FAILED(rv) ? -1 : eof ? 0 : 1;
}

static int webmdemux_seek(int64_t aOffset, int aWhence, void* aUserData)
{
  MOZ_ASSERT(aUserData);
  WebMDemuxer* demuxer = reinterpret_cast<WebMDemuxer*>(aUserData);
  nsresult rv = demuxer->GetResource()->Seek(aWhence, aOffset);
  return NS_SUCCEEDED(rv) ? 0 : -1;
}

static int64_t webmdemux_tell(void* aUserData)
{
  MOZ_ASSERT(aUserData);
  WebMDemuxer* demuxer = reinterpret_cast<WebMDemuxer*>(aUserData);
  return demuxer->GetResource()->Tell();
}

static void webmdemux_log(nestegg* aContext,
                          unsigned int aSeverity,
                          char const* aFormat, ...)
{
  if (!MOZ_LOG_TEST(gNesteggLog, LogLevel::Debug)) {
    return;
  }

  va_list args;
  char msg[256];
  const char* sevStr;

  switch(aSeverity) {
    case NESTEGG_LOG_DEBUG:
      sevStr = "DBG";
      break;
    case NESTEGG_LOG_INFO:
      sevStr = "INF";
      break;
    case NESTEGG_LOG_WARNING:
      sevStr = "WRN";
      break;
    case NESTEGG_LOG_ERROR:
      sevStr = "ERR";
      break;
    case NESTEGG_LOG_CRITICAL:
      sevStr = "CRT";
      break;
    default:
      sevStr = "UNK";
      break;
  }

  va_start(args, aFormat);

  PR_snprintf(msg, sizeof(msg), "%p [Nestegg-%s] ", aContext, sevStr);
  PR_vsnprintf(msg+strlen(msg), sizeof(msg)-strlen(msg), aFormat, args);
  MOZ_LOG(gNesteggLog, LogLevel::Debug, (msg));

  va_end(args);
}


WebMDemuxer::WebMDemuxer(MediaResource* aResource)
  : WebMDemuxer(aResource, false)
{
}

WebMDemuxer::WebMDemuxer(MediaResource* aResource, bool aIsMediaSource)
  : mResource(aResource)
  , mBufferedState(nullptr)
  , mInitData(nullptr)
  , mContext(nullptr)
  , mVideoTrack(0)
  , mAudioTrack(0)
  , mSeekPreroll(0)
  , mAudioCodec(-1)
  , mVideoCodec(-1)
  , mHasVideo(false)
  , mHasAudio(false)
  , mNeedReIndex(true)
  , mLastWebMBlockOffset(-1)
  , mIsMediaSource(aIsMediaSource)
{
  if (!gNesteggLog) {
    gNesteggLog = PR_NewLogModule("Nestegg");
  }
  if (!gWebMDemuxerLog) {
    gWebMDemuxerLog = PR_NewLogModule("WebMDemuxer");
  }
}

WebMDemuxer::~WebMDemuxer()
{
  Reset();
  Cleanup();
}

nsRefPtr<WebMDemuxer::InitPromise>
WebMDemuxer::Init()
{
  InitBufferedState();

  if (NS_FAILED(ReadMetadata())) {
    return InitPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
  }

  if (!GetNumberTracks(TrackInfo::kAudioTrack) &&
      !GetNumberTracks(TrackInfo::kVideoTrack)) {
    return InitPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
  }

  return InitPromise::CreateAndResolve(NS_OK, __func__);
}

void
WebMDemuxer::InitBufferedState()
{
  MOZ_ASSERT(!mBufferedState);
  mBufferedState = new WebMBufferedState;
}

bool
WebMDemuxer::HasTrackType(TrackInfo::TrackType aType) const
{
  return !!GetNumberTracks(aType);
}

uint32_t
WebMDemuxer::GetNumberTracks(TrackInfo::TrackType aType) const
{
  switch(aType) {
    case TrackInfo::kAudioTrack:
      return mHasAudio ? 1 : 0;
    case TrackInfo::kVideoTrack:
      return mHasVideo ? 1 : 0;
    default:
      return 0;
  }
}

UniquePtr<TrackInfo>
WebMDemuxer::GetTrackInfo(TrackInfo::TrackType aType,
                          size_t aTrackNumber) const
{
  switch(aType) {
    case TrackInfo::kAudioTrack:
      return mInfo.mAudio.Clone();
    case TrackInfo::kVideoTrack:
      return mInfo.mVideo.Clone();
    default:
      return nullptr;
  }
}

already_AddRefed<MediaTrackDemuxer>
WebMDemuxer::GetTrackDemuxer(TrackInfo::TrackType aType, uint32_t aTrackNumber)
{
  if (GetNumberTracks(aType) <= aTrackNumber) {
    return nullptr;
  }
  nsRefPtr<WebMTrackDemuxer> e =
    new WebMTrackDemuxer(this, aType, aTrackNumber);
  mDemuxers.AppendElement(e);

  return e.forget();
}

nsresult
WebMDemuxer::Reset()
{
  mVideoPackets.Reset();
  mAudioPackets.Reset();
  return NS_OK;
}

void
WebMDemuxer::Cleanup()
{
  if (mContext) {
    nestegg_destroy(mContext);
    mContext = nullptr;
  }
  mBufferedState = nullptr;
}

nsresult
WebMDemuxer::ReadMetadata()
{
  nestegg_io io;
  io.read = webmdemux_read;
  io.seek = webmdemux_seek;
  io.tell = webmdemux_tell;
  io.userdata = this;
  int r = nestegg_init(&mContext, io, &webmdemux_log,
                       IsMediaSource() ? mResource.GetLength() : -1);
  if (r == -1) {
    return NS_ERROR_FAILURE;
  }
  {
    // Check how much data nestegg read and force feed it to BufferedState.
    nsRefPtr<MediaByteBuffer> buffer = mResource.MediaReadAt(0, mResource.Tell());
    if (!buffer) {
      return NS_ERROR_FAILURE;
    }
    mBufferedState->NotifyDataArrived(buffer->Elements(), buffer->Length(), 0);
    if (mBufferedState->GetInitEndOffset() < 0) {
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(mBufferedState->GetInitEndOffset() <= mResource.Tell());
  }
  mInitData = mResource.MediaReadAt(0, mBufferedState->GetInitEndOffset());
  if (!mInitData ||
      mInitData->Length() != size_t(mBufferedState->GetInitEndOffset())) {
    return NS_ERROR_FAILURE;
  }

  unsigned int ntracks = 0;
  r = nestegg_track_count(mContext, &ntracks);
  if (r == -1) {
    return NS_ERROR_FAILURE;
  }

  for (unsigned int track = 0; track < ntracks; ++track) {
    int id = nestegg_track_codec_id(mContext, track);
    if (id == -1) {
      return NS_ERROR_FAILURE;
    }
    int type = nestegg_track_type(mContext, track);
    if (type == NESTEGG_TRACK_VIDEO && !mHasVideo) {
      nestegg_video_params params;
      r = nestegg_track_video_params(mContext, track, &params);
      if (r == -1) {
        return NS_ERROR_FAILURE;
      }
      mVideoCodec = nestegg_track_codec_id(mContext, track);
      switch(mVideoCodec) {
        case NESTEGG_CODEC_VP8:
          mInfo.mVideo.mMimeType = "video/webm; codecs=vp8";
          break;
        case NESTEGG_CODEC_VP9:
          mInfo.mVideo.mMimeType = "video/webm; codecs=vp9";
          break;
        default:
          NS_WARNING("Unknown WebM video codec");
          return NS_ERROR_FAILURE;
      }
      // Picture region, taking into account cropping, before scaling
      // to the display size.
      unsigned int cropH = params.crop_right + params.crop_left;
      unsigned int cropV = params.crop_bottom + params.crop_top;
      nsIntRect pictureRect(params.crop_left,
                            params.crop_top,
                            params.width - cropH,
                            params.height - cropV);

      // If the cropping data appears invalid then use the frame data
      if (pictureRect.width <= 0 ||
          pictureRect.height <= 0 ||
          pictureRect.x < 0 ||
          pictureRect.y < 0) {
        pictureRect.x = 0;
        pictureRect.y = 0;
        pictureRect.width = params.width;
        pictureRect.height = params.height;
      }

      // Validate the container-reported frame and pictureRect sizes. This
      // ensures that our video frame creation code doesn't overflow.
      nsIntSize displaySize(params.display_width, params.display_height);
      nsIntSize frameSize(params.width, params.height);
      if (!IsValidVideoRegion(frameSize, pictureRect, displaySize)) {
        // Video track's frame sizes will overflow. Ignore the video track.
        continue;
      }

      mVideoTrack = track;
      mHasVideo = true;

      mInfo.mVideo.mDisplay = displaySize;
      mInfo.mVideo.mImage = pictureRect;

      switch (params.stereo_mode) {
        case NESTEGG_VIDEO_MONO:
          mInfo.mVideo.mStereoMode = StereoMode::MONO;
          break;
        case NESTEGG_VIDEO_STEREO_LEFT_RIGHT:
          mInfo.mVideo.mStereoMode = StereoMode::LEFT_RIGHT;
          break;
        case NESTEGG_VIDEO_STEREO_BOTTOM_TOP:
          mInfo.mVideo.mStereoMode = StereoMode::BOTTOM_TOP;
          break;
        case NESTEGG_VIDEO_STEREO_TOP_BOTTOM:
          mInfo.mVideo.mStereoMode = StereoMode::TOP_BOTTOM;
          break;
        case NESTEGG_VIDEO_STEREO_RIGHT_LEFT:
          mInfo.mVideo.mStereoMode = StereoMode::RIGHT_LEFT;
          break;
      }
      uint64_t duration = 0;
      r = nestegg_duration(mContext, &duration);
      if (!r) {
        mInfo.mVideo.mDuration = media::TimeUnit::FromNanoseconds(duration).ToMicroseconds();
      }
    } else if (type == NESTEGG_TRACK_AUDIO && !mHasAudio) {
      nestegg_audio_params params;
      r = nestegg_track_audio_params(mContext, track, &params);
      if (r == -1) {
        return NS_ERROR_FAILURE;
      }

      mAudioTrack = track;
      mHasAudio = true;
      mCodecDelay = media::TimeUnit::FromNanoseconds(params.codec_delay).ToMicroseconds();
      mAudioCodec = nestegg_track_codec_id(mContext, track);
      if (mAudioCodec == NESTEGG_CODEC_VORBIS) {
        mInfo.mAudio.mMimeType = "audio/ogg; codecs=vorbis";
      } else if (mAudioCodec == NESTEGG_CODEC_OPUS) {
        mInfo.mAudio.mMimeType = "audio/ogg; codecs=opus";
        uint8_t c[sizeof(uint64_t)];
        BigEndian::writeUint64(&c[0], mCodecDelay);
        mInfo.mAudio.mCodecSpecificConfig->AppendElements(&c[0], sizeof(uint64_t));
      }
      mSeekPreroll = params.seek_preroll;
      mInfo.mAudio.mRate = params.rate;
      mInfo.mAudio.mChannels = params.channels;

      unsigned int nheaders = 0;
      r = nestegg_track_codec_data_count(mContext, track, &nheaders);
      if (r == -1) {
        return NS_ERROR_FAILURE;
      }

      nsAutoTArray<const unsigned char*,4> headers;
      nsAutoTArray<size_t,4> headerLens;
      for (uint32_t header = 0; header < nheaders; ++header) {
        unsigned char* data = 0;
        size_t length = 0;
        r = nestegg_track_codec_data(mContext, track, header, &data, &length);
        if (r == -1) {
          return NS_ERROR_FAILURE;
        }
        headers.AppendElement(data);
        headerLens.AppendElement(length);
      }

      // Vorbis has 3 headers, convert to Xiph extradata format to send them to
      // the demuxer.
      // TODO: This is already the format WebM stores them in. Would be nice
      // to avoid having libnestegg split them only for us to pack them again,
      // but libnestegg does not give us an API to access this data directly.
      if (nheaders > 1) {
        if (!XiphHeadersToExtradata(mInfo.mAudio.mCodecSpecificConfig,
                                    headers, headerLens)) {
          return NS_ERROR_FAILURE;
        }
      }
      else {
        mInfo.mAudio.mCodecSpecificConfig->AppendElements(headers[0],
                                                          headerLens[0]);
      }
      uint64_t duration = 0;
      r = nestegg_duration(mContext, &duration);
      if (!r) {
        mInfo.mAudio.mDuration = media::TimeUnit::FromNanoseconds(duration).ToMicroseconds();
      }
    }
  }
  return NS_OK;
}

bool
WebMDemuxer::IsSeekable() const
{
  return mContext && nestegg_has_cues(mContext);
}

void
WebMDemuxer::EnsureUpToDateIndex()
{
  if (!mNeedReIndex || !mInitData) {
    return;
  }
  AutoPinned<MediaResource> resource(mResource.GetResource());
  nsTArray<MediaByteRange> byteRanges;
  nsresult rv = resource->GetCachedRanges(byteRanges);
  if (NS_FAILED(rv) || !byteRanges.Length()) {
    return;
  }
  mBufferedState->UpdateIndex(byteRanges, resource);

  mNeedReIndex = false;

  if (!mIsMediaSource) {
    return;
  }
  mLastWebMBlockOffset = mBufferedState->GetLastBlockOffset();
  MOZ_ASSERT(mLastWebMBlockOffset <= mResource.GetLength());
}

void
WebMDemuxer::NotifyDataArrived()
{
  WEBM_DEBUG("");
  mNeedReIndex = true;
}

void
WebMDemuxer::NotifyDataRemoved()
{
  mBufferedState->Reset();
  if (mInitData) {
    mBufferedState->NotifyDataArrived(mInitData->Elements(), mInitData->Length(), 0);
  }
  mNeedReIndex = true;
}

UniquePtr<EncryptionInfo>
WebMDemuxer::GetCrypto()
{
  return nullptr;
}

bool
WebMDemuxer::GetNextPacket(TrackInfo::TrackType aType, MediaRawDataQueue *aSamples)
{
  if (mIsMediaSource) {
    // To ensure mLastWebMBlockOffset is properly up to date.
    EnsureUpToDateIndex();
  }

  nsRefPtr<NesteggPacketHolder> holder(NextPacket(aType));

  if (!holder) {
    return false;
  }

  int r = 0;
  unsigned int count = 0;
  r = nestegg_packet_count(holder->Packet(), &count);
  if (r == -1) {
    return false;
  }
  int64_t tstamp = holder->Timestamp();

  // The end time of this frame is the start time of the next frame.  Fetch
  // the timestamp of the next packet for this track.  If we've reached the
  // end of the resource, use the file's duration as the end time of this
  // video frame.
  int64_t next_tstamp = INT64_MIN;
  if (aType == TrackInfo::kAudioTrack) {
    nsRefPtr<NesteggPacketHolder> next_holder(NextPacket(aType));
    if (next_holder) {
      next_tstamp = next_holder->Timestamp();
      PushAudioPacket(next_holder);
    } else if (!mIsMediaSource ||
               (mIsMediaSource && mLastAudioFrameTime.isSome())) {
      next_tstamp = tstamp;
      next_tstamp += tstamp - mLastAudioFrameTime.refOr(0);
    } else {
      PushAudioPacket(holder);
    }
    mLastAudioFrameTime = Some(tstamp);
  } else if (aType == TrackInfo::kVideoTrack) {
    nsRefPtr<NesteggPacketHolder> next_holder(NextPacket(aType));
    if (next_holder) {
      next_tstamp = next_holder->Timestamp();
      PushVideoPacket(next_holder);
    } else if (!mIsMediaSource ||
               (mIsMediaSource && mLastVideoFrameTime.isSome())) {
      next_tstamp = tstamp;
      next_tstamp += tstamp - mLastVideoFrameTime.refOr(0);
    } else {
      PushVideoPacket(holder);
    }
    mLastVideoFrameTime = Some(tstamp);
  }

  if (mIsMediaSource && next_tstamp == INT64_MIN) {
    return false;
  }

  int64_t discardPadding = 0;
  (void) nestegg_packet_discard_padding(holder->Packet(), &discardPadding);

  for (uint32_t i = 0; i < count; ++i) {
    unsigned char* data;
    size_t length;
    r = nestegg_packet_data(holder->Packet(), i, &data, &length);
    if (r == -1) {
      WEBM_DEBUG("nestegg_packet_data failed r=%d", r);
      return false;
    }
    bool isKeyframe = false;
    if (aType == TrackInfo::kAudioTrack) {
      isKeyframe = true;
    } else if (aType == TrackInfo::kVideoTrack) {
      vpx_codec_stream_info_t si;
      PodZero(&si);
      si.sz = sizeof(si);
      switch (mVideoCodec) {
        case NESTEGG_CODEC_VP8:
          vpx_codec_peek_stream_info(vpx_codec_vp8_dx(), data, length, &si);
          break;
        case NESTEGG_CODEC_VP9:
          vpx_codec_peek_stream_info(vpx_codec_vp9_dx(), data, length, &si);
          break;
      }
      isKeyframe = si.is_kf;
    }

    WEBM_DEBUG("push sample tstamp: %ld next_tstamp: %ld length: %ld kf: %d",
               tstamp, next_tstamp, length, isKeyframe);
    nsRefPtr<MediaRawData> sample = new MediaRawData(data, length);
    sample->mTimecode = tstamp;
    sample->mTime = tstamp;
    sample->mDuration = next_tstamp - tstamp;
    sample->mOffset = holder->Offset();
    sample->mKeyframe = isKeyframe;
    if (discardPadding) {
      uint8_t c[8];
      BigEndian::writeInt64(&c[0], discardPadding);
      sample->mExtraData = new MediaByteBuffer;
      sample->mExtraData->AppendElements(&c[0], 8);
    }
    aSamples->Push(sample);
  }
  return true;
}

nsRefPtr<NesteggPacketHolder>
WebMDemuxer::NextPacket(TrackInfo::TrackType aType)
{
  bool isVideo = aType == TrackInfo::kVideoTrack;

  // The packet queue that packets will be pushed on if they
  // are not the type we are interested in.
  WebMPacketQueue& otherPackets = isVideo ? mAudioPackets : mVideoPackets;

  // The packet queue for the type that we are interested in.
  WebMPacketQueue &packets = isVideo ? mVideoPackets : mAudioPackets;

  // Flag to indicate that we do need to playback these types of
  // packets.
  bool hasType = isVideo ? mHasVideo : mHasAudio;

  // Flag to indicate that we do need to playback the other type
  // of track.
  bool hasOtherType = isVideo ? mHasAudio : mHasVideo;

  // Track we are interested in
  uint32_t ourTrack = isVideo ? mVideoTrack : mAudioTrack;

  // Value of other track
  uint32_t otherTrack = isVideo ? mAudioTrack : mVideoTrack;

  if (packets.GetSize() > 0) {
    return packets.PopFront();
  }

  do {
    nsRefPtr<NesteggPacketHolder> holder = DemuxPacket();
    if (!holder) {
      return nullptr;
    }

    if (hasOtherType && otherTrack == holder->Track()) {
      // Save the packet for when we want these packets
      otherPackets.Push(holder);
      continue;
    }

    // The packet is for the track we want to play
    if (hasType && ourTrack == holder->Track()) {
      return holder;
    }
  } while (true);
}

nsRefPtr<NesteggPacketHolder>
WebMDemuxer::DemuxPacket()
{
  nestegg_packet* packet;
  int r = nestegg_read_packet(mContext, &packet);
  if (r <= 0) {
    return nullptr;
  }

  unsigned int track = 0;
  r = nestegg_packet_track(packet, &track);
  if (r == -1) {
    return nullptr;
  }

  int64_t offset = mResource.Tell();
  nsRefPtr<NesteggPacketHolder> holder = new NesteggPacketHolder();
  if (!holder->Init(packet, offset, track, false)) {
    return nullptr;
  }

  return holder;
}

void
WebMDemuxer::PushAudioPacket(NesteggPacketHolder* aItem)
{
  mAudioPackets.PushFront(aItem);
}

void
WebMDemuxer::PushVideoPacket(NesteggPacketHolder* aItem)
{
  mVideoPackets.PushFront(aItem);
}

nsresult
WebMDemuxer::SeekInternal(const media::TimeUnit& aTarget)
{
  EnsureUpToDateIndex();
  uint32_t trackToSeek = mHasVideo ? mVideoTrack : mAudioTrack;
  uint64_t target = aTarget.ToNanoseconds();

  if (NS_FAILED(Reset())) {
    return NS_ERROR_FAILURE;
  }

  if (mSeekPreroll) {
    uint64_t startTime = 0;
    if (!mBufferedState->GetStartTime(&startTime)) {
      startTime = 0;
    }
    WEBM_DEBUG("Seek Target: %f",
               media::TimeUnit::FromNanoseconds(target).ToSeconds());
    if (target < mSeekPreroll || target - mSeekPreroll < startTime) {
      target = startTime;
    } else {
      target -= mSeekPreroll;
    }
    WEBM_DEBUG("SeekPreroll: %f StartTime: %f Adjusted Target: %f",
               media::TimeUnit::FromNanoseconds(mSeekPreroll).ToSeconds(),
               media::TimeUnit::FromNanoseconds(startTime).ToSeconds(),
               media::TimeUnit::FromNanoseconds(target).ToSeconds());
  }
  int r = nestegg_track_seek(mContext, trackToSeek, target);
  if (r == -1) {
    WEBM_DEBUG("track_seek for track %u to %f failed, r=%d", trackToSeek,
               media::TimeUnit::FromNanoseconds(target).ToSeconds(), r);
    // Try seeking directly based on cluster information in memory.
    int64_t offset = 0;
    bool rv = mBufferedState->GetOffsetForTime(target, &offset);
    if (!rv) {
      WEBM_DEBUG("mBufferedState->GetOffsetForTime failed too");
      return NS_ERROR_FAILURE;
    }

    r = nestegg_offset_seek(mContext, offset);
    if (r == -1) {
      WEBM_DEBUG("and nestegg_offset_seek to %" PRIu64 " failed", offset);
      return NS_ERROR_FAILURE;
    }
    WEBM_DEBUG("got offset from buffered state: %" PRIu64 "", offset);
  }

  mLastAudioFrameTime.reset();
  mLastVideoFrameTime.reset();

  return NS_OK;
}

media::TimeIntervals
WebMDemuxer::GetBuffered()
{
  EnsureUpToDateIndex();
  AutoPinned<MediaResource> resource(mResource.GetResource());

  media::TimeIntervals buffered;

  nsTArray<MediaByteRange> ranges;
  nsresult rv = resource->GetCachedRanges(ranges);
  if (NS_FAILED(rv)) {
    return media::TimeIntervals();
  }
  uint64_t duration = 0;
  uint64_t startOffset = 0;
  if (!nestegg_duration(mContext, &duration)) {
    if(mBufferedState->GetStartTime(&startOffset)) {
      duration += startOffset;
    }
    WEBM_DEBUG("Duration: %f StartTime: %f",
               media::TimeUnit::FromNanoseconds(duration).ToSeconds(),
               media::TimeUnit::FromNanoseconds(startOffset).ToSeconds());
  }
  for (uint32_t index = 0; index < ranges.Length(); index++) {
    uint64_t start, end;
    bool rv = mBufferedState->CalculateBufferedForRange(ranges[index].mStart,
                                                        ranges[index].mEnd,
                                                        &start, &end);
    if (rv) {
      NS_ASSERTION(startOffset <= start,
          "startOffset negative or larger than start time");

      if (duration && end > duration) {
        WEBM_DEBUG("limit range to duration, end: %f duration: %f",
                   media::TimeUnit::FromNanoseconds(end).ToSeconds(),
                   media::TimeUnit::FromNanoseconds(duration).ToSeconds());
        end = duration;
      }
      media::TimeUnit startTime = media::TimeUnit::FromNanoseconds(start);
      media::TimeUnit endTime = media::TimeUnit::FromNanoseconds(end);
      WEBM_DEBUG("add range %f-%f", startTime.ToSeconds(), endTime.ToSeconds());
      buffered += media::TimeInterval(startTime, endTime);
    }
  }
  return buffered;
}

bool WebMDemuxer::GetOffsetForTime(uint64_t aTime, int64_t* aOffset)
{
  EnsureUpToDateIndex();
  return mBufferedState && mBufferedState->GetOffsetForTime(aTime, aOffset);
}


//WebMTrackDemuxer
WebMTrackDemuxer::WebMTrackDemuxer(WebMDemuxer* aParent,
                                   TrackInfo::TrackType aType,
                                   uint32_t aTrackNumber)
  : mParent(aParent)
  , mType(aType)
{
  mInfo = mParent->GetTrackInfo(aType, aTrackNumber);
  MOZ_ASSERT(mInfo);
}

WebMTrackDemuxer::~WebMTrackDemuxer()
{
  mSamples.Reset();
}

UniquePtr<TrackInfo>
WebMTrackDemuxer::GetInfo() const
{
  return mInfo->Clone();
}

nsRefPtr<WebMTrackDemuxer::SeekPromise>
WebMTrackDemuxer::Seek(media::TimeUnit aTime)
{
  // Seeks to aTime. Upon success, SeekPromise will be resolved with the
  // actual time seeked to. Typically the random access point time

  media::TimeUnit seekTime = aTime;
  mSamples.Reset();
  mParent->SeekInternal(aTime);
  mParent->GetNextPacket(mType, &mSamples);

  // Check what time we actually seeked to.
  if (mSamples.GetSize() > 0) {
    nsRefPtr<MediaRawData> sample(mSamples.PopFront());
    seekTime = media::TimeUnit::FromMicroseconds(sample->mTime);
    mSamples.PushFront(sample);
  }
  SetNextKeyFrameTime();

  return SeekPromise::CreateAndResolve(seekTime, __func__);
}

nsRefPtr<MediaRawData>
WebMTrackDemuxer::NextSample()
{
  while (mSamples.GetSize() < 1 && mParent->GetNextPacket(mType, &mSamples)) {
  }
  if (mSamples.GetSize()) {
    return mSamples.PopFront();
  }
  return nullptr;
}

nsRefPtr<WebMTrackDemuxer::SamplesPromise>
WebMTrackDemuxer::GetSamples(int32_t aNumSamples)
{
  nsRefPtr<SamplesHolder> samples = new SamplesHolder;
  if (!aNumSamples) {
    return SamplesPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
  }

  while (aNumSamples) {
    nsRefPtr<MediaRawData> sample(NextSample());
    if (!sample) {
      break;
    }
    samples->mSamples.AppendElement(sample);
    aNumSamples--;
  }

  if (samples->mSamples.IsEmpty()) {
    return SamplesPromise::CreateAndReject(DemuxerFailureReason::END_OF_STREAM, __func__);
  } else {
    UpdateSamples(samples->mSamples);
    return SamplesPromise::CreateAndResolve(samples, __func__);
  }
}

void
WebMTrackDemuxer::SetNextKeyFrameTime()
{
  if (mType != TrackInfo::kVideoTrack) {
    return;
  }

  int64_t frameTime = -1;

  mNextKeyframeTime.reset();

  MediaRawDataQueue skipSamplesQueue;
  nsRefPtr<MediaRawData> sample;
  bool foundKeyframe = false;
  while (!foundKeyframe && mSamples.GetSize()) {
    sample = mSamples.PopFront();
    if (sample->mKeyframe) {
      frameTime = sample->mTime;
      foundKeyframe = true;
    }
    skipSamplesQueue.Push(sample);
  }
  Maybe<int64_t> startTime;
  if (skipSamplesQueue.GetSize()) {
    sample = skipSamplesQueue.PopFront();
    startTime.emplace(sample->mTimecode);
    skipSamplesQueue.PushFront(sample);
  }
  // Demux and buffer frames until we find a keyframe.
  while (!foundKeyframe && (sample = NextSample())) {
    if (sample->mKeyframe) {
      frameTime = sample->mTime;
      foundKeyframe = true;
    }
    skipSamplesQueue.Push(sample);
    if (!startTime) {
      startTime.emplace(sample->mTimecode);
    } else if (!foundKeyframe &&
               sample->mTimecode > startTime.ref() + MAX_LOOK_AHEAD) {
      WEBM_DEBUG("Couldn't find keyframe in a reasonable time, aborting");
      break;
    }
  }
  // We may have demuxed more than intended, so ensure that all frames are kept
  // in the right order.
  mSamples.PushFront(skipSamplesQueue);

  if (frameTime != -1) {
    mNextKeyframeTime.emplace(media::TimeUnit::FromMicroseconds(frameTime));
    WEBM_DEBUG("Next Keyframe %f (%u queued %.02fs)",
               mNextKeyframeTime.value().ToSeconds(),
               uint32_t(mSamples.GetSize()),
               media::TimeUnit::FromMicroseconds(mSamples.Last()->mTimecode - mSamples.First()->mTimecode).ToSeconds());
  } else {
    WEBM_DEBUG("Couldn't determine next keyframe time  (%u queued)",
               uint32_t(mSamples.GetSize()));
  }
}

void
WebMTrackDemuxer::Reset()
{
  mSamples.Reset();
  media::TimeIntervals buffered = GetBuffered();
  if (buffered.Length()) {
    WEBM_DEBUG("Seek to start point: %f", buffered.Start(0).ToSeconds());
    mParent->SeekInternal(buffered.Start(0));
    SetNextKeyFrameTime();
  } else {
    mNextKeyframeTime.reset();
  }
}

void
WebMTrackDemuxer::UpdateSamples(nsTArray<nsRefPtr<MediaRawData>>& aSamples)
{
  if (mNextKeyframeTime.isNothing() ||
      aSamples.LastElement()->mTime >= mNextKeyframeTime.value().ToMicroseconds()) {
    SetNextKeyFrameTime();
  }
}

nsresult
WebMTrackDemuxer::GetNextRandomAccessPoint(media::TimeUnit* aTime)
{
  if (mNextKeyframeTime.isNothing()) {
    // There's no next key frame.
    *aTime =
      media::TimeUnit::FromMicroseconds(std::numeric_limits<int64_t>::max());
  } else {
    *aTime = mNextKeyframeTime.ref();
  }
  return NS_OK;
}

nsRefPtr<WebMTrackDemuxer::SkipAccessPointPromise>
WebMTrackDemuxer::SkipToNextRandomAccessPoint(media::TimeUnit aTimeThreshold)
{
  uint32_t parsed = 0;
  bool found = false;
  nsRefPtr<MediaRawData> sample;

  WEBM_DEBUG("TimeThreshold: %f", aTimeThreshold.ToSeconds());
  while (!found && (sample = NextSample())) {
    parsed++;
    if (sample->mKeyframe && sample->mTime >= aTimeThreshold.ToMicroseconds()) {
      found = true;
      mSamples.Reset();
      mSamples.PushFront(sample);
    }
  }
  SetNextKeyFrameTime();
  if (found) {
    WEBM_DEBUG("next sample: %f (parsed: %d)",
               media::TimeUnit::FromMicroseconds(sample->mTime).ToSeconds(),
               parsed);
    return SkipAccessPointPromise::CreateAndResolve(parsed, __func__);
  } else {
    SkipFailureHolder failure(DemuxerFailureReason::END_OF_STREAM, parsed);
    return SkipAccessPointPromise::CreateAndReject(Move(failure), __func__);
  }
}

media::TimeIntervals
WebMTrackDemuxer::GetBuffered()
{
  return mParent->GetBuffered();
}

void
WebMTrackDemuxer::BreakCycles()
{
  mParent = nullptr;
}

#undef WEBM_DEBUG
} // namespace mozilla
