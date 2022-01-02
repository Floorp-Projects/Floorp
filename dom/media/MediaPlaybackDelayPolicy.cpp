
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaPlaybackDelayPolicy.h"

#include "nsPIDOMWindow.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla::dom {

using AudibleState = AudioChannelService::AudibleState;

static AudibleState DetermineMediaAudibleState(const HTMLMediaElement* aElement,
                                               bool aIsAudible) {
  MOZ_ASSERT(aElement);
  if (!aElement->HasAudio()) {
    return AudibleState::eNotAudible;
  }
  // `eMaybeAudible` is used to distinguish if the media has audio track or not,
  // because we would only show the delayed media playback icon for media with
  // an audio track.
  return aIsAudible ? AudibleState::eAudible : AudibleState::eMaybeAudible;
}

ResumeDelayedPlaybackAgent::~ResumeDelayedPlaybackAgent() {
  if (mDelegate) {
    mDelegate->Clear();
    mDelegate = nullptr;
  }
}

bool ResumeDelayedPlaybackAgent::InitDelegate(const HTMLMediaElement* aElement,
                                              bool aIsAudible) {
  MOZ_ASSERT(!mDelegate, "Delegate has been initialized!");
  mDelegate = new ResumePlayDelegate();
  if (!mDelegate->Init(aElement, aIsAudible)) {
    mDelegate->Clear();
    mDelegate = nullptr;
    return false;
  }
  return true;
}

RefPtr<ResumeDelayedPlaybackAgent::ResumePromise>
ResumeDelayedPlaybackAgent::GetResumePromise() {
  MOZ_ASSERT(mDelegate);
  return mDelegate->GetResumePromise();
}

void ResumeDelayedPlaybackAgent::UpdateAudibleState(
    const HTMLMediaElement* aElement, bool aIsAudible) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(mDelegate);
  mDelegate->UpdateAudibleState(aElement, aIsAudible);
}

NS_IMPL_ISUPPORTS(ResumeDelayedPlaybackAgent::ResumePlayDelegate,
                  nsIAudioChannelAgentCallback)

ResumeDelayedPlaybackAgent::ResumePlayDelegate::~ResumePlayDelegate() {
  MOZ_ASSERT(!mAudioChannelAgent);
}

bool ResumeDelayedPlaybackAgent::ResumePlayDelegate::Init(
    const HTMLMediaElement* aElement, bool aIsAudible) {
  MOZ_ASSERT(aElement);
  if (!aElement->OwnerDoc()->GetInnerWindow()) {
    return false;
  }

  MOZ_ASSERT(!mAudioChannelAgent);
  mAudioChannelAgent = new AudioChannelAgent();
  nsresult rv =
      mAudioChannelAgent->Init(aElement->OwnerDoc()->GetInnerWindow(), this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Clear();
    return false;
  }

  // Start AudioChannelAgent in order to wait the suspended state change when we
  // are able to resume delayed playback.
  AudibleState audibleState = DetermineMediaAudibleState(aElement, aIsAudible);
  rv = mAudioChannelAgent->NotifyStartedPlaying(audibleState);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Clear();
    return false;
  }

  return true;
}

void ResumeDelayedPlaybackAgent::ResumePlayDelegate::Clear() {
  if (mAudioChannelAgent) {
    mAudioChannelAgent->NotifyStoppedPlaying();
    mAudioChannelAgent = nullptr;
  }
  mPromise.RejectIfExists(true, __func__);
}

RefPtr<ResumeDelayedPlaybackAgent::ResumePromise>
ResumeDelayedPlaybackAgent::ResumePlayDelegate::GetResumePromise() {
  return mPromise.Ensure(__func__);
}

void ResumeDelayedPlaybackAgent::ResumePlayDelegate::UpdateAudibleState(
    const HTMLMediaElement* aElement, bool aIsAudible) {
  MOZ_ASSERT(aElement);
  // It's possible for the owner of `ResumeDelayedPlaybackAgent` to call
  // `UpdateAudibleState()` after we resolve the promise and clean resource.
  if (!mAudioChannelAgent) {
    return;
  }
  AudibleState audibleState = DetermineMediaAudibleState(aElement, aIsAudible);
  mAudioChannelAgent->NotifyStartedAudible(
      audibleState,
      AudioChannelService::AudibleChangedReasons::eDataAudibleChanged);
}

NS_IMETHODIMP
ResumeDelayedPlaybackAgent::ResumePlayDelegate::WindowVolumeChanged(
    float aVolume, bool aMuted) {
  return NS_OK;
}

NS_IMETHODIMP
ResumeDelayedPlaybackAgent::ResumePlayDelegate::WindowAudioCaptureChanged(
    bool aCapture) {
  return NS_OK;
}

NS_IMETHODIMP
ResumeDelayedPlaybackAgent::ResumePlayDelegate::WindowSuspendChanged(
    SuspendTypes aSuspend) {
  if (aSuspend == nsISuspendedTypes::NONE_SUSPENDED) {
    mPromise.ResolveIfExists(true, __func__);
    Clear();
  }
  return NS_OK;
}

bool MediaPlaybackDelayPolicy::ShouldDelayPlayback(
    const HTMLMediaElement* aElement) {
  MOZ_ASSERT(aElement);
  if (!StaticPrefs::media_block_autoplay_until_in_foreground()) {
    return false;
  }

  const Document* doc = aElement->OwnerDoc();
  nsPIDOMWindowInner* inner = nsPIDOMWindowInner::From(doc->GetInnerWindow());
  nsPIDOMWindowOuter* outer = nsPIDOMWindowOuter::GetFromCurrentInner(inner);
  return outer && outer->ShouldDelayMediaFromStart();
}

RefPtr<ResumeDelayedPlaybackAgent>
MediaPlaybackDelayPolicy::CreateResumeDelayedPlaybackAgent(
    const HTMLMediaElement* aElement, bool aIsAudible) {
  MOZ_ASSERT(aElement);
  RefPtr<ResumeDelayedPlaybackAgent> agent = new ResumeDelayedPlaybackAgent();
  return agent->InitDelegate(aElement, aIsAudible) ? agent : nullptr;
}

}  // namespace mozilla::dom
