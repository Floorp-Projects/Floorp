/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelService.h"

#include "base/basictypes.h"

#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"

#include "nsContentUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsThreadUtils.h"
#include "nsHashPropertyBag.h"
#include "nsComponentManagerUtils.h"
#include "nsGlobalWindow.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;

mozilla::LazyLogModule gAudioChannelLog("AudioChannel");

namespace {

bool sXPCOMShuttingDown = false;

class AudioPlaybackRunnable final : public Runnable {
 public:
  AudioPlaybackRunnable(nsPIDOMWindowOuter* aWindow, bool aActive,
                        AudioChannelService::AudibleChangedReasons aReason)
      : mozilla::Runnable("AudioPlaybackRunnable"),
        mWindow(aWindow),
        mActive(aActive),
        mReason(aReason) {}

  NS_IMETHOD Run() override {
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return NS_ERROR_FAILURE;
    }

    nsAutoString state;
    GetActiveState(state);

    observerService->NotifyObservers(ToSupports(mWindow), "audio-playback",
                                     state.get());

    MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
            ("AudioPlaybackRunnable, active = %s, reason = %s\n",
             mActive ? "true" : "false", AudibleChangedReasonToStr(mReason)));

    return NS_OK;
  }

 private:
  void GetActiveState(nsAString& aState) {
    if (mActive) {
      aState.AssignLiteral("active");
    } else {
      if (mReason ==
          AudioChannelService::AudibleChangedReasons::ePauseStateChanged) {
        aState.AssignLiteral("inactive-pause");
      } else {
        aState.AssignLiteral("inactive-nonaudible");
      }
    }
  }

  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
  bool mActive;
  AudioChannelService::AudibleChangedReasons mReason;
};

}  // anonymous namespace

namespace mozilla {
namespace dom {

const char* SuspendTypeToStr(const nsSuspendedTypes& aSuspend) {
  MOZ_ASSERT(aSuspend == nsISuspendedTypes::NONE_SUSPENDED ||
             aSuspend == nsISuspendedTypes::SUSPENDED_BLOCK);

  switch (aSuspend) {
    case nsISuspendedTypes::NONE_SUSPENDED:
      return "none";
    case nsISuspendedTypes::SUSPENDED_BLOCK:
      return "block";
    default:
      return "unknown";
  }
}

const char* AudibleStateToStr(
    const AudioChannelService::AudibleState& aAudible) {
  MOZ_ASSERT(aAudible == AudioChannelService::AudibleState::eNotAudible ||
             aAudible == AudioChannelService::AudibleState::eMaybeAudible ||
             aAudible == AudioChannelService::AudibleState::eAudible);

  switch (aAudible) {
    case AudioChannelService::AudibleState::eNotAudible:
      return "not-audible";
    case AudioChannelService::AudibleState::eMaybeAudible:
      return "maybe-audible";
    case AudioChannelService::AudibleState::eAudible:
      return "audible";
    default:
      return "unknown";
  }
}

const char* AudibleChangedReasonToStr(
    const AudioChannelService::AudibleChangedReasons& aReason) {
  MOZ_ASSERT(
      aReason == AudioChannelService::AudibleChangedReasons::eVolumeChanged ||
      aReason ==
          AudioChannelService::AudibleChangedReasons::eDataAudibleChanged ||
      aReason ==
          AudioChannelService::AudibleChangedReasons::ePauseStateChanged);

  switch (aReason) {
    case AudioChannelService::AudibleChangedReasons::eVolumeChanged:
      return "volume";
    case AudioChannelService::AudibleChangedReasons::eDataAudibleChanged:
      return "data-audible";
    case AudioChannelService::AudibleChangedReasons::ePauseStateChanged:
      return "pause-state";
    default:
      return "unknown";
  }
}

StaticRefPtr<AudioChannelService> gAudioChannelService;

/* static */
void AudioChannelService::CreateServiceIfNeeded() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gAudioChannelService) {
    gAudioChannelService = new AudioChannelService();
  }
}

