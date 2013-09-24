/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AbstractMediaDecoder_h_
#define AbstractMediaDecoder_h_

#include "mozilla/Attributes.h"
#include "nsISupports.h"
#include "nsDataHashtable.h"
#include "nsThreadUtils.h"

namespace mozilla
{

namespace layers
{
  class ImageContainer;
}
class MediaResource;
class ReentrantMonitor;
class VideoFrameContainer;
class TimedMetadata;
class MediaDecoderOwner;

typedef nsDataHashtable<nsCStringHashKey, nsCString> MetadataTags;

static inline bool IsCurrentThread(nsIThread* aThread) {
  return NS_GetCurrentThread() == aThread;
}

/**
 * The AbstractMediaDecoder class describes the public interface for a media decoder
 * and is used by the MediaReader classes.
 */
class AbstractMediaDecoder : public nsISupports
{
public:
  // Returns the monitor for other threads to synchronise access to
  // state.
  virtual ReentrantMonitor& GetReentrantMonitor() = 0;

  // Returns true if the decoder is shut down.
  virtual bool IsShutdown() const = 0;

  virtual bool OnStateMachineThread() const = 0;

  virtual bool OnDecodeThread() const = 0;

  // Get the current MediaResource being used. Its URI will be returned
  // by currentSrc. Returns what was passed to Load(), if Load() has been called.
  virtual MediaResource* GetResource() const = 0;

  // Called by the decode thread to keep track of the number of bytes read
  // from the resource.
  virtual void NotifyBytesConsumed(int64_t aBytes, int64_t aOffset) = 0;

  // Increments the parsed and decoded frame counters by the passed in counts.
  // Can be called on any thread.
  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded) = 0;

  // Returns the end time of the last sample in the media. Note that a media
  // can have a non-zero start time, so the end time may not necessarily be
  // the same as the duration (i.e. duration is (end_time - start_time)).
  virtual int64_t GetEndMediaTime() const = 0;

  // Return the duration of the media in microseconds.
  virtual int64_t GetMediaDuration() = 0;

  // Set the duration of the media in microseconds.
  virtual void SetMediaDuration(int64_t aDuration) = 0;

  // Sets the duration of the media in microseconds. The MediaDecoder
  // fires a durationchange event to its owner (e.g., an HTML audio
  // tag).
  virtual void UpdateEstimatedMediaDuration(int64_t aDuration) = 0;

  // Set the media as being seekable or not.
  virtual void SetMediaSeekable(bool aMediaSeekable) = 0;

  // Set the transport level as being seekable or not.
  virtual void SetTransportSeekable(bool aTransportSeekable) = 0;

  virtual VideoFrameContainer* GetVideoFrameContainer() = 0;
  virtual mozilla::layers::ImageContainer* GetImageContainer() = 0;

  // Return true if the media layer supports seeking.
  virtual bool IsTransportSeekable() = 0;

  // Return true if the transport layer supports seeking.
  virtual bool IsMediaSeekable() = 0;

  virtual void MetadataLoaded(int aChannels, int aRate, bool aHasAudio, bool aHasVideo, MetadataTags* aTags) = 0;
  virtual void QueueMetadata(int64_t aTime, int aChannels, int aRate, bool aHasAudio, bool aHasVideo, MetadataTags* aTags) = 0;

  // Set the media end time in microseconds
  virtual void SetMediaEndTime(int64_t aTime) = 0;

  // Make the decoder state machine update the playback position. Called by
  // the reader on the decoder thread (Assertions for this checked by
  // mDecoderStateMachine). This must be called with the decode monitor
  // held.
  virtual void UpdatePlaybackPosition(int64_t aTime) = 0;

  // May be called by the reader to notify this decoder that the metadata from
  // the media file has been read. Call on the decode thread only.
  virtual void OnReadMetadataCompleted() = 0;

  // Returns the owner of this media decoder. The owner should only be used
  // on the main thread.
  virtual MediaDecoderOwner* GetOwner() = 0;

  // Stack based class to assist in notifying the frame statistics of
  // parsed and decoded frames. Use inside video demux & decode functions
  // to ensure all parsed and decoded frames are reported on all return paths.
  class AutoNotifyDecoded {
  public:
    AutoNotifyDecoded(AbstractMediaDecoder* aDecoder, uint32_t& aParsed, uint32_t& aDecoded)
      : mDecoder(aDecoder), mParsed(aParsed), mDecoded(aDecoded) {}
    ~AutoNotifyDecoded() {
      mDecoder->NotifyDecodedFrames(mParsed, mDecoded);
    }
  private:
    AbstractMediaDecoder* mDecoder;
    uint32_t& mParsed;
    uint32_t& mDecoded;
  };
};

class AudioMetadataEventRunner : public nsRunnable
{
  private:
    nsRefPtr<AbstractMediaDecoder> mDecoder;
  public:
    AudioMetadataEventRunner(AbstractMediaDecoder* aDecoder, int aChannels, int aRate, bool aHasAudio, bool aHasVideo, MetadataTags* aTags)
      : mDecoder(aDecoder),
        mChannels(aChannels),
        mRate(aRate),
        mHasAudio(aHasAudio),
        mHasVideo(aHasVideo),
        mTags(aTags)
  {}

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    mDecoder->MetadataLoaded(mChannels, mRate, mHasAudio, mHasVideo, mTags);
    return NS_OK;
  }

  int mChannels;
  int mRate;
  bool mHasAudio;
  bool mHasVideo;
  MetadataTags* mTags;
};


}

#endif

