/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChannelMediaDecoder_h_
#define ChannelMediaDecoder_h_

#include "MediaDecoder.h"
#include "MediaResourceCallback.h"
#include "MediaChannelStatistics.h"

class nsIChannel;
class nsIStreamListener;

namespace mozilla {

class BaseMediaResource;

DDLoggedTypeDeclNameAndBase(ChannelMediaDecoder, MediaDecoder);

class ChannelMediaDecoder
    : public MediaDecoder,
      public DecoderDoctorLifeLogger<ChannelMediaDecoder> {
  // Used to register with MediaResource to receive notifications which will
  // be forwarded to MediaDecoder.
  class ResourceCallback : public MediaResourceCallback {
    // Throttle calls to MediaDecoder::NotifyDataArrived()
    // to be at most once per 500ms.
    static const uint32_t sDelay = 500;

   public:
    explicit ResourceCallback(AbstractThread* aMainThread);
    // Start to receive notifications from ResourceCallback.
    void Connect(ChannelMediaDecoder* aDecoder);
    // Called upon shutdown to stop receiving notifications.
    void Disconnect();

   private:
    ~ResourceCallback();

    /* MediaResourceCallback functions */
    AbstractThread* AbstractMainThread() const override;
    MediaDecoderOwner* GetMediaOwner() const override;
    void NotifyNetworkError(const MediaResult& aError) override;
    void NotifyDataArrived() override;
    void NotifyDataEnded(nsresult aStatus) override;
    void NotifyPrincipalChanged() override;
    void NotifySuspendedStatusChanged(bool aSuspendedByCache) override;

    static void TimerCallback(nsITimer* aTimer, void* aClosure);

    // The decoder to send notifications. Main-thread only.
    ChannelMediaDecoder* mDecoder = nullptr;
    nsCOMPtr<nsITimer> mTimer;
    bool mTimerArmed = false;
    const RefPtr<AbstractThread> mAbstractMainThread;
  };

 protected:
  void ShutdownInternal() override;
  void OnPlaybackEvent(MediaPlaybackEvent&& aEvent) override;
  void DurationChanged() override;
  void MetadataLoaded(UniquePtr<MediaInfo> aInfo, UniquePtr<MetadataTags> aTags,
                      MediaDecoderEventVisibility aEventVisibility) override;
  void NotifyPrincipalChanged() override;

  RefPtr<ResourceCallback> mResourceCallback;
  RefPtr<BaseMediaResource> mResource;

  explicit ChannelMediaDecoder(MediaDecoderInit& aInit);

  void GetDebugInfo(dom::MediaDecoderDebugInfo& aInfo);

 public:
  // Create a decoder for the given aType. Returns null if we were unable
  // to create the decoder, for example because the requested MIME type in
  // the init struct was unsupported.
  static already_AddRefed<ChannelMediaDecoder> Create(
      MediaDecoderInit& aInit, DecoderDoctorDiagnostics* aDiagnostics);

  void Shutdown() override;

  bool CanClone();

  // Create a new decoder of the same type as this one.
  already_AddRefed<ChannelMediaDecoder> Clone(MediaDecoderInit& aInit);

  nsresult Load(nsIChannel* aChannel, bool aIsPrivateBrowsing,
                nsIStreamListener** aStreamListener);

  void AddSizeOfResources(ResourceSizes* aSizes) override;
  already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override;
  bool HadCrossOriginRedirects() override;
  bool IsTransportSeekable() override;
  void SetLoadInBackground(bool aLoadInBackground) override;
  void Suspend() override;
  void Resume() override;

 private:
  void DownloadProgressed();

  // Create a new state machine to run this decoder.
  MediaDecoderStateMachineBase* CreateStateMachine(
      bool aDisableExternalEngine) override;

  nsresult Load(BaseMediaResource* aOriginal);

  // Called by MediaResource when the download has ended.
  // Called on the main thread only. aStatus is the result from OnStopRequest.
  void NotifyDownloadEnded(nsresult aStatus);

  // Called by the MediaResource to keep track of the number of bytes read
  // from the resource. Called on the main by an event runner dispatched
  // by the MediaResource read functions.
  void NotifyBytesConsumed(int64_t aBytes, int64_t aOffset);

  bool CanPlayThroughImpl() final;

  struct PlaybackRateInfo {
    uint32_t mRate;  // Estimate of the current playback rate (bytes/second).
    bool mReliable;  // True if mRate is a reliable estimate.
  };
  // The actual playback rate computation.
  static PlaybackRateInfo ComputePlaybackRate(
      const MediaChannelStatistics& aStats, BaseMediaResource* aResource,
      double aDuration);

  // Something has changed that could affect the computed playback rate,
  // so recompute it.
  static void UpdatePlaybackRate(const PlaybackRateInfo& aInfo,
                                 BaseMediaResource* aResource);

  // Return statistics. This is used for progress events and other things.
  // This can be called from any thread. It's only a snapshot of the
  // current state, since other threads might be changing the state
  // at any time.
  static MediaStatistics GetStatistics(const PlaybackRateInfo& aInfo,
                                       BaseMediaResource* aRes,
                                       int64_t aPlaybackPosition);

  bool ShouldThrottleDownload(const MediaStatistics& aStats);

  // Data needed to estimate playback data rate. The timeline used for
  // this estimate is "decode time" (where the "current time" is the
  // time of the last decoded video frame).
  MediaChannelStatistics mPlaybackStatistics;

  // Current playback position in the stream. This is (approximately)
  // where we're up to playing back the stream. This is not adjusted
  // during decoder seek operations, but it's updated at the end when we
  // start playing back again.
  int64_t mPlaybackPosition = 0;

  bool mCanPlayThrough = false;

  // True if we've been notified that the ChannelMediaResource has
  // a principal.
  bool mInitialChannelPrincipalKnown = false;

  // Set in Shutdown() when we start closing mResource, if mResource is set.
  // Must resolve before we unregister the shutdown blocker.
  RefPtr<GenericPromise> mResourceClosePromise;
};

}  // namespace mozilla

#endif  // ChannelMediaDecoder_h_
