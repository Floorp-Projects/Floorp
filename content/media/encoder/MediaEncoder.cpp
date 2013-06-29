/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaEncoder.h"
#include "MediaDecoder.h"
#ifdef MOZ_OGG
#include "OggWriter.h"
#endif
#ifdef MOZ_OPUS
#include "OpusTrackEncoder.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "MediaEncoder", ## args);
#else
#define LOG(args,...)
#endif

namespace mozilla {

#define TRACK_BUFFER_LEN 8192

namespace {

template <class String>
static bool
TypeListContains(char const *const * aTypes, const String& aType)
{
  for (int32_t i = 0; aTypes[i]; ++i) {
    if (aType.EqualsASCII(aTypes[i]))
      return true;
  }
  return false;
}

#ifdef MOZ_OGG
// The recommended mime-type for Ogg Opus files is audio/ogg.
// See http://wiki.xiph.org/OggOpus for more details.
static const char* const gOggTypes[2] = {
  "audio/ogg",
  nullptr
};

static bool
IsOggType(const nsAString& aType)
{
  if (!MediaDecoder::IsOggEnabled()) {
    return false;
  }

  return TypeListContains(gOggTypes, aType);
}
#endif
} //anonymous namespace

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
  if (aQueuedMedia.GetType() == MediaSegment::AUDIO) {
    mAudioEncoder->NotifyQueuedTrackChanges(aGraph, aID, aTrackRate,
                                            aTrackOffset, aTrackEvents,
                                            aQueuedMedia);

  } else {
    // Type video is not supported for now.
  }
}

void
MediaEncoder::NotifyRemoved(MediaStreamGraph* aGraph)
{
  // In case that MediaEncoder does not receive a TRACK_EVENT_ENDED event.
  LOG("NotifyRemoved in [MediaEncoder].");
  mAudioEncoder->NotifyRemoved(aGraph);
}

/* static */
already_AddRefed<MediaEncoder>
MediaEncoder::CreateEncoder(const nsAString& aMIMEType)
{
  nsAutoPtr<ContainerWriter> writer;
  nsAutoPtr<AudioTrackEncoder> audioEncoder;
  nsAutoPtr<VideoTrackEncoder> videoEncoder;
  nsRefPtr<MediaEncoder> encoder;

  if (aMIMEType.IsEmpty()) {
    // TODO: Should pick out a default container+codec base on the track
    //       coming from MediaStreamGraph. For now, just default to Ogg+Opus.
    const_cast<nsAString&>(aMIMEType) = NS_LITERAL_STRING("audio/ogg");
  }

  bool isAudioOnly = FindInReadable(NS_LITERAL_STRING("audio/"), aMIMEType);
#ifdef MOZ_OGG
  if (IsOggType(aMIMEType)) {
    writer = new OggWriter();
    if (!isAudioOnly) {
      // Initialize the videoEncoder.
    }
#ifdef MOZ_OPUS
    audioEncoder = new OpusTrackEncoder();
#endif
  }
#endif
  // If the given mime-type is video but fail to create the video encoder.
  if (!isAudioOnly) {
    NS_ENSURE_TRUE(videoEncoder, nullptr);
  }

  // Return null if we fail to create the audio encoder.
  NS_ENSURE_TRUE(audioEncoder, nullptr);

  encoder = new MediaEncoder(writer.forget(), audioEncoder.forget(),
                             videoEncoder.forget(), aMIMEType);


  return encoder.forget();
}

/**
 * GetEncodedData() runs as a state machine, starting with mState set to
 * ENCODE_HEADER, the procedure should be as follow:
 *
 * While non-stop
 *   If mState is ENCODE_HEADER
 *     Create the header from audio/video encoder
 *     If a header is generated
 *       Insert header data into the container stream of writer
 *       Force copied the final container data from writer
 *       Return the copy of final container data
 *     Else
 *       Set mState to ENCODE_TRACK
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
 *   If mState is ENCODE_DONE
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
    case ENCODE_HEADER: {
      nsTArray<uint8_t> buffer;
      nsresult rv = mAudioEncoder->GetHeader(&buffer);
      if (NS_FAILED(rv)) {
        // Encoding might be canceled.
        mState = ENCODE_DONE;
        break;
      }

      if (!buffer.IsEmpty()) {
        rv = mWriter->WriteEncodedTrack(buffer, 0);
        if (NS_FAILED(rv)) {
          LOG("ERROR! Fail to write header to the media container.");
          mState = ENCODE_DONE;
          break;
        }

        rv = mWriter->GetContainerData(aOutputBufs,
                                       ContainerWriter::FLUSH_NEEDED);
        if (NS_SUCCEEDED(rv)) {
          // Successfully get the copy of final container data from writer.
          reloop = false;
          break;
        }
      } else {
        // No more headers, starts to encode tracks.
        mState = ENCODE_TRACK;
      }
      break;
    }

    case ENCODE_TRACK: {
      nsTArray<uint8_t> buffer;
      int encodedDuration = 0;
      nsresult rv = mAudioEncoder->GetEncodedTrack(&buffer, encodedDuration);
      if (NS_FAILED(rv)) {
        // Encoding might be canceled.
        LOG("ERROR! Fail to get encoded data from encoder.");
        mState = ENCODE_DONE;
        break;
      }

      rv = mWriter->WriteEncodedTrack(buffer, encodedDuration,
                                      mAudioEncoder->IsEncodingComplete() ?
                                      ContainerWriter::END_OF_STREAM : 0);
      if (NS_FAILED(rv)) {
        LOG("ERROR! Fail to write encoded track to the media container.");
        mState = ENCODE_DONE;
        break;
      }

      rv = mWriter->GetContainerData(aOutputBufs,
                                     mAudioEncoder->IsEncodingComplete() ?
                                     ContainerWriter::FLUSH_NEEDED : 0);
      if (NS_SUCCEEDED(rv)) {
        // Successfully get the copy of final container data from writer.
        reloop = false;
        break;
      }

      mState = (mAudioEncoder->IsEncodingComplete()) ? ENCODE_DONE : ENCODE_TRACK;
      break;
    }

    case ENCODE_DONE:
      LOG("MediaEncoder has been shutdown.");
      mShutdown = true;
      reloop = false;
      break;

    default:
      MOZ_CRASH("Invalid encode state");
    }
  }
}

}
