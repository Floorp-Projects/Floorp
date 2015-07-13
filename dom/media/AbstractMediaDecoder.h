/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AbstractMediaDecoder_h_
#define AbstractMediaDecoder_h_

#include "mozilla/Attributes.h"
#include "StateMirroring.h"
#include "MediaInfo.h"
#include "nsISupports.h"
#include "nsDataHashtable.h"
#include "nsThreadUtils.h"

namespace mozilla
{

namespace layers
{
  class ImageContainer;
} // namespace layers
class MediaResource;
class ReentrantMonitor;
class VideoFrameContainer;
class MediaDecoderOwner;
#ifdef MOZ_EME
class CDMProxy;
#endif

typedef nsDataHashtable<nsCStringHashKey, nsCString> MetadataTags;

static inline bool IsCurrentThread(nsIThread* aThread) {
  return NS_GetCurrentThread() == aThread;
}

enum class MediaDecoderEventVisibility : int8_t {
  Observable,
  Suppressed
};

/**
 * The AbstractMediaDecoder class describes the public interface for a media decoder
 * and is used by the MediaReader classes.
 */
class AbstractMediaDecoder : public nsIObserver
{
public:
  // Returns the monitor for other threads to synchronise access to
  // state.
  virtual ReentrantMonitor& GetReentrantMonitor() = 0;

  // Returns true if the decoder is shut down.
  virtual bool IsShutdown() const = 0;

  // A special version of the above for the ogg decoder that is allowed to be
  // called cross-thread.
  virtual bool IsOggDecoderShutdown() { return false; }

  virtual bool OnStateMachineTaskQueue() const = 0;

  virtual bool OnDecodeTaskQueue() const = 0;

  // Get the current MediaResource being used. Its URI will be returned
  // by currentSrc. Returns what was passed to Load(), if Load() has been called.
  virtual MediaResource* GetResource() const = 0;

  // Called by the decode thread to keep track of the number of bytes read
  // from the resource.
  virtual void NotifyBytesConsumed(int64_t aBytes, int64_t aOffset) = 0;

  // Increments the parsed, decoded and dropped frame counters by the passed in
  // counts.
  // Can be called on any thread.
  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded,
                                   uint32_t aDropped) = 0;

  virtual AbstractCanonical<media::NullableTimeUnit>* CanonicalDurationOrNull() { return nullptr; };

protected:
  virtual void UpdateEstimatedMediaDuration(int64_t aDuration) {};
public:
  void DispatchUpdateEstimatedMediaDuration(int64_t aDuration)
  {
    nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableMethodWithArg<int64_t>(this, &AbstractMediaDecoder::UpdateEstimatedMediaDuration,
                                           aDuration);
    NS_DispatchToMainThread(r);
  }

  // Set the media as being seekable or not.
  virtual void SetMediaSeekable(bool aMediaSeekable) = 0;

  virtual VideoFrameContainer* GetVideoFrameContainer() = 0;
  virtual mozilla::layers::ImageContainer* GetImageContainer() = 0;

  // Return true if the media layer supports seeking.
  virtual bool IsTransportSeekable() = 0;

  // Return true if the transport layer supports seeking.
  virtual bool IsMediaSeekable() = 0;

  virtual void MetadataLoaded(nsAutoPtr<MediaInfo> aInfo, nsAutoPtr<MetadataTags> aTags, MediaDecoderEventVisibility aEventVisibility) = 0;
  virtual void QueueMetadata(int64_t aTime, nsAutoPtr<MediaInfo> aInfo, nsAutoPtr<MetadataTags> aTags) = 0;
  virtual void FirstFrameLoaded(nsAutoPtr<MediaInfo> aInfo, MediaDecoderEventVisibility aEventVisibility) = 0;

  virtual void RemoveMediaTracks() = 0;

  // May be called by the reader to notify this decoder that the metadata from
  // the media file has been read. Call on the decode thread only.
  virtual void OnReadMetadataCompleted() = 0;

  // Returns the owner of this media decoder. The owner should only be used
  // on the main thread.
  virtual MediaDecoderOwner* GetOwner() = 0;

  // May be called by the reader to notify the decoder that the resources
  // required to begin playback have been acquired. Can be called on any thread.
  virtual void NotifyWaitingForResourcesStatusChanged() = 0;

  // Called by the reader's MediaResource as data arrives over the network.
  // Must be called on the main thread.
  virtual void NotifyDataArrived(uint32_t aLength, int64_t aOffset,
                                 bool aThrottleUpdates) = 0;

  // Set by Reader if the current audio track can be offloaded
  virtual void SetPlatformCanOffloadAudio(bool aCanOffloadAudio) {}

  // Called by Decoder/State machine to check audio offload condtions are met
  virtual bool CheckDecoderCanOffloadAudio() { return false; }

