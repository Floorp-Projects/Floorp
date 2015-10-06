/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsError.h"
#include "MediaDecoderStateMachine.h"
#include "AbstractMediaDecoder.h"
#include "SoftwareWebMVideoDecoder.h"
#include "WebMReader.h"
#include "WebMBufferedParser.h"
#include "gfx2DGlue.h"
#include "Layers.h"
#include "mozilla/Preferences.h"
#include "mozilla/SharedThreadPool.h"

#include <algorithm>

#define VPX_DONT_DEFINE_STDINT_TYPES
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

#undef LOG

#include "prprf.h"
#define LOG(type, msg) MOZ_LOG(gMediaDecoderLog, type, msg)
#ifdef SEEK_LOGGING
#define SEEK_LOG(type, msg) MOZ_LOG(gMediaDecoderLog, type, msg)
#else
#define SEEK_LOG(type, msg)
#endif

namespace mozilla {

using namespace gfx;
using namespace layers;
using namespace media;

extern PRLogModuleInfo* gMediaDecoderLog;
PRLogModuleInfo* gNesteggLog;

// Functions for reading and seeking using MediaResource required for
// nestegg_io. The 'user data' passed to these functions is the
// decoder from which the media resource is obtained.
static int webm_read(void *aBuffer, size_t aLength, void *aUserData)
{
  MOZ_ASSERT(aUserData);
  MediaResourceIndex* resource =
    reinterpret_cast<MediaResourceIndex*>(aUserData);

  nsresult rv = NS_OK;
  uint32_t bytes = 0;

  rv = resource->Read(static_cast<char *>(aBuffer), aLength, &bytes);

  bool eof = !bytes;

  return NS_FAILED(rv) ? -1 : eof ? 0 : 1;
}

static int webm_seek(int64_t aOffset, int aWhence, void *aUserData)
{
  MOZ_ASSERT(aUserData);
  MediaResourceIndex* resource =
    reinterpret_cast<MediaResourceIndex*>(aUserData);
  nsresult rv = resource->Seek(aWhence, aOffset);
  return NS_SUCCEEDED(rv) ? 0 : -1;
}

static int64_t webm_tell(void *aUserData)
{
  MOZ_ASSERT(aUserData);
  MediaResourceIndex* resource =
    reinterpret_cast<MediaResourceIndex*>(aUserData);
  return resource->Tell();
}

static void webm_log(nestegg * context,
                     unsigned int severity,
                     char const * format, ...)
{
  if (!MOZ_LOG_TEST(gNesteggLog, LogLevel::Debug)) {
    return;
  }

  va_list args;
  char msg[256];
  const char * sevStr;

  switch(severity) {
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

  va_start(args, format);

  PR_snprintf(msg, sizeof(msg), "%p [Nestegg-%s] ", context, sevStr);
  PR_vsnprintf(msg+strlen(msg), sizeof(msg)-strlen(msg), format, args);
  MOZ_LOG(gNesteggLog, LogLevel::Debug, (msg));

  va_end(args);
}

WebMReader::WebMReader(AbstractMediaDecoder* aDecoder, TaskQueue* aBorrowedTaskQueue)
  : MediaDecoderReader(aDecoder, aBorrowedTaskQueue)
  , mContext(nullptr)
  , mVideoTrack(0)
  , mAudioTrack(0)
  , mAudioStartUsec(-1)
  , mAudioFrames(0)
  , mSeekPreroll(0)
  , mLastVideoFrameTime(0)
  , mAudioCodec(-1)
  , mVideoCodec(-1)
  , mLayersBackendType(layers::LayersBackend::LAYERS_NONE)
  , mHasVideo(false)
  , mHasAudio(false)
  , mResource(aDecoder->GetResource())
{
  MOZ_COUNT_CTOR(WebMReader);
  if (!gNesteggLog) {
    gNesteggLog = PR_NewLogModule("Nestegg");
  }
}

WebMReader::~WebMReader()
{
  Cleanup();
  mVideoPackets.Reset();
  mAudioPackets.Reset();
  MOZ_ASSERT(!mAudioDecoder);
  MOZ_ASSERT(!mVideoDecoder);
  MOZ_COUNT_DTOR(WebMReader);
}

nsRefPtr<ShutdownPromise>
WebMReader::Shutdown()
{
  if (mAudioDecoder) {
    mAudioDecoder->Shutdown();
    mAudioDecoder = nullptr;
  }

  if (mVideoDecoder) {
    mVideoDecoder->Shutdown();
    mVideoDecoder = nullptr;
  }

  return MediaDecoderReader::Shutdown();
}

nsresult WebMReader::Init(MediaDecoderReader* aCloneDonor)
{
  if (aCloneDonor) {
    mBufferedState = static_cast<WebMReader*>(aCloneDonor)->mBufferedState;
  } else {
    mBufferedState = new WebMBufferedState;
  }

  return NS_OK;
}

void WebMReader::InitLayersBackendType()
{
  if (!IsVideoContentType(GetDecoder()->GetResource()->GetContentType())) {
    // Not playing video, we don't care about the layers backend type.
    return;
  }
  // Extract the layer manager backend type so that platform decoders
  // can determine whether it's worthwhile using hardware accelerated
  // video decoding.
  MediaDecoderOwner* owner = mDecoder->GetOwner();
  if (!owner) {
    NS_WARNING("WebMReader without a decoder owner, can't get HWAccel");
    return;
  }

  dom::HTMLMediaElement* element = owner->GetMediaElement();
  NS_ENSURE_TRUE_VOID(element);

  nsRefPtr<LayerManager> layerManager =
    nsContentUtils::LayerManagerForDocument(element->OwnerDoc());
  NS_ENSURE_TRUE_VOID(layerManager);

  mLayersBackendType = layerManager->GetCompositorBackendType();
}

nsresult WebMReader::ResetDecode()
{
  mAudioFrames = 0;
  mAudioStartUsec = -1;
  nsresult res = NS_OK;
  if (NS_FAILED(MediaDecoderReader::ResetDecode())) {
    res = NS_ERROR_FAILURE;
  }

  if (mAudioDecoder) {
    mAudioDecoder->ResetDecode();
  }

  mVideoPackets.Reset();
  mAudioPackets.Reset();

  return res;
}

void WebMReader::Cleanup()
{
  if (mContext) {
    nestegg_destroy(mContext);
    mContext = nullptr;
  }
}

nsRefPtr<MediaDecoderReader::MetadataPromise>
WebMReader::AsyncReadMetadata()
{
  nsRefPtr<MetadataHolder> metadata = new MetadataHolder();

  if (NS_FAILED(RetrieveWebMMetadata(&metadata->mInfo)) ||
      !metadata->mInfo.HasValidMedia()) {
    return MetadataPromise::CreateAndReject(ReadMetadataFailureReason::METADATA_ERROR,
                                            __func__);
  }

  return MetadataPromise::CreateAndResolve(metadata, __func__);
}

nsresult
WebMReader::RetrieveWebMMetadata(MediaInfo* aInfo)
{
  // We can't use OnTaskQueue() here because of the wacky initialization task
  // queue that TrackBuffer uses. We should be able to fix this when we do
  // bug 1148234.
  MOZ_ASSERT(mDecoder->OnDecodeTaskQueue());

  nestegg_io io;
  io.read = webm_read;
  io.seek = webm_seek;
  io.tell = webm_tell;
  io.userdata = &mResource;
  int64_t maxOffset = mDecoder->HasInitializationData() ?
    mBufferedState->GetInitEndOffset() : -1;
  int r = nestegg_init(&mContext, io, &webm_log, maxOffset);
  if (r == -1) {
    return NS_ERROR_FAILURE;
  }

  uint64_t duration = 0;
  r = nestegg_duration(mContext, &duration);
  if (r == 0) {
    mInfo.mMetadataDuration.emplace(TimeUnit::FromNanoseconds(duration));
  }

  unsigned int ntracks = 0;
  r = nestegg_track_count(mContext, &ntracks);
  if (r == -1) {
    Cleanup();
    return NS_ERROR_FAILURE;
  }

  for (uint32_t track = 0; track < ntracks; ++track) {
    int id = nestegg_track_codec_id(mContext, track);
    if (id == -1) {
      Cleanup();
      return NS_ERROR_FAILURE;
    }
    int type = nestegg_track_type(mContext, track);
    if (!mHasVideo && type == NESTEGG_TRACK_VIDEO &&
        mDecoder->GetImageContainer()) {
      nestegg_video_params params;
      r = nestegg_track_video_params(mContext, track, &params);
      if (r == -1) {
        Cleanup();
        return NS_ERROR_FAILURE;
      }

      mVideoCodec = nestegg_track_codec_id(mContext, track);

      if (!mVideoDecoder) {
        mVideoDecoder = SoftwareWebMVideoDecoder::Create(this);
      }

      if (!mVideoDecoder ||
          NS_FAILED(mVideoDecoder->Init(params.display_width,
                                        params.display_height))) {
        Cleanup();
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
      mPicture = pictureRect;
      mInitialFrame = frameSize;

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
    } else if (!mHasAudio && type == NESTEGG_TRACK_AUDIO) {
      nestegg_audio_params params;
      r = nestegg_track_audio_params(mContext, track, &params);
      if (r == -1) {
        Cleanup();
        return NS_ERROR_FAILURE;
      }

      mAudioTrack = track;
      mHasAudio = true;
      mAudioCodec = nestegg_track_codec_id(mContext, track);
      mCodecDelay = params.codec_delay / NS_PER_USEC;
      mSeekPreroll = params.seek_preroll;

      if (mAudioCodec == NESTEGG_CODEC_VORBIS) {
        mAudioDecoder = new VorbisDecoder(this);
      } else if (mAudioCodec == NESTEGG_CODEC_OPUS) {
        mAudioDecoder = new OpusDecoder(this);
      } else {
        Cleanup();
        return NS_ERROR_FAILURE;
      }

      if (!mAudioDecoder || NS_FAILED(mAudioDecoder->Init())) {
        Cleanup();
        return NS_ERROR_FAILURE;
      }

      unsigned int nheaders = 0;
      r = nestegg_track_codec_data_count(mContext, track, &nheaders);
      if (r == -1) {
        Cleanup();
        return NS_ERROR_FAILURE;
      }

      for (uint32_t header = 0; header < nheaders; ++header) {
        unsigned char* data = 0;
        size_t length = 0;
        r = nestegg_track_codec_data(mContext, track, header, &data, &length);
        if (r == -1) {
          Cleanup();
          return NS_ERROR_FAILURE;
        }
        if (NS_FAILED(mAudioDecoder->DecodeHeader(data, length))) {
          Cleanup();
          return NS_ERROR_FAILURE;
        }
      }
      if (NS_FAILED(mAudioDecoder->FinishInit(mInfo.mAudio))) {
        Cleanup();
        return NS_ERROR_FAILURE;
      }
    }
  }

  *aInfo = mInfo;

  return NS_OK;
}

bool
WebMReader::IsMediaSeekable()
{
  return mContext && nestegg_has_cues(mContext);
}

bool WebMReader::DecodeAudioPacket(NesteggPacketHolder* aHolder)
{
  MOZ_ASSERT(OnTaskQueue());

  int r = 0;
  unsigned int count = 0;
  r = nestegg_packet_count(aHolder->Packet(), &count);
  if (r == -1) {
    return false;
  }

  int64_t tstamp = aHolder->Timestamp();
  if (mAudioStartUsec == -1) {
    // This is the first audio chunk. Assume the start time of our decode
    // is the start of this chunk.
    mAudioStartUsec = tstamp;
  }
  // If there's a gap between the start of this audio chunk and the end of
  // the previous audio chunk, we need to increment the packet count so that
  // the vorbis decode doesn't use data from before the gap to help decode
  // from after the gap.
  CheckedInt64 tstamp_frames = UsecsToFrames(tstamp, mInfo.mAudio.mRate);
  CheckedInt64 decoded_frames = UsecsToFrames(mAudioStartUsec,
                                              mInfo.mAudio.mRate);
  if (!tstamp_frames.isValid() || !decoded_frames.isValid()) {
    NS_WARNING("Int overflow converting WebM times to frames");
    return false;
  }
  decoded_frames += mAudioFrames;
  if (!decoded_frames.isValid()) {
    NS_WARNING("Int overflow adding decoded_frames");
    return false;
  }
  if (tstamp_frames.value() > decoded_frames.value()) {
#ifdef DEBUG
    int64_t gap_frames = tstamp_frames.value() - decoded_frames.value();
    CheckedInt64 usecs = FramesToUsecs(gap_frames, mInfo.mAudio.mRate);
    LOG(LogLevel::Debug, ("WebMReader detected gap of %lld, %lld frames, in audio",
                       usecs.isValid() ? usecs.value() : -1,
                       gap_frames));
#endif
    mAudioStartUsec = tstamp;
    mAudioFrames = 0;
  }

  int32_t total_frames = 0;
  for (uint32_t i = 0; i < count; ++i) {
    unsigned char* data;
    size_t length;
    r = nestegg_packet_data(aHolder->Packet(), i, &data, &length);
    if (r == -1) {
      return false;
    }
    int64_t discardPadding = 0;
    (void) nestegg_packet_discard_padding(aHolder->Packet(), &discardPadding);

    if (!mAudioDecoder->Decode(data, length, aHolder->Offset(), tstamp, discardPadding, &total_frames)) {
      mHitAudioDecodeError = true;
      return false;
    }
  }

  mAudioFrames += total_frames;

  return true;
}

nsRefPtr<NesteggPacketHolder> WebMReader::NextPacket(TrackType aTrackType)
{
  // The packet queue that packets will be pushed on if they
  // are not the type we are interested in.
  WebMPacketQueue& otherPackets =
    aTrackType == VIDEO ? mAudioPackets : mVideoPackets;

  // The packet queue for the type that we are interested in.
  WebMPacketQueue &packets =
    aTrackType == VIDEO ? mVideoPackets : mAudioPackets;

  // Flag to indicate that we do need to playback these types of
  // packets.
  bool hasType = aTrackType == VIDEO ? mHasVideo : mHasAudio;

  // Flag to indicate that we do need to playback the other type
  // of track.
  bool hasOtherType = aTrackType == VIDEO ? mHasAudio : mHasVideo;

  // Track we are interested in
  uint32_t ourTrack = aTrackType == VIDEO ? mVideoTrack : mAudioTrack;

  // Value of other track
  uint32_t otherTrack = aTrackType == VIDEO ? mAudioTrack : mVideoTrack;

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
WebMReader::DemuxPacket()
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

  // Figure out if this is a keyframe.
  bool isKeyframe = false;
  if (track == mAudioTrack) {
    isKeyframe = true;
  } else if (track == mVideoTrack) {
    unsigned char* data;
    size_t length;
    r = nestegg_packet_data(packet, 0, &data, &length);
    if (r == -1) {
      return nullptr;
    }
    vpx_codec_stream_info_t si;
    memset(&si, 0, sizeof(si));
    si.sz = sizeof(si);
    if (mVideoCodec == NESTEGG_CODEC_VP8) {
      vpx_codec_peek_stream_info(vpx_codec_vp8_dx(), data, length, &si);
    } else if (mVideoCodec == NESTEGG_CODEC_VP9) {
      vpx_codec_peek_stream_info(vpx_codec_vp9_dx(), data, length, &si);
    }
    isKeyframe = si.is_kf;
  }

  int64_t offset = mResource.Tell();
  nsRefPtr<NesteggPacketHolder> holder = new NesteggPacketHolder();
  if (!holder->Init(packet, offset, track, isKeyframe)) {
    return nullptr;
  }

  return holder;
}

bool WebMReader::DecodeAudioData()
{
  MOZ_ASSERT(OnTaskQueue());

  nsRefPtr<NesteggPacketHolder> holder(NextPacket(AUDIO));
  if (!holder) {
    return false;
  }

  return DecodeAudioPacket(holder);
}

bool WebMReader::FilterPacketByTime(int64_t aEndTime, WebMPacketQueue& aOutput)
{
  // Push the video frames to the aOutput which's timestamp is less
  // than aEndTime.
  while (true) {
    nsRefPtr<NesteggPacketHolder> holder(NextPacket(VIDEO));
    if (!holder) {
      break;
    }
    int64_t tstamp = holder->Timestamp();
    if (tstamp >= aEndTime) {
      PushVideoPacket(holder);
      return true;
    } else {
      aOutput.PushFront(holder);
    }
  }

  return false;
}

int64_t WebMReader::GetNextKeyframeTime(int64_t aTimeThreshold)
{
  WebMPacketQueue skipPacketQueue;
  if (!FilterPacketByTime(aTimeThreshold, skipPacketQueue)) {
    // Restore the packets before we return -1.
    uint32_t size = skipPacketQueue.GetSize();
    for (uint32_t i = 0; i < size; ++i) {
      nsRefPtr<NesteggPacketHolder> packetHolder = skipPacketQueue.PopFront();
      PushVideoPacket(packetHolder);
    }
    return -1;
  }

  // Find keyframe.
  bool foundKeyframe = false;
  int64_t keyframeTime = -1;
  while (!foundKeyframe) {
    nsRefPtr<NesteggPacketHolder> holder(NextPacket(VIDEO));
    if (!holder) {
      break;
    }

    if (holder->IsKeyframe()) {
      foundKeyframe = true;
      keyframeTime = holder->Timestamp();
    }

    skipPacketQueue.PushFront(holder);
  }

  uint32_t size = skipPacketQueue.GetSize();
  for (uint32_t i = 0; i < size; ++i) {
    nsRefPtr<NesteggPacketHolder> packetHolder = skipPacketQueue.PopFront();
    PushVideoPacket(packetHolder);
  }

  return keyframeTime;
}

bool WebMReader::ShouldSkipVideoFrame(int64_t aTimeThreshold)
{
  return GetNextKeyframeTime(aTimeThreshold) != -1;
}

bool WebMReader::DecodeVideoFrame(bool &aKeyframeSkip, int64_t aTimeThreshold)
{
  if (!(aKeyframeSkip && ShouldSkipVideoFrame(aTimeThreshold))) {
    LOG(LogLevel::Verbose, ("Reader [%p]: set the aKeyframeSkip to false.",this));
    aKeyframeSkip = false;
  }
  return mVideoDecoder->DecodeVideoFrame(aKeyframeSkip, aTimeThreshold);
}

void WebMReader::PushVideoPacket(NesteggPacketHolder* aItem)
{
    mVideoPackets.PushFront(aItem);
}

nsRefPtr<MediaDecoderReader::SeekPromise>
WebMReader::Seek(int64_t aTarget, int64_t aEndTime)
{
  nsresult res = SeekInternal(aTarget);
  if (NS_FAILED(res)) {
    return SeekPromise::CreateAndReject(res, __func__);
  } else {
    return SeekPromise::CreateAndResolve(aTarget, __func__);
  }
}

nsresult WebMReader::SeekInternal(int64_t aTarget)
{
  MOZ_ASSERT(OnTaskQueue());
  NS_ENSURE_TRUE(HaveStartTime(), NS_ERROR_FAILURE);
  if (mVideoDecoder) {
    nsresult rv = mVideoDecoder->Flush();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  LOG(LogLevel::Debug, ("Reader [%p] for Decoder [%p]: About to seek to %fs",
                     this, mDecoder, double(aTarget) / USECS_PER_S));
  if (NS_FAILED(ResetDecode())) {
    return NS_ERROR_FAILURE;
  }
  uint32_t trackToSeek = mHasVideo ? mVideoTrack : mAudioTrack;
  uint64_t target = aTarget * NS_PER_USEC;

  if (mSeekPreroll) {
    uint64_t startTime = uint64_t(StartTime()) * NS_PER_USEC;
    if (target < mSeekPreroll || target - mSeekPreroll < startTime) {
      target = startTime;
    } else {
      target -= mSeekPreroll;
    }
    LOG(LogLevel::Debug,
        ("Reader [%p] SeekPreroll: %f StartTime: %f AdjustedTarget: %f",
        this, double(mSeekPreroll) / NS_PER_S,
        double(startTime) / NS_PER_S, double(target) / NS_PER_S));
  }
  int r = nestegg_track_seek(mContext, trackToSeek, target);
  if (r != 0) {
    LOG(LogLevel::Debug, ("Reader [%p]: track_seek for track %u failed, r=%d",
                       this, trackToSeek, r));

    // Try seeking directly based on cluster information in memory.
    int64_t offset = 0;
    bool rv = mBufferedState->GetOffsetForTime(target, &offset);
    if (!rv) {
      return NS_ERROR_FAILURE;
    }

    r = nestegg_offset_seek(mContext, offset);
    LOG(LogLevel::Debug, ("Reader [%p]: attempted offset_seek to %lld r=%d",
                       this, offset, r));
    if (r != 0) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

media::TimeIntervals WebMReader::GetBuffered()
{
  MOZ_ASSERT(OnTaskQueue());
  if (!HaveStartTime()) {
    return media::TimeIntervals();
  }
  AutoPinned<MediaResource> resource(mDecoder->GetResource());

  media::TimeIntervals buffered;
  // Special case completely cached files.  This also handles local files.
  if (mContext && resource->IsDataCachedToEndOfResource(0)) {
    uint64_t duration = 0;
    if (nestegg_duration(mContext, &duration) == 0) {
      buffered +=
        media::TimeInterval(media::TimeUnit::FromSeconds(0),
                            media::TimeUnit::FromSeconds(duration / NS_PER_S));
      return buffered;
    }
  }

  // Either we the file is not fully cached, or we couldn't find a duration in
  // the WebM bitstream.
  nsTArray<MediaByteRange> ranges;
  nsresult res = resource->GetCachedRanges(ranges);
  NS_ENSURE_SUCCESS(res, media::TimeIntervals::Invalid());

  for (uint32_t index = 0; index < ranges.Length(); index++) {
    uint64_t start, end;
    bool rv = mBufferedState->CalculateBufferedForRange(ranges[index].mStart,
                                                        ranges[index].mEnd,
                                                        &start, &end);
    if (rv) {
      int64_t startOffset = StartTime() * NS_PER_USEC;
      NS_ASSERTION(startOffset >= 0 && uint64_t(startOffset) <= start,
                   "startOffset negative or larger than start time");
      if (!(startOffset >= 0 && uint64_t(startOffset) <= start)) {
        startOffset = 0;
      }
      double startTime = (start - startOffset) / NS_PER_S;
      double endTime = (end - startOffset) / NS_PER_S;
      // If this range extends to the end of the file, the true end time
      // is the file's duration.
      if (mContext &&
          resource->IsDataCachedToEndOfResource(ranges[index].mStart)) {
        uint64_t duration = 0;
        if (nestegg_duration(mContext, &duration) == 0) {
          endTime = duration / NS_PER_S;
        }
      }
      buffered += media::TimeInterval(media::TimeUnit::FromSeconds(startTime),
                                      media::TimeUnit::FromSeconds(endTime));
    }
  }

  return buffered;
}

void WebMReader::NotifyDataArrivedInternal(uint32_t aLength, int64_t aOffset)
{
  MOZ_ASSERT(OnTaskQueue());
  nsRefPtr<MediaByteBuffer> bytes =
    mDecoder->GetResource()->MediaReadAt(aOffset, aLength);
  NS_ENSURE_TRUE_VOID(bytes);
  mBufferedState->NotifyDataArrived(bytes->Elements(), aLength, aOffset);
}

int WebMReader::GetVideoCodec()
{
  return mVideoCodec;
}

nsIntRect WebMReader::GetPicture()
{
  return mPicture;
}

nsIntSize WebMReader::GetInitialFrame()
{
  return mInitialFrame;
}

int64_t WebMReader::GetLastVideoFrameTime()
{
  return mLastVideoFrameTime;
}

void WebMReader::SetLastVideoFrameTime(int64_t aFrameTime)
{
  mLastVideoFrameTime = aFrameTime;
}

} // namespace mozilla
