
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaPlaybackDelayPolicy.h"

#include "nsPIDOMWindow.h"
#include "mozilla/dom/HTMLMediaElement.h"

namespace mozilla {
namespace dom {

using AudibleState = AudioChannelService::AudibleState;

ResumeDelayedPlaybackAgent::~ResumeDelayedPlaybackAgent() {
  if (mDelegate) {
    mDelegate->Clear();
    mDelegate = nullptr;
  }
}

bool ResumeDelayedPlaybackAgent::InitDelegate(const HTMLMediaElement* aElement,
                                              AudibleState aAudibleState) {
  MOZ_ASSERT(!mDelegate, "Delegate has been initialized!");
  mDelegate = new ResumePlayDelegate();
  if (!mDelegate->Init(aElement, aAudibleState)) {
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
    AudibleState aAudibleState) {
  MOZ_ASSERT(mDelegate);
  mDelegate->UpdateAudibleState(aAudibleState);
}

NS_IMPL_ISUPPORTS(ResumeDelayedPlaybackAgent::ResumePlayDelegate,
                  nsIAudioChannelAgentCallback)

ResumeDelayedPlaybackAgent::ResumePlayDelegate::~ResumePlayDelegate() {
  MOZ_ASSERT(!mAudioChannelAgent);
}

bool ResumeDelayedPlaybackAgent::ResumePlayDelegate::Init(
    const HTMLMediaElement* aElement, AudibleState aAudibleState) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aElement->OwnerDoc());
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
  rv = mAudioChannelAgent->NotifyStartedPlaying(aAudibleState);
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
    AudibleState aAudibleState) {
  // It's possible for the owner of `ResumeDelayedPlaybackAgent` to call
  // `UpdateAudibleState()` after we resolve the promise and clean resource.
  if (!mAudioChannelAgent) {
    return;
  }
  mAudioChannelAgent->NotifyStartedAudible(
      aAudibleState,
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
  if (!doc) {
    return false;
  }

  nsPIDOMWindowInner* inner = nsPIDOMWindowInner::From(doc->GetInnerWindow());
  nsPIDOMWindowOuter* outer = nsPIDOMWindowOuter::GetFromCurrentInner(inner);
  if (!outer) {
    return false;
  }
  return outer->GetMediaSuspend() == nsISuspendedTypes::SUSPENDED_BLOCK;
}

RefPtr<ResumeDelayedPlaybackAgent>
MediaPlaybackDelayPolicy::CreateResumeDelayedPlaybackAgent(
    const HTMLMediaElement* aElement, AudibleState aAudibleState) {
  MOZ_ASSERT(aElement);
  RefPtr<ResumeDelayedPlaybackAgent> agent = new ResumeDelayedPlaybackAgent();
  return agent->InitDelegate(aElement, aAudibleState) ? agent : nullptr;
}

}  // namespace dom
}  // namespace mozilla
