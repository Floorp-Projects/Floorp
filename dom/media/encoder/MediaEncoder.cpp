/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaEncoder.h"
#include "MediaDecoder.h"
#include "nsIPrincipal.h"
#include "nsMimeTypes.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/gfx/Point.h" // IntSize

#include"GeckoProfiler.h"
#include "OggWriter.h"
#include "OpusTrackEncoder.h"

#ifdef MOZ_WEBM_ENCODER
#include "VP8TrackEncoder.h"
#include "WebMWriter.h"
#endif
#ifdef MOZ_OMX_ENCODER
#include "OmxTrackEncoder.h"
#include "ISOMediaWriter.h"
#endif

#ifdef LOG
#undef LOG
#endif

mozilla::LazyLogModule gMediaEncoderLog("MediaEncoder");
#define LOG(type, msg) MOZ_LOG(gMediaEncoderLog, type, msg)

namespace mozilla {

void
MediaStreamVideoRecorderSink::SetCurrentFrames(const VideoSegment& aSegment)
{
  MOZ_ASSERT(mVideoEncoder);
  mVideoEncoder->SetCurrentFrames(aSegment);
}

void
MediaEncoder::SetDirectConnect(bool aConnected)
{
  mDirectConnected = aConnected;
}

void
MediaEncoder::NotifyRealtimeData(MediaStreamGraph* aGraph,
                                 TrackID aID,
                                 StreamTime aTrackOffset,
                                 uint32_t aTrackEvents,
                                 const MediaSegment& aRealtimeMedia)
{
  if (mSuspended == RECORD_NOT_SUSPENDED) {
    // Process the incoming raw track data from MediaStreamGraph, called on the
    // thread of MediaStreamGraph.
    if (mAudioEncoder && aRealtimeMedia.GetType() == MediaSegment::AUDIO) {
      mAudioEncoder->NotifyQueuedTrackChanges(aGraph, aID,
                                              aTrackOffset, aTrackEvents,
                                              aRealtimeMedia);
    } else if (mVideoEncoder &&
               aRealtimeMedia.GetType() == MediaSegment::VIDEO &&
               aTrackEvents != TrackEventCommand::TRACK_EVENT_NONE) {
      mVideoEncoder->NotifyQueuedTrackChanges(aGraph, aID,
                                              aTrackOffset, aTrackEvents,
                                              aRealtimeMedia);
    }
  }
}

void
MediaEncoder::NotifyQueuedTrackChanges(MediaStreamGraph* aGraph,
                                       TrackID aID,
                                       StreamTime aTrackOffset,
                                       TrackEventCommand aTrackEvents,
                                       const MediaSegment& aQueuedMedia,
                                       MediaStream* aInputStream,
                                       TrackID aInputTrackID)
{
  if (!mDirectConnected) {
    NotifyRealtimeData(aGraph, aID, aTrackOffset, aTrackEvents, aQueuedMedia);
  } else {
    if (aTrackEvents != TrackEventCommand::TRACK_EVENT_NONE) {
      // forward events (TRACK_EVENT_ENDED) but not the media
      if (aQueuedMedia.GetType() == MediaSegment::VIDEO) {
        VideoSegment segment;
        NotifyRealtimeData(aGraph, aID, aTrackOffset, aTrackEvents, segment);
      } else {
        AudioSegment segment;
        NotifyRealtimeData(aGraph, aID, aTrackOffset, aTrackEvents, segment);
      }
    }
    if (mSuspended == RECORD_RESUMED) {
      if (mVideoEncoder) {
        if (aQueuedMedia.GetType() == MediaSegment::VIDEO) {
          // insert a null frame of duration equal to the first segment passed
          // after Resume(), so it'll get added to one of the DirectListener frames
          VideoSegment segment;
          gfx::IntSize size(0,0);
          segment.AppendFrame(nullptr, aQueuedMedia.GetDuration(), size,
                              PRINCIPAL_HANDLE_NONE);
          mVideoEncoder->NotifyQueuedTrackChanges(aGraph, aID,
                                                  aTrackOffset, aTrackEvents,
                                                  segment);
          mSuspended = RECORD_NOT_SUSPENDED;
        }
      } else {
        mSuspended = RECORD_NOT_SUSPENDED; // no video
      }
    }
  }
}

void
MediaEncoder::NotifyQueuedAudioData(MediaStreamGraph* aGraph, TrackID aID,
                                    StreamTime aTrackOffset,
                                    const AudioSegment& aQueuedMedia,
                                    MediaStream* aInputStream,
                                    TrackID aInputTrackID)
{
  if (!mDirectConnected) {
    NotifyRealtimeData(aGraph, aID, aTrackOffset, 0, aQueuedMedia);
  } else {
    if (mSuspended == RECORD_RESUMED) {
      if (!mVideoEncoder) {
        mSuspended = RECORD_NOT_SUSPENDED; // no video
      }
    }
  }
}

void
MediaEncoder::NotifyEvent(MediaStreamGraph* aGraph,
                          MediaStreamGraphEvent event)
{
  // In case that MediaEncoder does not receive a TRACK_EVENT_ENDED event.
  LOG(LogLevel::Debug, ("NotifyRemoved in [MediaEncoder]."));
  if (mAudioEncoder) {
    mAudioEncoder->NotifyEvent(aGraph, event);
  }
  if (mVideoEncoder) {
    mVideoEncoder->NotifyEvent(aGraph, event);
  }
}

/* static */
already_AddRefed<MediaEncoder>
MediaEncoder::CreateEncoder(const nsAString& aMIMEType, uint32_t aAudioBitrate,
                            uint32_t aVideoBitrate, uint32_t aBitrate,
                            uint8_t aTrackTypes,
                            TrackRate aTrackRate)
{
  PROFILER_LABEL("MediaEncoder", "CreateEncoder",
    js::ProfileEntry::Category::OTHER);

  nsAutoPtr<ContainerWriter> writer;
  nsAutoPtr<AudioTrackEncoder> audioEncoder;
  nsAutoPtr<VideoTrackEncoder> videoEncoder;
  RefPtr<MediaEncoder> encoder;
  nsString mimeType;
  if (!aTrackTypes) {
    LOG(LogLevel::Error, ("NO TrackTypes!!!"));
    return nullptr;
  }
#ifdef MOZ_WEBM_ENCODER
  else if (MediaEncoder::IsWebMEncoderEnabled() &&
          (aMIMEType.EqualsLiteral(VIDEO_WEBM) ||
          (aTrackTypes & ContainerWriter::CREATE_VIDEO_TRACK))) {
    if (aTrackTypes & ContainerWriter::CREATE_AUDIO_TRACK
        && MediaDecoder::IsOpusEnabled()) {
      audioEncoder = new OpusTrackEncoder();
      NS_ENSURE_TRUE(audioEncoder, nullptr);
    }
    videoEncoder = new VP8TrackEncoder(aTrackRate);
    writer = new WebMWriter(aTrackTypes);
    NS_ENSURE_TRUE(writer, nullptr);
    NS_ENSURE_TRUE(videoEncoder, nullptr);
    mimeType = NS_LITERAL_STRING(VIDEO_WEBM);
  }
#endif //MOZ_WEBM_ENCODER
#ifdef MOZ_OMX_ENCODER
  else if (MediaEncoder::IsOMXEncoderEnabled() &&
          (aMIMEType.EqualsLiteral(VIDEO_MP4) ||
          (aTrackTypes & ContainerWriter::CREATE_VIDEO_TRACK))) {
    if (aTrackTypes & ContainerWriter::CREATE_AUDIO_TRACK) {
      audioEncoder = new OmxAACAudioTrackEncoder();
      NS_ENSURE_TRUE(audioEncoder, nullptr);
    }
    videoEncoder = new OmxVideoTrackEncoder(aTrackRate);
    writer = new ISOMediaWriter(aTrackTypes);
    NS_ENSURE_TRUE(writer, nullptr);
    NS_ENSURE_TRUE(videoEncoder, nullptr);
    mimeType = NS_LITERAL_STRING(VIDEO_MP4);
  } else if (MediaEncoder::IsOMXEncoderEnabled() &&
            (aMIMEType.EqualsLiteral(AUDIO_3GPP))) {
    audioEncoder = new OmxAMRAudioTrackEncoder();
    NS_ENSURE_TRUE(audioEncoder, nullptr);

    writer = new ISOMediaWriter(aTrackTypes, ISOMediaWriter::TYPE_FRAG_3GP);
    NS_ENSURE_TRUE(writer, nullptr);
    mimeType = NS_LITERAL_STRING(AUDIO_3GPP);
  } else if (MediaEncoder::IsOMXEncoderEnabled() &&
            (aMIMEType.EqualsLiteral(AUDIO_3GPP2))) {
    audioEncoder = new OmxEVRCAudioTrackEncoder();
    NS_ENSURE_TRUE(audioEncoder, nullptr);

    writer = new ISOMediaWriter(aTrackTypes, ISOMediaWriter::TYPE_FRAG_3G2);
    NS_ENSURE_TRUE(writer, nullptr);
    mimeType = NS_LITERAL_STRING(AUDIO_3GPP2) ;
  }
#endif // MOZ_OMX_ENCODER
  else if (MediaDecoder::IsOggEnabled() && MediaDecoder::IsOpusEnabled() &&
           (aMIMEType.EqualsLiteral(AUDIO_OGG) ||
           (aTrackTypes & ContainerWriter::CREATE_AUDIO_TRACK))) {
    writer = new OggWriter();
    audioEncoder = new OpusTrackEncoder();
    NS_ENSURE_TRUE(writer, nullptr);
    NS_ENSURE_TRUE(audioEncoder, nullptr);
    mimeType = NS_LITERAL_STRING(AUDIO_OGG);
  }
  else {
    LOG(LogLevel::Error, ("Can not find any encoder to record this media stream"));
    return nullptr;
  }
  LOG(LogLevel::Debug, ("Create encoder result:a[%d] v[%d] w[%d] mimeType = %s.",
                      audioEncoder != nullptr, videoEncoder != nullptr,
                      writer != nullptr, mimeType.get()));
  if (videoEncoder && aVideoBitrate != 0) {
    videoEncoder->SetBitrate(aVideoBitrate);
  }
  if (audioEncoder && aAudioBitrate != 0) {
    audioEncoder->SetBitrate(aAudioBitrate);
  }
  encoder = new MediaEncoder(writer.forget(), audioEncoder.forget(),
                             videoEncoder.forget(), mimeType, aAudioBitrate,
                             aVideoBitrate, aBitrate);
  return encoder.forget();
}

/**
 * GetEncodedData() runs as a state machine, starting with mState set to
 * GET_METADDATA, the procedure should be as follow:
 *
 * While non-stop
 *   If mState is GET_METADDATA
 *     Get the meta data from audio/video encoder
 *     If a meta data is generated
 *       Get meta data from audio/video encoder
 *       Set mState to ENCODE_TRACK
 *       Return the final container data
 *
 *   If mState is ENCODE_TRACK
 *     Get encoded track data from audio/video encoder
 *     If a packet of track data is generated
 *       Insert encoded track data into the container stream of writer
 *       If the final container data is copied to aOutput
 *         Return the copy of final container data
 *       If this is the last packet of input stream
 *         Set mState to ENCODE_DONE
 *
 *   If mState is ENCODE_DONE or ENCODE_ERROR
 *     Stop the loop
 */
void
MediaEncoder::GetEncodedData(nsTArray<nsTArray<uint8_t> >* aOutputBufs,
                             nsAString& aMIMEType)
{
  MOZ_ASSERT(!NS_IsMainThread());

  aMIMEType = mMIMEType;
  PROFILER_LABEL("MediaEncoder", "GetEncodedData",
    js::ProfileEntry::Category::OTHER);

  bool reloop = true;
  while (reloop) {
    switch (mState) {
    case ENCODE_METADDATA: {
      LOG(LogLevel::Debug, ("ENCODE_METADDATA TimeStamp = %f", GetEncodeTimeStamp()));
      nsresult rv = CopyMetadataToMuxer(mAudioEncoder.get());
      if (NS_FAILED(rv)) {
        LOG(LogLevel::Error, ("Error! Fail to Set Audio Metadata"));
        break;
      }
      rv = CopyMetadataToMuxer(mVideoEncoder.get());
      if (NS_FAILED(rv)) {
        LOG(LogLevel::Error, ("Error! Fail to Set Video Metadata"));
        break;
      }

      rv = mWriter->GetContainerData(aOutputBufs,
                                     ContainerWriter::GET_HEADER);
      if (aOutputBufs != nullptr) {
        mSizeOfBuffer = aOutputBufs->ShallowSizeOfExcludingThis(MallocSizeOf);
      }
      if (NS_FAILED(rv)) {
       LOG(LogLevel::Error,("Error! writer fail to generate header!"));
       mState = ENCODE_ERROR;
       break;
      }
      LOG(LogLevel::Debug, ("Finish ENCODE_METADDATA TimeStamp = %f", GetEncodeTimeStamp()));
      mState = ENCODE_TRACK;
      break;
    }

    case ENCODE_TRACK: {
      LOG(LogLevel::Debug, ("ENCODE_TRACK TimeStamp = %f", GetEncodeTimeStamp()));
      EncodedFrameContainer encodedData;
      nsresult rv = NS_OK;
      // We're most likely to actually wait for a video frame, so do that first to minimize
      // capture offset/lipsync issues
      rv = WriteEncodedDataToMuxer(mVideoEncoder.get());
      if (NS_FAILED(rv)) {
        LOG(LogLevel::Error, ("Fail to write video encoder data to muxer"));
        break;
      }
      rv = WriteEncodedDataToMuxer(mAudioEncoder.get());
      if (NS_FAILED(rv)) {
        LOG(LogLevel::Error, ("Error! Fail to write audio encoder data to muxer"));
        break;
      }
      LOG(LogLevel::Debug, ("Audio encoded TimeStamp = %f", GetEncodeTimeStamp()));
      LOG(LogLevel::Debug, ("Video encoded TimeStamp = %f", GetEncodeTimeStamp()));
      // In audio only or video only case, let unavailable track's flag to be true.
      bool isAudioCompleted = (mAudioEncoder && mAudioEncoder->IsEncodingComplete()) || !mAudioEncoder;
      bool isVideoCompleted = (mVideoEncoder && mVideoEncoder->IsEncodingComplete()) || !mVideoEncoder;
      rv = mWriter->GetContainerData(aOutputBufs,
                                     isAudioCompleted && isVideoCompleted ?
                                     ContainerWriter::FLUSH_NEEDED : 0);
      if (aOutputBufs != nullptr) {
        mSizeOfBuffer = aOutputBufs->ShallowSizeOfExcludingThis(MallocSizeOf);
      }
      if (NS_SUCCEEDED(rv)) {
        // Successfully get the copy of final container data from writer.
        reloop = false;
      }
      mState = (mWriter->IsWritingComplete()) ? ENCODE_DONE : ENCODE_TRACK;
      LOG(LogLevel::Debug, ("END ENCODE_TRACK TimeStamp = %f "
          "mState = %d aComplete %d vComplete %d",
          GetEncodeTimeStamp(), mState, isAudioCompleted, isVideoCompleted));
      break;
    }

    case ENCODE_DONE:
    case ENCODE_ERROR:
      LOG(LogLevel::Debug, ("MediaEncoder has been shutdown."));
      mSizeOfBuffer = 0;
      mShutdown = true;
      reloop = false;
      break;
    default:
      MOZ_CRASH("Invalid encode state");
    }
  }
}

nsresult
MediaEncoder::WriteEncodedDataToMuxer(TrackEncoder *aTrackEncoder)
{
  if (aTrackEncoder == nullptr) {
    return NS_OK;
  }
  if (aTrackEncoder->IsEncodingComplete()) {
    return NS_OK;
  }

  PROFILER_LABEL("MediaEncoder", "WriteEncodedDataToMuxer",
    js::ProfileEntry::Category::OTHER);

  EncodedFrameContainer encodedVideoData;
  nsresult rv = aTrackEncoder->GetEncodedTrack(encodedVideoData);
  if (NS_FAILED(rv)) {
    // Encoding might be canceled.
    LOG(LogLevel::Error, ("Error! Fail to get encoded data from video encoder."));
    mState = ENCODE_ERROR;
    return rv;
  }
  rv = mWriter->WriteEncodedTrack(encodedVideoData,
                                  aTrackEncoder->IsEncodingComplete() ?
                                  ContainerWriter::END_OF_STREAM : 0);
  if (NS_FAILED(rv)) {
    LOG(LogLevel::Error, ("Error! Fail to write encoded video track to the media container."));
    mState = ENCODE_ERROR;
  }
  return rv;
}

nsresult
MediaEncoder::CopyMetadataToMuxer(TrackEncoder *aTrackEncoder)
{
  if (aTrackEncoder == nullptr) {
    return NS_OK;
  }

  PROFILER_LABEL("MediaEncoder", "CopyMetadataToMuxer",
    js::ProfileEntry::Category::OTHER);

  RefPtr<TrackMetadataBase> meta = aTrackEncoder->GetMetadata();
  if (meta == nullptr) {
    LOG(LogLevel::Error, ("Error! metadata = null"));
    mState = ENCODE_ERROR;
    return NS_ERROR_ABORT;
  }

  nsresult rv = mWriter->SetMetadata(meta);
  if (NS_FAILED(rv)) {
   LOG(LogLevel::Error, ("Error! SetMetadata fail"));
   mState = ENCODE_ERROR;
  }
  return rv;
}

#ifdef MOZ_WEBM_ENCODER
bool
MediaEncoder::IsWebMEncoderEnabled()
{
  return Preferences::GetBool("media.encoder.webm.enabled");
}
#endif

#ifdef MOZ_OMX_ENCODER
bool
MediaEncoder::IsOMXEncoderEnabled()
{
  return Preferences::GetBool("media.encoder.omx.enabled");
}
#endif

/*
 * SizeOfExcludingThis measures memory being used by the Media Encoder.
 * Currently it measures the size of the Encoder buffer and memory occupied
 * by mAudioEncoder and mVideoEncoder.
 */
size_t
MediaEncoder::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t amount = 0;
  if (mState == ENCODE_TRACK) {
    amount = mSizeOfBuffer +
             (mAudioEncoder != nullptr ? mAudioEncoder->SizeOfExcludingThis(aMallocSizeOf) : 0) +
             (mVideoEncoder != nullptr ? mVideoEncoder->SizeOfExcludingThis(aMallocSizeOf) : 0);
  }
  return amount;
}

} // namespace mozilla
