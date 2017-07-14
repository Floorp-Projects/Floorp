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

namespace mozilla {

namespace layers {
class ImageContainer;
class KnowsCompositor;
} // namespace layers

class AbstractThread;
class MediaResource;
class ReentrantMonitor;
class VideoFrameContainer;
class MediaDecoderOwner;
class CDMProxy;
class GMPCrashHelper;

typedef nsDataHashtable<nsCStringHashKey, nsCString> MetadataTags;

/**
 * The AbstractMediaDecoder class describes the public interface for a media decoder
 * and is used by the MediaReader classes.
 */
class AbstractMediaDecoder : public nsIObserver
{
public:
  // Increments the parsed, decoded and dropped frame counters by the passed in
  // counts.
  // Can be called on any thread.
  virtual void NotifyDecodedFrames(const FrameStatisticsData& aStats) = 0;

  // Notify the media decoder that a decryption key is required before emitting
  // further output. This only needs to be overridden for decoders that expect
  // encryption, such as the MediaSource decoder.
  virtual void NotifyWaitingForKey() { }

  // Return an event that will be notified when a decoder is waiting for a
  // decryption key before it can return more output.
  virtual MediaEventSource<void>* WaitingForKeyEvent()
  {
    return nullptr;
  }

  // Return an abstract thread on which to run main thread runnables.
  virtual AbstractThread* AbstractMainThread() const = 0;

public:
  virtual VideoFrameContainer* GetVideoFrameContainer() = 0;
  virtual mozilla::layers::ImageContainer* GetImageContainer() = 0;

  // Returns the owner of this decoder or null when the decoder is shutting
  // down. The owner should only be used on the main thread.
  virtual MediaDecoderOwner* GetOwner() const = 0;

  // Set by Reader if the current audio track can be offloaded
  virtual void SetPlatformCanOffloadAudio(bool aCanOffloadAudio) { }

  // Stack based class to assist in notifying the frame statistics of
  // parsed and decoded frames. Use inside video demux & decode functions
  // to ensure all parsed and decoded frames are reported on all return paths.
  class AutoNotifyDecoded
  {
  public:
    explicit AutoNotifyDecoded(AbstractMediaDecoder* aDecoder)
      : mDecoder(aDecoder)
    {
    }
    ~AutoNotifyDecoded()
    {
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
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override
  {
    MOZ_CRASH("Forbidden method");
    return NS_OK;
  }
};

} // namespace mozilla

#endif

