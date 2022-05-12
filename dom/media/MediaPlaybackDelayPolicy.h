/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mediaplaybackdelaypolicy_h__
#define mozilla_dom_mediaplaybackdelaypolicy_h__

#include "AudioChannelAgent.h"
#include "AudioChannelService.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

typedef uint32_t SuspendTypes;

namespace mozilla::dom {

class HTMLMediaElement;
/**
 * We usaually start AudioChannelAgent when media starts and stop it when media
 * stops. However, when we decide to delay media playback for unvisited tab, we
 * would start AudioChannelAgent even if media doesn't start in order to
 * register the agent to AudioChannelService, so that the service could notify
 * us when we are able to resume media playback. Therefore,
 * ResumeDelayedPlaybackAgent is used to handle this special use case of
 * AudioChannelAgent.
 * - Use `GetResumePromise()` to require resume-promise and then do follow-up
 *   resume behavior when promise is resolved.
 * - Use `UpdateAudibleState()` to update audible state only when media info
 *   changes. As having audio track or not is the only thing for us to decide
 *   whether we would show the delayed media playback icon on the tab bar.
 */
class ResumeDelayedPlaybackAgent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ResumeDelayedPlaybackAgent);
  ResumeDelayedPlaybackAgent() = default;

  using ResumePromise = MozPromise<bool, bool, true /* IsExclusive */>;
  RefPtr<ResumePromise> GetResumePromise();
  void UpdateAudibleState(const HTMLMediaElement* aElement, bool aIsAudible);

 private:
  friend class MediaPlaybackDelayPolicy;

  ~ResumeDelayedPlaybackAgent();
  bool InitDelegate(const HTMLMediaElement* aElement, bool aIsAudible);

  class ResumePlayDelegate final : public nsIAudioChannelAgentCallback {
   public:
    NS_DECL_ISUPPORTS

    ResumePlayDelegate() = default;

    bool Init(const HTMLMediaElement* aElement, bool aIsAudible);
    void UpdateAudibleState(const HTMLMediaElement* aElement, bool aIsAudible);
    RefPtr<ResumePromise> GetResumePromise();
    void Clear();

    NS_IMETHODIMP WindowVolumeChanged(float aVolume, bool aMuted) override;
    NS_IMETHODIMP WindowAudioCaptureChanged(bool aCapture) override;
    NS_IMETHODIMP WindowSuspendChanged(SuspendTypes aSuspend) override;

   private:
    virtual ~ResumePlayDelegate();

    MozPromiseHolder<ResumePromise> mPromise;
    RefPtr<AudioChannelAgent> mAudioChannelAgent;
  };

  RefPtr<ResumePlayDelegate> mDelegate;
};

class MediaPlaybackDelayPolicy {
 public:
  static bool ShouldDelayPlayback(const HTMLMediaElement* aElement);
  static RefPtr<ResumeDelayedPlaybackAgent> CreateResumeDelayedPlaybackAgent(
      const HTMLMediaElement* aElement, bool aIsAudible);
};

}  // namespace mozilla::dom

#endif
