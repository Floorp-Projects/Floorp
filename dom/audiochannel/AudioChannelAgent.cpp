/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelAgent.h"
#include "AudioChannelService.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIURI.h"

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
  : mAudioChannelType(AUDIO_AGENT_CHANNEL_ERROR)
  , mInnerWindowID(0)
  , mIsRegToService(false)
{
  // Init service in the begining, it can help us to know whether there is any
  // created media component via AudioChannelService::IsServiceStarted().
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
}

AudioChannelAgent::~AudioChannelAgent()
{
  Shutdown();
}

void
AudioChannelAgent::Shutdown()
{
  if (mIsRegToService) {
    NotifyStoppedPlaying();
  }
}

NS_IMETHODIMP AudioChannelAgent::GetAudioChannelType(int32_t *aAudioChannelType)
{
  *aAudioChannelType = mAudioChannelType;
  return NS_OK;
}

NS_IMETHODIMP
AudioChannelAgent::Init(mozIDOMWindow* aWindow, int32_t aChannelType,
                        nsIAudioChannelAgentCallback *aCallback)
{
  return InitInternal(nsPIDOMWindowInner::From(aWindow), aChannelType,
                      aCallback, /* useWeakRef = */ false);
}

NS_IMETHODIMP
AudioChannelAgent::InitWithWeakCallback(mozIDOMWindow* aWindow,
                                        int32_t aChannelType,
                                        nsIAudioChannelAgentCallback *aCallback)
{
  return InitInternal(nsPIDOMWindowInner::From(aWindow), aChannelType,
                      aCallback, /* useWeakRef = */ true);
}

nsresult
AudioChannelAgent::FindCorrectWindow(nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(aWindow->IsInnerWindow());

  mWindow = aWindow->GetScriptableTop();
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
  nsCOMPtr<nsPIDOMWindowOuter> outerParent = mWindow->GetParent();
  if (!outerParent || outerParent == mWindow) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowInner> parent = outerParent->GetCurrentInnerWindow();
  if (!parent) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = parent->GetExtantDoc();
  if (!doc) {
    return NS_OK;
  }

  if (nsContentUtils::IsChromeDoc(doc)) {
    return NS_OK;
  }

  nsAdoptingCString systemAppUrl =
    mozilla::Preferences::GetCString("b2g.system_startup_url");
  if (!systemAppUrl) {
    return NS_OK;
  }

  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
  nsCOMPtr<nsIURI> uri;
  principal->GetURI(getter_AddRefs(uri));

  if (uri) {
    nsAutoCString spec;
    uri->GetSpec(spec);

    if (spec.Equals(systemAppUrl)) {
      return NS_OK;
    }
  }

  return FindCorrectWindow(parent);
}

nsresult
AudioChannelAgent::InitInternal(nsPIDOMWindowInner* aWindow,
                                int32_t aChannelType,
                                nsIAudioChannelAgentCallback *aCallback,
                                bool aUseWeakRef)
{
  // We syncd the enum of channel type between nsIAudioChannelAgent.idl and
  // AudioChannelBinding.h the same.
  MOZ_ASSERT(int(AUDIO_AGENT_CHANNEL_NORMAL) == int(AudioChannel::Normal) &&
             int(AUDIO_AGENT_CHANNEL_CONTENT) == int(AudioChannel::Content) &&
             int(AUDIO_AGENT_CHANNEL_NOTIFICATION) == int(AudioChannel::Notification) &&
             int(AUDIO_AGENT_CHANNEL_ALARM) == int(AudioChannel::Alarm) &&
             int(AUDIO_AGENT_CHANNEL_TELEPHONY) == int(AudioChannel::Telephony) &&
             int(AUDIO_AGENT_CHANNEL_RINGER) == int(AudioChannel::Ringer) &&
             int(AUDIO_AGENT_CHANNEL_SYSTEM) == int(AudioChannel::System) &&
             int(AUDIO_AGENT_CHANNEL_PUBLICNOTIFICATION) == int(AudioChannel::Publicnotification),
             "Enum of channel on nsIAudioChannelAgent.idl should be the same with AudioChannelBinding.h");

  if (mAudioChannelType != AUDIO_AGENT_CHANNEL_ERROR ||
      aChannelType > AUDIO_AGENT_CHANNEL_SYSTEM ||
      aChannelType < AUDIO_AGENT_CHANNEL_NORMAL) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aWindow)) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(aWindow->IsInnerWindow());
  mInnerWindowID = aWindow->WindowID();

  nsresult rv = FindCorrectWindow(aWindow);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mAudioChannelType = aChannelType;

  if (aUseWeakRef) {
    mWeakCallback = do_GetWeakReference(aCallback);
  } else {
    mCallback = aCallback;
  }

  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  service->NotifyCreatedNewAgent(this);

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("AudioChannelAgent, InitInternal, this = %p, type = %d, "
          "owner = %p, hasCallback = %d\n", this, mAudioChannelType,
          mWindow.get(), (!!mCallback || !!mWeakCallback)));

  return NS_OK;
}

