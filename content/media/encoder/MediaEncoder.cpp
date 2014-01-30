/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaEncoder.h"
#include "MediaDecoder.h"
#include "nsIPrincipal.h"
#include "nsMimeTypes.h"
#include "prlog.h"
#include "mozilla/Preferences.h"

#ifdef MOZ_OGG
#include "OggWriter.h"
#endif
#ifdef MOZ_OPUS
#include "OpusTrackEncoder.h"

#endif

#ifdef MOZ_VORBIS
#include "VorbisTrackEncoder.h"
#endif
#ifdef MOZ_WEBM_ENCODER
#include "VorbisTrackEncoder.h"
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

#ifdef PR_LOGGING
PRLogModuleInfo* gMediaEncoderLog;
#define LOG(type, msg) PR_LOG(gMediaEncoderLog, type, msg)
#else
#define LOG(type, msg)
#endif

namespace mozilla {

void
MediaEncoder::NotifyQueuedTrackChanges(MediaStreamGraph* aGraph,
                                       TrackID aID,
                                       TrackRate aTrackRate,
                                       TrackTicks aTrackOffset,
                                       uint32_t aTrackEvents,
                                       const MediaSegment& aQueuedMedia)
{
  // Process the incoming raw track data from MediaStreamGraph, called on the
  // thread of MediaStreamGraph.
  if (mAudioEncoder && aQueuedMedia.GetType() == MediaSegment::AUDIO) {
    mAudioEncoder->NotifyQueuedTrackChanges(aGraph, aID, aTrackRate,
                                            aTrackOffset, aTrackEvents,
                                            aQueuedMedia);

  } else if (mVideoEncoder && aQueuedMedia.GetType() == MediaSegment::VIDEO) {
      mVideoEncoder->NotifyQueuedTrackChanges(aGraph, aID, aTrackRate,
                                              aTrackOffset, aTrackEvents,
                                              aQueuedMedia);
  }
}

void
MediaEncoder::NotifyRemoved(MediaStreamGraph* aGraph)
{
  // In case that MediaEncoder does not receive a TRACK_EVENT_ENDED event.
  LOG(PR_LOG_DEBUG, ("NotifyRemoved in [MediaEncoder]."));
  if (mAudioEncoder) {
    mAudioEncoder->NotifyRemoved(aGraph);
  }
  if (mVideoEncoder) {
    mVideoEncoder->NotifyRemoved(aGraph);
  }

}

/* static */
already_AddRefed<MediaEncoder>
MediaEncoder::CreateEncoder(const nsAString& aMIMEType, uint8_t aTrackTypes)
{
#ifdef PR_LOGGING
  if (!gMediaEncoderLog) {
    gMediaEncoderLog = PR_NewLogModule("MediaEncoder");
  }
#endif
  nsAutoPtr<ContainerWriter> writer;
  nsAutoPtr<AudioTrackEncoder> audioEncoder;
  nsAutoPtr<VideoTrackEncoder> videoEncoder;
  nsRefPtr<MediaEncoder> encoder;
  nsString mimeType;
  if (!aTrackTypes) {
    LOG(PR_LOG_ERROR, ("NO TrackTypes!!!"));
    return nullptr;
  }
#ifdef MOZ_WEBM_ENCODER
  else if (MediaEncoder::IsWebMEncoderEnabled() &&
          (aMIMEType.EqualsLiteral(VIDEO_WEBM) ||
          (aTrackTypes & ContainerWriter::CREATE_VIDEO_TRACK))) {
    if (aTrackTypes & ContainerWriter::CREATE_AUDIO_TRACK) {
      audioEncoder = new VorbisTrackEncoder();
      NS_ENSURE_TRUE(audioEncoder, nullptr);
    }
    videoEncoder = new VP8TrackEncoder();
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
      audioEncoder = new OmxAudioTrackEncoder();
      NS_ENSURE_TRUE(audioEncoder, nullptr);
    }
    videoEncoder = new OmxVideoTrackEncoder();
    writer = new ISOMediaWriter(aTrackTypes);
    NS_ENSURE_TRUE(writer, nullptr);
    NS_ENSURE_TRUE(videoEncoder, nullptr);
    mimeType = NS_LITERAL_STRING(VIDEO_MP4);
  }
#endif // MOZ_OMX_ENCODER
#ifdef MOZ_OGG
  else if (MediaDecoder::IsOggEnabled() && MediaDecoder::IsOpusEnabled() &&
           (aMIMEType.EqualsLiteral(AUDIO_OGG) ||
           (aTrackTypes & ContainerWriter::CREATE_AUDIO_TRACK))) {
    writer = new OggWriter();
    audioEncoder = new OpusTrackEncoder();
    NS_ENSURE_TRUE(writer, nullptr);
    NS_ENSURE_TRUE(audioEncoder, nullptr);
    mimeType = NS_LITERAL_STRING(AUDIO_OGG);
  }
#endif  // MOZ_OGG
  else {
    LOG(PR_LOG_ERROR, ("Can not find any encoder to record this media stream"));
    return nullptr;
  }
  LOG(PR_LOG_DEBUG, ("Create encoder result:a[%d] v[%d] w[%d] mimeType = %s.",
                      audioEncoder != nullptr, videoEncoder != nullptr,
                      writer != nullptr, mimeType.get()));
  encoder = new MediaEncoder(writer.forget(), audioEncoder.forget(),
                             videoEncoder.forget(), mimeType);
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