/* static */
already_AddRefed<AudioChannelService> AudioChannelService::GetOrCreate() {
  if (sXPCOMShuttingDown) {
    return nullptr;
  }

  CreateServiceIfNeeded();
  RefPtr<AudioChannelService> service = gAudioChannelService.get();
  return service.forget();
}

/* static */
already_AddRefed<AudioChannelService> AudioChannelService::Get() {
  if (sXPCOMShuttingDown) {
    return nullptr;
  }

  RefPtr<AudioChannelService> service = gAudioChannelService.get();
  return service.forget();
}

/* static */
LogModule* AudioChannelService::GetAudioChannelLog() {
  return gAudioChannelLog;
}

/* static */
void AudioChannelService::Shutdown() {
  if (gAudioChannelService) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(gAudioChannelService, "xpcom-shutdown");
      obs->RemoveObserver(gAudioChannelService, "outer-window-destroyed");
    }

    gAudioChannelService->mWindows.Clear();

    gAudioChannelService = nullptr;
  }
}

NS_INTERFACE_MAP_BEGIN(AudioChannelService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(AudioChannelService)
NS_IMPL_RELEASE(AudioChannelService)

AudioChannelService::AudioChannelService() {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
    obs->AddObserver(this, "outer-window-destroyed", false);
  }
}

AudioChannelService::~AudioChannelService() = default;

void AudioChannelService::RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                                    AudibleState aAudible) {
  MOZ_ASSERT(aAgent);

  uint64_t windowID = aAgent->WindowID();
  AudioChannelWindow* winData = GetWindowData(windowID);
  if (!winData) {
    winData = new AudioChannelWindow(windowID);
    mWindows.AppendElement(WrapUnique(winData));
  }

  // To make sure agent would be alive because AppendAgent() would trigger the
  // callback function of AudioChannelAgentOwner that means the agent might be
  // released in their callback.
  RefPtr<AudioChannelAgent> kungFuDeathGrip(aAgent);
  winData->AppendAgent(aAgent, aAudible);
}

void AudioChannelService::UnregisterAudioChannelAgent(
    AudioChannelAgent* aAgent) {
  MOZ_ASSERT(aAgent);

  AudioChannelWindow* winData = GetWindowData(aAgent->WindowID());
  if (!winData) {
    return;
  }

  // To make sure agent would be alive because AppendAgent() would trigger the
  // callback function of AudioChannelAgentOwner that means the agent might be
  // released in their callback.
  RefPtr<AudioChannelAgent> kungFuDeathGrip(aAgent);
  winData->RemoveAgent(aAgent);
}

AudioPlaybackConfig AudioChannelService::GetMediaConfig(
    nsPIDOMWindowOuter* aWindow) const {
  AudioPlaybackConfig config(1.0, false, nsISuspendedTypes::NONE_SUSPENDED);

  if (!aWindow) {
    config.mVolume = 0.0;
    config.mMuted = true;
    config.mSuspend = nsISuspendedTypes::SUSPENDED_BLOCK;
    return config;
  }

  AudioChannelWindow* winData = nullptr;
  nsCOMPtr<nsPIDOMWindowOuter> window = aWindow;

  // The volume must be calculated based on the window hierarchy. Here we go up
  // to the top window and we calculate the volume and the muted flag.
  do {
    winData = GetWindowData(window->WindowID());
    if (winData) {
      config.mVolume *= winData->mConfig.mVolume;
      config.mMuted = config.mMuted || winData->mConfig.mMuted;
      config.mCapturedAudio = winData->mIsAudioCaptured;
    }

    config.mVolume *= window->GetAudioVolume();
    config.mMuted = config.mMuted || window->GetAudioMuted();
    if (window->GetMediaSuspend() != nsISuspendedTypes::NONE_SUSPENDED) {
      config.mSuspend = window->GetMediaSuspend();
    }

    nsCOMPtr<nsPIDOMWindowOuter> win =
        window->GetInProcessScriptableParentOrNull();
    if (!win) {
      break;
    }

    window = win;

    // If there is no parent, or we are the toplevel we don't continue.
  } while (window && window != aWindow);

  return config;
}