NS_IMETHODIMP
AudioChannelAgent::NotifyStartedPlaying(AudioPlaybackConfig* aConfig,
                                        uint8_t aAudible)
{
  if (NS_WARN_IF(!aConfig)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (mAudioChannelType == AUDIO_AGENT_CHANNEL_ERROR ||
      service == nullptr || mIsRegToService) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(AudioChannelService::AudibleState::eNotAudible == 0 &&
             AudioChannelService::AudibleState::eMaybeAudible == 1 &&
             AudioChannelService::AudibleState::eAudible == 2);
  service->RegisterAudioChannelAgent(this,
    static_cast<AudioChannelService::AudibleState>(aAudible));

  AudioPlaybackConfig config = service->GetMediaConfig(mWindow,
                                                       mAudioChannelType);

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("AudioChannelAgent, NotifyStartedPlaying, this = %p, "
          "audible = %s, mute = %s, volume = %f, suspend = %s\n", this,
          AudibleStateToStr(static_cast<AudioChannelService::AudibleState>(aAudible)),
          config.mMuted ? "true" : "false", config.mVolume,
          SuspendTypeToStr(config.mSuspend)));

  aConfig->SetConfig(config.mVolume, config.mMuted, config.mSuspend);
  mIsRegToService = true;
  return NS_OK;
}

NS_IMETHODIMP
AudioChannelAgent::NotifyStoppedPlaying()
{
  if (mAudioChannelType == AUDIO_AGENT_CHANNEL_ERROR ||
      !mIsRegToService) {
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
AudioChannelAgent::NotifyStartedAudible(uint8_t aAudible, uint32_t aReason)
{
  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("AudioChannelAgent, NotifyStartedAudible, this = %p, "
          "audible = %s, reason = %s\n", this,
          AudibleStateToStr(static_cast<AudioChannelService::AudibleState>(aAudible)),
          AudibleChangedReasonToStr(static_cast<AudioChannelService::AudibleChangedReasons>(aReason))));

  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_FAILURE;
  }

  service->AudioAudibleChanged(
    this,
    static_cast<AudioChannelService::AudibleState>(aAudible),
    static_cast<AudioChannelService::AudibleChangedReasons>(aReason));
  return NS_OK;
}

already_AddRefed<nsIAudioChannelAgentCallback>
AudioChannelAgent::GetCallback()
{
  nsCOMPtr<nsIAudioChannelAgentCallback> callback = mCallback;
  if (!callback) {
    callback = do_QueryReferent(mWeakCallback);
  }
  return callback.forget();
}

void
AudioChannelAgent::WindowVolumeChanged()
{
  nsCOMPtr<nsIAudioChannelAgentCallback> callback = GetCallback();
  if (!callback) {
    return;
  }

  AudioPlaybackConfig config = GetMediaConfig();
  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("AudioChannelAgent, WindowVolumeChanged, this = %p, mute = %s, "
          "volume = %f\n",
          this, config.mMuted ? "true" : "false", config.mVolume));

  callback->WindowVolumeChanged(config.mVolume, config.mMuted);
}

void
AudioChannelAgent::WindowSuspendChanged(nsSuspendedTypes aSuspend)
{
  nsCOMPtr<nsIAudioChannelAgentCallback> callback = GetCallback();
  if (!callback) {
    return;
  }

  if (!IsDisposableSuspend(aSuspend)) {
    aSuspend = GetMediaConfig().mSuspend;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("AudioChannelAgent, WindowSuspendChanged, this = %p, "
          "suspended = %s\n", this, SuspendTypeToStr(aSuspend)));

  callback->WindowSuspendChanged(aSuspend);
}

AudioPlaybackConfig
AudioChannelAgent::GetMediaConfig()
{
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  AudioPlaybackConfig config(1.0, false, nsISuspendedTypes::NONE_SUSPENDED);
  if (service) {
    config = service->GetMediaConfig(mWindow, mAudioChannelType);
  }
  return config;
}

bool
AudioChannelAgent::IsDisposableSuspend(nsSuspendedTypes aSuspend) const
{
  return (aSuspend == nsISuspendedTypes::SUSPENDED_PAUSE_DISPOSABLE ||
          aSuspend == nsISuspendedTypes::SUSPENDED_STOP_DISPOSABLE);
}

uint64_t
AudioChannelAgent::WindowID() const
{
  return mWindow ? mWindow->WindowID() : 0;
}

uint64_t
AudioChannelAgent::InnerWindowID() const
{
  return mInnerWindowID;
}

void
AudioChannelAgent::WindowAudioCaptureChanged(uint64_t aInnerWindowID,
                                             bool aCapture)
{
  if (aInnerWindowID != mInnerWindowID) {
    return;
  }

  nsCOMPtr<nsIAudioChannelAgentCallback> callback = GetCallback();
  if (!callback) {
    return;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("AudioChannelAgent, WindowAudioCaptureChanged, this = %p, "
          "capture = %d\n", this, aCapture));

  callback->WindowAudioCaptureChanged(aCapture);
}

bool
AudioChannelAgent::IsPlayingStarted() const
{
  return mIsRegToService;
}

bool
AudioChannelAgent::ShouldBlockMedia() const
{
  return mWindow ?
    mWindow->GetMediaSuspend() == nsISuspendedTypes::SUSPENDED_BLOCK : false;
}
