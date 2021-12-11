/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelAgent.h"
#include "AudioChannelService.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Document.h"
#include "nsPIDOMWindow.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(AudioChannelAgent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AudioChannelAgent)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AudioChannelAgent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioChannelAgent)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgent)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AudioChannelAgent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AudioChannelAgent)

AudioChannelAgent::AudioChannelAgent()
    : mInnerWindowID(0), mIsRegToService(false) {
  // Init service in the begining, it can help us to know whether there is any
  // created media component via AudioChannelService::IsServiceStarted().
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
}

AudioChannelAgent::~AudioChannelAgent() { Shutdown(); }

void AudioChannelAgent::Shutdown() {
  if (mIsRegToService) {
    NotifyStoppedPlaying();
  }
}

NS_IMETHODIMP
AudioChannelAgent::Init(mozIDOMWindow* aWindow,
                        nsIAudioChannelAgentCallback* aCallback) {
  return InitInternal(nsPIDOMWindowInner::From(aWindow), aCallback,
                      /* useWeakRef = */ false);
}

NS_IMETHODIMP
AudioChannelAgent::InitWithWeakCallback(
    mozIDOMWindow* aWindow, nsIAudioChannelAgentCallback* aCallback) {
  return InitInternal(nsPIDOMWindowInner::From(aWindow), aCallback,
                      /* useWeakRef = */ true);
}

nsresult AudioChannelAgent::FindCorrectWindow(nsPIDOMWindowInner* aWindow) {
  mWindow = aWindow->GetInProcessScriptableTop();
  if (NS_WARN_IF(!mWindow)) {
    return NS_OK;
  }

  // From here we do an hack for nested iframes.
  // The system app doesn't have access to the nested iframe objects so it
  // cannot control the volume of the agents running in nested apps. What we do
  // here is to assign those Agents to the top scriptable window of the parent
  // iframe (what is controlled by the system app).
  // For doing this we go recursively back into the chain of windows until we
  // find apps that are not the system one.
  nsCOMPtr<nsPIDOMWindowOuter> outerParent = mWindow->GetInProcessParent();
  if (!outerParent || outerParent == mWindow) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowInner> parent = outerParent->GetCurrentInnerWindow();
  if (!parent) {
    return NS_OK;
  }

  nsCOMPtr<Document> doc = parent->GetExtantDoc();
  if (!doc) {
    return NS_OK;
  }

  if (nsContentUtils::IsChromeDoc(doc)) {
    return NS_OK;
  }

  return FindCorrectWindow(parent);
}

nsresult AudioChannelAgent::InitInternal(
    nsPIDOMWindowInner* aWindow, nsIAudioChannelAgentCallback* aCallback,
    bool aUseWeakRef) {
  if (NS_WARN_IF(!aWindow)) {
    return NS_ERROR_FAILURE;
  }

  mInnerWindowID = aWindow->WindowID();

  nsresult rv = FindCorrectWindow(aWindow);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aUseWeakRef) {
    mWeakCallback = do_GetWeakReference(aCallback);
  } else {
    mCallback = aCallback;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelAgent, InitInternal, this = %p, "
           "owner = %p, hasCallback = %d\n",
           this, mWindow.get(), (!!mCallback || !!mWeakCallback)));

  return NS_OK;
}

void AudioChannelAgent::PullInitialUpdate() {
  RefPtr<AudioChannelService> service = AudioChannelService::Get();
  MOZ_ASSERT(service);
  MOZ_ASSERT(mIsRegToService);

  AudioPlaybackConfig config = service->GetMediaConfig(mWindow);
  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelAgent, PullInitialUpdate, this=%p, "
           "mute=%s, volume=%f, suspend=%s, audioCapturing=%s\n",
           this, config.mMuted ? "true" : "false", config.mVolume,
           SuspendTypeToStr(config.mSuspend),
           config.mCapturedAudio ? "true" : "false"));
  WindowVolumeChanged(config.mVolume, config.mMuted);
  WindowSuspendChanged(config.mSuspend);
  WindowAudioCaptureChanged(InnerWindowID(), config.mCapturedAudio);
}