void AudioChannelService::AudioAudibleChanged(AudioChannelAgent* aAgent,
                                              AudibleState aAudible,
                                              AudibleChangedReasons aReason) {
  MOZ_ASSERT(aAgent);

  uint64_t windowID = aAgent->WindowID();
  AudioChannelWindow* winData = GetWindowData(windowID);
  if (winData) {
    winData->AudioAudibleChanged(aAgent, aAudible, aReason);
  }
}

NS_IMETHODIMP
AudioChannelService::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    sXPCOMShuttingDown = true;
    Shutdown();
  } else if (!strcmp(aTopic, "outer-window-destroyed")) {
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

    uint64_t outerID;
    nsresult rv = wrapper->GetData(&outerID);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    UniquePtr<AudioChannelWindow> winData;
    {
      nsTObserverArray<UniquePtr<AudioChannelWindow>>::ForwardIterator iter(
          mWindows);
      while (iter.HasMore()) {
        auto& next = iter.GetNext();
        if (next->mWindowID == outerID) {
          uint32_t pos = mWindows.IndexOf(next);
          winData = std::move(next);
          mWindows.RemoveElementAt(pos);
          break;
        }
      }
    }

    if (winData) {
      nsTObserverArray<AudioChannelAgent*>::ForwardIterator iter(
          winData->mAgents);
      while (iter.HasMore()) {
        iter.GetNext()->WindowVolumeChanged(winData->mConfig.mVolume,
                                            winData->mConfig.mMuted);
      }
    }
  }

  return NS_OK;
}

void AudioChannelService::RefreshAgents(
    nsPIDOMWindowOuter* aWindow,
    const std::function<void(AudioChannelAgent*)>& aFunc) {
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsPIDOMWindowOuter> topWindow = aWindow->GetInProcessScriptableTop();
  if (!topWindow) {
    return;
  }

  AudioChannelWindow* winData = GetWindowData(topWindow->WindowID());
  if (!winData) {
    return;
  }

  nsTObserverArray<AudioChannelAgent*>::ForwardIterator iter(winData->mAgents);
  while (iter.HasMore()) {
    aFunc(iter.GetNext());
  }
}

void AudioChannelService::RefreshAgentsVolume(nsPIDOMWindowOuter* aWindow,
                                              float aVolume, bool aMuted) {
  RefreshAgents(aWindow, [aVolume, aMuted](AudioChannelAgent* agent) {
    agent->WindowVolumeChanged(aVolume, aMuted);
  });
}

void AudioChannelService::RefreshAgentsSuspend(nsPIDOMWindowOuter* aWindow,
                                               nsSuspendedTypes aSuspend) {
  RefreshAgents(aWindow, [aSuspend](AudioChannelAgent* agent) {
    agent->WindowSuspendChanged(aSuspend);
  });
}

void AudioChannelService::SetWindowAudioCaptured(nsPIDOMWindowOuter* aWindow,
                                                 uint64_t aInnerWindowID,
                                                 bool aCapture) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  MOZ_LOG(GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelService, SetWindowAudioCaptured, window = %p, "
           "aCapture = %d\n",
           aWindow, aCapture));

  nsCOMPtr<nsPIDOMWindowOuter> topWindow = aWindow->GetInProcessScriptableTop();
  if (!topWindow) {
    return;
  }

  AudioChannelWindow* winData = GetWindowData(topWindow->WindowID());

  // This can happen, but only during shutdown, because the the outer window
  // changes ScriptableTop, so that its ID is different.
  // In this case either we are capturing, and it's too late because the window
  // has been closed anyways, or we are un-capturing, and everything has already
  // been cleaned up by the HTMLMediaElements or the AudioContexts.
  if (!winData) {
    return;
  }

  if (aCapture != winData->mIsAudioCaptured) {
    winData->mIsAudioCaptured = aCapture;
    nsTObserverArray<AudioChannelAgent*>::ForwardIterator iter(
        winData->mAgents);
    while (iter.HasMore()) {
      iter.GetNext()->WindowAudioCaptureChanged(aInnerWindowID, aCapture);
    }
  }
}