  // Called from HTMLMediaElement when owner document activity changes
  virtual void SetElementVisibility(bool aIsVisible) {}

  // Called by some MediaDecoderReader to determine if we can rely
  // on the resource length to limit reads.
  virtual bool HasInitializationData() { return false; }

  // Stack based class to assist in notifying the frame statistics of
  // parsed and decoded frames. Use inside video demux & decode functions
  // to ensure all parsed and decoded frames are reported on all return paths.
  class AutoNotifyDecoded {
  public:
    explicit AutoNotifyDecoded(AbstractMediaDecoder* aDecoder)
      : mParsed(0), mDecoded(0), mDropped(0), mDecoder(aDecoder) {}
    ~AutoNotifyDecoded() {
      if (mDecoder) {
        mDecoder->NotifyDecodedFrames(mParsed, mDecoded, mDropped);
      }
    }
    uint32_t mParsed;
    uint32_t mDecoded;
    uint32_t mDropped;

  private:
    AbstractMediaDecoder* mDecoder;
  };

  // Classes directly inheriting from AbstractMediaDecoder do not support
  // Observe and it should never be called directly.
  NS_IMETHOD Observe(nsISupports *aSubject, const char * aTopic, const char16_t * aData) override
  { MOZ_CRASH("Forbidden method"); return NS_OK; }

#ifdef MOZ_EME
  virtual nsresult SetCDMProxy(CDMProxy* aProxy) { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual CDMProxy* GetCDMProxy() { return nullptr; }
#endif
};

class MetadataContainer
{
protected:
  MetadataContainer(AbstractMediaDecoder* aDecoder,
                    nsAutoPtr<MediaInfo> aInfo,
                    nsAutoPtr<MetadataTags> aTags,
                    MediaDecoderEventVisibility aEventVisibility)
    : mDecoder(aDecoder),
      mInfo(aInfo),
      mTags(aTags),
      mEventVisibility(aEventVisibility)
  {}

  nsRefPtr<AbstractMediaDecoder> mDecoder;
  nsAutoPtr<MediaInfo>  mInfo;
  nsAutoPtr<MetadataTags> mTags;
  MediaDecoderEventVisibility mEventVisibility;
};

class MetadataEventRunner : public nsRunnable, private MetadataContainer
{
public:
  MetadataEventRunner(AbstractMediaDecoder* aDecoder,
                      nsAutoPtr<MediaInfo> aInfo,
                      nsAutoPtr<MetadataTags> aTags,
                      MediaDecoderEventVisibility aEventVisibility = MediaDecoderEventVisibility::Observable)
    : MetadataContainer(aDecoder, aInfo, aTags, aEventVisibility)
  {}

  NS_IMETHOD Run() override
  {
    mDecoder->MetadataLoaded(mInfo, mTags, mEventVisibility);
    return NS_OK;
  }
};

class FirstFrameLoadedEventRunner : public nsRunnable, private MetadataContainer
{
public:
  FirstFrameLoadedEventRunner(AbstractMediaDecoder* aDecoder,
                              nsAutoPtr<MediaInfo> aInfo,
                              MediaDecoderEventVisibility aEventVisibility = MediaDecoderEventVisibility::Observable)
    : MetadataContainer(aDecoder, aInfo, nsAutoPtr<MetadataTags>(nullptr), aEventVisibility)
  {}

  NS_IMETHOD Run() override
  {
    mDecoder->FirstFrameLoaded(mInfo, mEventVisibility);
    return NS_OK;
  }
};

class MetadataUpdatedEventRunner : public nsRunnable, private MetadataContainer
{
public:
  MetadataUpdatedEventRunner(AbstractMediaDecoder* aDecoder,
                             nsAutoPtr<MediaInfo> aInfo,
                             nsAutoPtr<MetadataTags> aTags,
                             MediaDecoderEventVisibility aEventVisibility = MediaDecoderEventVisibility::Observable)
    : MetadataContainer(aDecoder, aInfo, aTags, aEventVisibility)
  {}

  NS_IMETHOD Run() override
  {
    nsAutoPtr<MediaInfo> info(new MediaInfo());
    *info = *mInfo;
    mDecoder->MetadataLoaded(info, mTags, mEventVisibility);
    mDecoder->FirstFrameLoaded(mInfo, mEventVisibility);
    return NS_OK;
  }
};

class RemoveMediaTracksEventRunner : public nsRunnable
{
public:
  explicit RemoveMediaTracksEventRunner(AbstractMediaDecoder* aDecoder)
    : mDecoder(aDecoder)
  {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    mDecoder->RemoveMediaTracks();
    return NS_OK;
  }

private:
  nsRefPtr<AbstractMediaDecoder> mDecoder;
};

} // namespace mozilla

#endif

