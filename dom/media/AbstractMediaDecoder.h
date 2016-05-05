/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AbstractMediaDecoder_h_
#define AbstractMediaDecoder_h_

#include "mozilla/Attributes.h"
#include "mozilla/StateMirroring.h"

#include "MediaEventSource.h"
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
  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded,
                                   uint32_t aDropped) = 0;

  virtual AbstractCanonical<media::NullableTimeUnit>* CanonicalDurationOrNull() { return nullptr; };

  // Return an event that will be notified when data arrives in MediaResource.
  // MediaDecoderReader will register with this event to receive notifications
  // in order to udpate buffer ranges.
  // Return null if this decoder doesn't support the event.
  virtual MediaEventSource<void>* DataArrivedEvent()
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

  // Returns the owner of this media decoder. The owner should only be used
  // on the main thread.
  virtual MediaDecoderOwner* GetOwner() = 0;

  // Set by Reader if the current audio track can be offloaded
  virtual void SetPlatformCanOffloadAudio(bool aCanOffloadAudio) {}

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
};

} // namespace mozilla

#endif