NS_IMETHODIMP
AudioChannelAgent::NotifyStartedPlaying(uint8_t aAudible) {
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (service == nullptr || mIsRegToService) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(AudioChannelService::AudibleState::eNotAudible == 0 &&
             AudioChannelService::AudibleState::eMaybeAudible == 1 &&
             AudioChannelService::AudibleState::eAudible == 2);
  service->RegisterAudioChannelAgent(
      this, static_cast<AudioChannelService::AudibleState>(aAudible));

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelAgent, NotifyStartedPlaying, this = %p, audible = %s\n",
           this,
           AudibleStateToStr(
               static_cast<AudioChannelService::AudibleState>(aAudible))));

  mIsRegToService = true;
  return NS_OK;
}

NS_IMETHODIMP
AudioChannelAgent::NotifyStoppedPlaying() {
  if (!mIsRegToService) {
    return NS_ERROR_FAILURE;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelAgent, NotifyStoppedPlaying, this = %p\n", this));

  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (service) {
    service->UnregisterAudioChannelAgent(this);
  }

  mIsRegToService = false;
  return NS_OK;
}

NS_IMETHODIMP
AudioChannelAgent::NotifyStartedAudible(uint8_t aAudible, uint32_t aReason) {
  MOZ_LOG(
      AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
      ("AudioChannelAgent, NotifyStartedAudible, this = %p, "
       "audible = %s, reason = %s\n",
       this,
       AudibleStateToStr(
           static_cast<AudioChannelService::AudibleState>(aAudible)),
       AudibleChangedReasonToStr(
           static_cast<AudioChannelService::AudibleChangedReasons>(aReason))));

  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_FAILURE;
  }

  service->AudioAudibleChanged(
      this, static_cast<AudioChannelService::AudibleState>(aAudible),
      static_cast<AudioChannelService::AudibleChangedReasons>(aReason));
  return NS_OK;
}

already_AddRefed<nsIAudioChannelAgentCallback>
AudioChannelAgent::GetCallback() {
  nsCOMPtr<nsIAudioChannelAgentCallback> callback = mCallback;
  if (!callback) {
    callback = do_QueryReferent(mWeakCallback);
  }
  return callback.forget();
}

void AudioChannelAgent::WindowVolumeChanged(float aVolume, bool aMuted) {
  nsCOMPtr<nsIAudioChannelAgentCallback> callback = GetCallback();
  if (!callback) {
    return;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelAgent, WindowVolumeChanged, this = %p, mute = %s, "
           "volume = %f\n",
           this, aMuted ? "true" : "false", aVolume));
  callback->WindowVolumeChanged(aVolume, aMuted);
}

void AudioChannelAgent::WindowSuspendChanged(nsSuspendedTypes aSuspend) {
  nsCOMPtr<nsIAudioChannelAgentCallback> callback = GetCallback();
  if (!callback) {
    return;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelAgent, WindowSuspendChanged, this = %p, "
           "suspended = %s\n",
           this, SuspendTypeToStr(aSuspend)));
  callback->WindowSuspendChanged(aSuspend);
}

AudioPlaybackConfig AudioChannelAgent::GetMediaConfig() const {
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  AudioPlaybackConfig config(1.0, false, nsISuspendedTypes::NONE_SUSPENDED);
  if (service) {
    config = service->GetMediaConfig(mWindow);
  }
  return config;
}

uint64_t AudioChannelAgent::WindowID() const {
  return mWindow ? mWindow->WindowID() : 0;
}

uint64_t AudioChannelAgent::InnerWindowID() const { return mInnerWindowID; }

void AudioChannelAgent::WindowAudioCaptureChanged(uint64_t aInnerWindowID,
                                                  bool aCapture) {
  if (aInnerWindowID != mInnerWindowID) {
    return;
  }

  nsCOMPtr<nsIAudioChannelAgentCallback> callback = GetCallback();
  if (!callback) {
    return;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelAgent, WindowAudioCaptureChanged, this = %p, "
           "capture = %d\n",
           this, aCapture));

  callback->WindowAudioCaptureChanged(aCapture);
}

bool AudioChannelAgent::IsWindowAudioCapturingEnabled() const {
  return GetMediaConfig().mCapturedAudio;
}

bool AudioChannelAgent::IsPlayingStarted() const { return mIsRegToService; }