AudioChannelService::AudioChannelWindow*
AudioChannelService::GetOrCreateWindowData(nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  AudioChannelWindow* winData = GetWindowData(aWindow->WindowID());
  if (!winData) {
    winData = new AudioChannelWindow(aWindow->WindowID());
    mWindows.AppendElement(WrapUnique(winData));
  }

  return winData;
}

AudioChannelService::AudioChannelWindow* AudioChannelService::GetWindowData(
    uint64_t aWindowID) const {
  nsTObserverArray<UniquePtr<AudioChannelWindow>>::ForwardIterator iter(
      mWindows);
  while (iter.HasMore()) {
    AudioChannelWindow* next = iter.GetNext().get();
    if (next->mWindowID == aWindowID) {
      return next;
    }
  }

  return nullptr;
}

bool AudioChannelService::IsWindowActive(nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());

  auto* window = nsPIDOMWindowOuter::From(aWindow)->GetInProcessScriptableTop();
  if (!window) {
    return false;
  }

  AudioChannelWindow* winData = GetWindowData(window->WindowID());
  if (!winData) {
    return false;
  }

  return !winData->mAudibleAgents.IsEmpty();
}

void AudioChannelService::NotifyMediaResumedFromBlock(
    nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsPIDOMWindowOuter> topWindow = aWindow->GetInProcessScriptableTop();
  if (!topWindow) {
    return;
  }

  AudioChannelWindow* winData = GetWindowData(topWindow->WindowID());
  if (!winData) {
    return;
  }

  winData->NotifyMediaBlockStop(aWindow);
}

void AudioChannelService::AudioChannelWindow::AppendAgent(
    AudioChannelAgent* aAgent, AudibleState aAudible) {
  MOZ_ASSERT(aAgent);

  AppendAgentAndIncreaseAgentsNum(aAgent);
  AudioAudibleChanged(aAgent, aAudible,
                      AudibleChangedReasons::eDataAudibleChanged);
}

void AudioChannelService::AudioChannelWindow::RemoveAgent(
    AudioChannelAgent* aAgent) {
  MOZ_ASSERT(aAgent);

  RemoveAgentAndReduceAgentsNum(aAgent);
  AudioAudibleChanged(aAgent, AudibleState::eNotAudible,
                      AudibleChangedReasons::ePauseStateChanged);
}

void AudioChannelService::AudioChannelWindow::NotifyMediaBlockStop(
    nsPIDOMWindowOuter* aWindow) {
  if (mShouldSendActiveMediaBlockStopEvent) {
    mShouldSendActiveMediaBlockStopEvent = false;
    nsCOMPtr<nsPIDOMWindowOuter> window = aWindow;
    NS_DispatchToCurrentThread(NS_NewRunnableFunction(
        "dom::AudioChannelService::AudioChannelWindow::NotifyMediaBlockStop",
        [window]() -> void {
          nsCOMPtr<nsIObserverService> observerService =
              services::GetObserverService();
          if (NS_WARN_IF(!observerService)) {
            return;
          }

          observerService->NotifyObservers(ToSupports(window), "audio-playback",
                                           u"activeMediaBlockStop");
        }));
  }
}

void AudioChannelService::AudioChannelWindow::AppendAgentAndIncreaseAgentsNum(
    AudioChannelAgent* aAgent) {
  MOZ_ASSERT(aAgent);
  MOZ_ASSERT(!mAgents.Contains(aAgent));

  mAgents.AppendElement(aAgent);

  ++mConfig.mNumberOfAgents;
}