  bool reloop = true;
  while (reloop) {
    switch (mState) {
    case ENCODE_METADDATA: {
      LOG(PR_LOG_DEBUG, ("ENCODE_METADDATA TimeStamp = %f", GetEncodeTimeStamp()));
      nsresult rv = CopyMetadataToMuxer(mAudioEncoder.get());
      if (NS_FAILED(rv)) {
        LOG(PR_LOG_ERROR, ("Error! Fail to Set Audio Metadata"));
        break;
      }
      rv = CopyMetadataToMuxer(mVideoEncoder.get());
      if (NS_FAILED(rv)) {
        LOG(PR_LOG_ERROR, ("Error! Fail to Set Video Metadata"));
        break;
      }

      rv = mWriter->GetContainerData(aOutputBufs,
                                     ContainerWriter::GET_HEADER);
      if (NS_FAILED(rv)) {
       LOG(PR_LOG_ERROR,("Error! writer fail to generate header!"));
       mState = ENCODE_ERROR;
       break;
      }
      LOG(PR_LOG_DEBUG, ("Finish ENCODE_METADDATA TimeStamp = %f", GetEncodeTimeStamp()));
      mState = ENCODE_TRACK;
      break;
    }

    case ENCODE_TRACK: {
      LOG(PR_LOG_DEBUG, ("ENCODE_TRACK TimeStamp = %f", GetEncodeTimeStamp()));
      EncodedFrameContainer encodedData;
      nsresult rv = NS_OK;
      rv = WriteEncodedDataToMuxer(mAudioEncoder.get());
      if (NS_FAILED(rv)) {
        LOG(PR_LOG_ERROR, ("Error! Fail to write audio encoder data to muxer"));
        break;
      }
      LOG(PR_LOG_DEBUG, ("Audio encoded TimeStamp = %f", GetEncodeTimeStamp()));
      rv = WriteEncodedDataToMuxer(mVideoEncoder.get());
      if (NS_FAILED(rv)) {
        LOG(PR_LOG_ERROR, ("Fail to write video encoder data to muxer"));
        break;
      }
      LOG(PR_LOG_DEBUG, ("Video encoded TimeStamp = %f", GetEncodeTimeStamp()));
      // In audio only or video only case, let unavailable track's flag to be true.
      bool isAudioCompleted = (mAudioEncoder && mAudioEncoder->IsEncodingComplete()) || !mAudioEncoder;
      bool isVideoCompleted = (mVideoEncoder && mVideoEncoder->IsEncodingComplete()) || !mVideoEncoder;
      rv = mWriter->GetContainerData(aOutputBufs,
                                     isAudioCompleted && isVideoCompleted ?
                                     ContainerWriter::FLUSH_NEEDED : 0);
      if (NS_SUCCEEDED(rv)) {
        // Successfully get the copy of final container data from writer.
        reloop = false;
      }
      mState = (mWriter->IsWritingComplete()) ? ENCODE_DONE : ENCODE_TRACK;
      LOG(PR_LOG_DEBUG, ("END ENCODE_TRACK TimeStamp = %f "
          "mState = %d aComplete %d vComplete %d",
          GetEncodeTimeStamp(), mState, isAudioCompleted, isVideoCompleted));
      break;
    }

    case ENCODE_DONE:
    case ENCODE_ERROR:
      LOG(PR_LOG_DEBUG, ("MediaEncoder has been shutdown."));
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
  EncodedFrameContainer encodedVideoData;
  nsresult rv = aTrackEncoder->GetEncodedTrack(encodedVideoData);
  if (NS_FAILED(rv)) {
    // Encoding might be canceled.
    LOG(PR_LOG_ERROR, ("Error! Fail to get encoded data from video encoder."));
    mState = ENCODE_ERROR;
    return rv;
  }
  rv = mWriter->WriteEncodedTrack(encodedVideoData,
                                  aTrackEncoder->IsEncodingComplete() ?
                                  ContainerWriter::END_OF_STREAM : 0);
  if (NS_FAILED(rv)) {
    LOG(PR_LOG_ERROR, ("Error! Fail to write encoded video track to the media container."));
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
  nsRefPtr<TrackMetadataBase> meta = aTrackEncoder->GetMetadata();
  if (meta == nullptr) {
    LOG(PR_LOG_ERROR, ("Error! metadata = null"));
    mState = ENCODE_ERROR;
    return NS_ERROR_ABORT;
  }

  nsresult rv = mWriter->SetMetadata(meta);
  if (NS_FAILED(rv)) {
   LOG(PR_LOG_ERROR, ("Error! SetMetadata fail"));
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

}
