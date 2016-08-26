/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AbstractMediaDecoder_h_
#define AbstractMediaDecoder_h_

#include "mozilla/Attributes.h"
#include "mozilla/StateMirroring.h"

#include "FrameStatistics.h"
#include "MediaEventSource.h"
#include "MediaInfo.h"
#include "nsISupports.h"
#include "nsDataHashtable.h"
#include "nsThreadUtils.h"

class GMPCrashHelper;

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

/**
 * The AbstractMediaDecoder class describes the public interface for a media decoder
 * and is used by the MediaReader classes.
 */
class AbstractMediaDecoder : public nsIObserver
{
public:
  // A special version of the above for the ogg decoder that is allowed to be
  // called cross-thread.
  virtual bool IsOggDecoderShutdown() { return false; }

  // Get the current MediaResource being used. Its URI will be returned
  // by currentSrc. Returns what was passed to Load(), if Load() has been called.
  virtual MediaResource* GetResource() const = 0;

  // Increments the parsed, decoded and dropped frame counters by the passed in
  // counts.
  // Can be called on any thread.
  virtual void NotifyDecodedFrames(const FrameStatisticsData& aStats) = 0;

  virtual AbstractCanonical<media::NullableTimeUnit>* CanonicalDurationOrNull() { return nullptr; };

  // Return an event that will be notified when data arrives in MediaResource.
  // MediaDecoderReader will register with this event to receive notifications
  // in order to update buffer ranges.
  // Return null if this decoder doesn't support the event.
  virtual MediaEventSource<void>* DataArrivedEvent()
  {
    return nullptr;
  }

  // Notify the media decoder that a decryption key is required before emitting
  // further output. This only needs to be overridden for decoders that expect
  // encryption, such as the MediaSource decoder.
  virtual void NotifyWaitingForKey() {}

  // Return an event that will be notified when a decoder is waiting for a
  // decryption key before it can return more output.
  virtual MediaEventSource<void>* WaitingForKeyEvent()
  {
    return nullptr;
  }

protected:
  virtual void UpdateEstimatedMediaDuration(int64_t aDuration) {};
public:
  void DispatchUpdateEstimatedMediaDuration(int64_t aDuration)
  {
    NS_DispatchToMainThread(NewRunnableMethod<int64_t>(this,
                                                       &AbstractMediaDecoder::UpdateEstimatedMediaDuration,
                                                       aDuration));
  }

  virtual VideoFrameContainer* GetVideoFrameContainer() = 0;
  virtual mozilla::layers::ImageContainer* GetImageContainer() = 0;

  // Returns the owner of this decoder or null when the decoder is shutting
  // down. The owner should only be used on the main thread.
  virtual MediaDecoderOwner* GetOwner() const = 0;

  // Set by Reader if the current audio track can be offloaded
  virtual void SetPlatformCanOffloadAudio(bool aCanOffloadAudio) {}

  virtual already_AddRefed<GMPCrashHelper> GetCrashHelper() { return nullptr; }

  // Stack based class to assist in notifying the frame statistics of
  // parsed and decoded frames. Use inside video demux & decode functions
  // to ensure all parsed and decoded frames are reported on all return paths.
  class AutoNotifyDecoded {
  public:
    explicit AutoNotifyDecoded(AbstractMediaDecoder* aDecoder)
      : mDecoder(aDecoder)
    {}
    ~AutoNotifyDecoded() {
      if (mDecoder) {
        mDecoder->NotifyDecodedFrames(mStats);
      }
    }

    FrameStatisticsData mStats;

  private:
    AbstractMediaDecoder* mDecoder;
  };

  // Classes directly inheriting from AbstractMediaDecoder do not support
  // Observe and it should never be called directly.
  NS_IMETHOD Observe(nsISupports *aSubject, const char * aTopic, const char16_t * aData) override
  { MOZ_CRASH("Forbidden method"); return NS_OK; }
};

} // namespace mozilla

#endif