void AudioChannelService::AudioChannelWindow::RemoveAgentAndReduceAgentsNum(
    AudioChannelAgent* aAgent) {
  MOZ_ASSERT(aAgent);
  MOZ_ASSERT(mAgents.Contains(aAgent));

  mAgents.RemoveElement(aAgent);

  MOZ_ASSERT(mConfig.mNumberOfAgents > 0);
  --mConfig.mNumberOfAgents;
}

void AudioChannelService::AudioChannelWindow::AudioAudibleChanged(
    AudioChannelAgent* aAgent, AudibleState aAudible,
    AudibleChangedReasons aReason) {
  MOZ_ASSERT(aAgent);

  if (aAudible == AudibleState::eAudible) {
    AppendAudibleAgentIfNotContained(aAgent, aReason);
  } else {
    RemoveAudibleAgentIfContained(aAgent, aReason);
  }

  if (aAudible != AudibleState::eNotAudible) {
    MaybeNotifyMediaBlockStart(aAgent);
  }
}

void AudioChannelService::AudioChannelWindow::AppendAudibleAgentIfNotContained(
    AudioChannelAgent* aAgent, AudibleChangedReasons aReason) {
  MOZ_ASSERT(aAgent);
  MOZ_ASSERT(mAgents.Contains(aAgent));

  if (!mAudibleAgents.Contains(aAgent)) {
    mAudibleAgents.AppendElement(aAgent);
    if (IsFirstAudibleAgent()) {
      NotifyAudioAudibleChanged(aAgent->Window(), AudibleState::eAudible,
                                aReason);
    }
  }
}

void AudioChannelService::AudioChannelWindow::RemoveAudibleAgentIfContained(
    AudioChannelAgent* aAgent, AudibleChangedReasons aReason) {
  MOZ_ASSERT(aAgent);

  if (mAudibleAgents.Contains(aAgent)) {
    mAudibleAgents.RemoveElement(aAgent);
    if (IsLastAudibleAgent()) {
      NotifyAudioAudibleChanged(aAgent->Window(), AudibleState::eNotAudible,
                                aReason);
    }
  }
}

bool AudioChannelService::AudioChannelWindow::IsFirstAudibleAgent() const {
  return (mAudibleAgents.Length() == 1);
}

bool AudioChannelService::AudioChannelWindow::IsLastAudibleAgent() const {
  return mAudibleAgents.IsEmpty();
}

void AudioChannelService::AudioChannelWindow::NotifyAudioAudibleChanged(
    nsPIDOMWindowOuter* aWindow, AudibleState aAudible,
    AudibleChangedReasons aReason) {
  RefPtr<AudioPlaybackRunnable> runnable = new AudioPlaybackRunnable(
      aWindow, aAudible == AudibleState::eAudible, aReason);
  DebugOnly<nsresult> rv = NS_DispatchToCurrentThread(runnable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToCurrentThread failed");
}

void AudioChannelService::AudioChannelWindow::MaybeNotifyMediaBlockStart(
    AudioChannelAgent* aAgent) {
  nsCOMPtr<nsPIDOMWindowOuter> window = aAgent->Window();
  if (!window) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowInner> inner = window->GetCurrentInnerWindow();
  if (!inner) {
    return;
  }

  nsCOMPtr<Document> doc = inner->GetExtantDoc();
  if (!doc) {
    return;
  }

  if (window->GetMediaSuspend() != nsISuspendedTypes::SUSPENDED_BLOCK ||
      !doc->Hidden()) {
    return;
  }

  if (!mShouldSendActiveMediaBlockStopEvent) {
    mShouldSendActiveMediaBlockStopEvent = true;
    NS_DispatchToCurrentThread(NS_NewRunnableFunction(
        "dom::AudioChannelService::AudioChannelWindow::"
        "MaybeNotifyMediaBlockStart",
        [window]() -> void {
          nsCOMPtr<nsIObserverService> observerService =
              services::GetObserverService();
          if (NS_WARN_IF(!observerService)) {
            return;
          }

          observerService->NotifyObservers(ToSupports(window), "audio-playback",
                                           u"activeMediaBlockStart");
        }));
  }
}

}  // namespace dom
}  // namespace mozilla
