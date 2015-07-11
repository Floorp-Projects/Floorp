/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelAgent.h"
#include "AudioChannelService.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsXULAppAPI.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION(AudioChannelAgent, mWindow, mCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioChannelAgent)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgent)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AudioChannelAgent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AudioChannelAgent)

AudioChannelAgent::AudioChannelAgent()
  : mAudioChannelType(AUDIO_AGENT_CHANNEL_ERROR)
  , mIsRegToService(false)
{
}

AudioChannelAgent::~AudioChannelAgent()
{
  if (mIsRegToService) {
    NotifyStoppedPlaying();
  }
}

/* readonly attribute long audioChannelType; */
NS_IMETHODIMP AudioChannelAgent::GetAudioChannelType(int32_t *aAudioChannelType)
{
  *aAudioChannelType = mAudioChannelType;
  return NS_OK;
}

/* boolean init (in nsIDOMWindow window, in long channelType,
 *               in nsIAudioChannelAgentCallback callback); */
NS_IMETHODIMP
AudioChannelAgent::Init(nsIDOMWindow* aWindow, int32_t aChannelType,
                        nsIAudioChannelAgentCallback *aCallback)
{
  return InitInternal(aWindow, aChannelType, aCallback,
                      /* useWeakRef = */ false);
}

/* boolean initWithWeakCallback (in nsIDOMWindow window, in long channelType,
 *                               in nsIAudioChannelAgentCallback callback); */
NS_IMETHODIMP
AudioChannelAgent::InitWithWeakCallback(nsIDOMWindow* aWindow,
                                        int32_t aChannelType,
                                        nsIAudioChannelAgentCallback *aCallback)
{
  return InitInternal(aWindow, aChannelType, aCallback,
                      /* useWeakRef = */ true);
}

nsresult
AudioChannelAgent::InitInternal(nsIDOMWindow* aWindow, int32_t aChannelType,
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
             int(AUDIO_AGENT_CHANNEL_PUBLICNOTIFICATION) == int(AudioChannel::Publicnotification),
             "Enum of channel on nsIAudioChannelAgent.idl should be the same with AudioChannelBinding.h");

  if (mAudioChannelType != AUDIO_AGENT_CHANNEL_ERROR ||
      aChannelType > AUDIO_AGENT_CHANNEL_PUBLICNOTIFICATION ||
      aChannelType < AUDIO_AGENT_CHANNEL_NORMAL) {
    return NS_ERROR_FAILURE;
  }

  if (aWindow) {
    nsCOMPtr<nsIDOMWindow> topWindow;
    aWindow->GetScriptableTop(getter_AddRefs(topWindow));
    MOZ_ASSERT(topWindow);

    mWindow = do_QueryInterface(topWindow);
    if (!mWindow) {
      return NS_ERROR_FAILURE;
    }

    mWindow = mWindow->GetOuterWindow();
  }

  mAudioChannelType = aChannelType;

  if (aUseWeakRef) {
    mWeakCallback = do_GetWeakReference(aCallback);
  } else {
    mCallback = aCallback;
  }

  return NS_OK;
}

/* boolean notifyStartedPlaying (); */
NS_IMETHODIMP AudioChannelAgent::NotifyStartedPlaying(float *aVolume,
                                                      bool* aMuted)
{
  MOZ_ASSERT(aVolume);
  MOZ_ASSERT(aMuted);

  nsRefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (mAudioChannelType == AUDIO_AGENT_CHANNEL_ERROR ||
      service == nullptr || mIsRegToService) {
    return NS_ERROR_FAILURE;
  }

  service->RegisterAudioChannelAgent(this,
    static_cast<AudioChannel>(mAudioChannelType));

  service->GetState(mWindow, mAudioChannelType, aVolume, aMuted);

  mIsRegToService = true;
  return NS_OK;
}

/* void notifyStoppedPlaying (); */
NS_IMETHODIMP AudioChannelAgent::NotifyStoppedPlaying(void)
{
  if (mAudioChannelType == AUDIO_AGENT_CHANNEL_ERROR ||
      !mIsRegToService) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  service->UnregisterAudioChannelAgent(this);
  mIsRegToService = false;
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

  float volume = 1.0;
  bool muted = false;

  nsRefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  service->GetState(mWindow, mAudioChannelType, &volume, &muted);

  callback->WindowVolumeChanged(volume, muted);
}

uint64_t
AudioChannelAgent::WindowID() const
{
  return mWindow ? mWindow->WindowID() : 0;
}
