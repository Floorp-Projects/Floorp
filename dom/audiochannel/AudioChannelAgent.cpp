/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelAgent.h"
#include "AudioChannelCommon.h"
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
  , mVisible(true)
  , mWithVideo(false)
{
}

AudioChannelAgent::~AudioChannelAgent()
{
  if (mIsRegToService) {
    StopPlaying();
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

/* void initWithVideo(in nsIDOMWindow window, in long channelType,
 *                    in nsIAudioChannelAgentCallback callback, in boolean weak); */
NS_IMETHODIMP
AudioChannelAgent::InitWithVideo(nsIDOMWindow* aWindow, int32_t aChannelType,
                                 nsIAudioChannelAgentCallback *aCallback,
                                 bool aUseWeakRef)
{
  return InitInternal(aWindow, aChannelType, aCallback, aUseWeakRef,
                      /* withVideo = */ true);
}

nsresult
AudioChannelAgent::InitInternal(nsIDOMWindow* aWindow, int32_t aChannelType,
                                nsIAudioChannelAgentCallback *aCallback,
                                bool aUseWeakRef, bool aWithVideo)
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
    nsCOMPtr<nsPIDOMWindow> pWindow = do_QueryInterface(aWindow);
    if (!pWindow->IsInnerWindow()) {
      pWindow = pWindow->GetCurrentInnerWindow();
    }

    mWindow = pWindow.forget();
  }

  mAudioChannelType = aChannelType;

  if (aUseWeakRef) {
    mWeakCallback = do_GetWeakReference(aCallback);
  } else {
    mCallback = aCallback;
  }

  mWithVideo = aWithVideo;

  return NS_OK;
}

/* boolean startPlaying (); */
NS_IMETHODIMP AudioChannelAgent::StartPlaying(int32_t *_retval)
{
  AudioChannelService *service = AudioChannelService::GetOrCreateAudioChannelService();
  if (mAudioChannelType == AUDIO_AGENT_CHANNEL_ERROR ||
      service == nullptr || mIsRegToService) {
    return NS_ERROR_FAILURE;
  }

  service->RegisterAudioChannelAgent(this,
    static_cast<AudioChannel>(mAudioChannelType), mWithVideo);
  *_retval = service->GetState(this, !mVisible);
  mIsRegToService = true;
  return NS_OK;
}

/* void stopPlaying (); */
NS_IMETHODIMP AudioChannelAgent::StopPlaying(void)
{
  if (mAudioChannelType == AUDIO_AGENT_CHANNEL_ERROR ||
      !mIsRegToService) {
    return NS_ERROR_FAILURE;
  }

  AudioChannelService *service = AudioChannelService::GetOrCreateAudioChannelService();
  service->UnregisterAudioChannelAgent(this);
  mIsRegToService = false;
  return NS_OK;
}

/* void setVisibilityState (in boolean visible); */
NS_IMETHODIMP AudioChannelAgent::SetVisibilityState(bool visible)
{
  bool oldVisibility = mVisible;

  nsCOMPtr<nsIAudioChannelAgentCallback> callback = GetCallback();

  mVisible = visible;
  if (mIsRegToService && oldVisibility != mVisible && callback) {
    AudioChannelService *service = AudioChannelService::GetOrCreateAudioChannelService();
    callback->CanPlayChanged(service->GetState(this, !mVisible));
  }
  return NS_OK;
}

void AudioChannelAgent::NotifyAudioChannelStateChanged()
{
  nsCOMPtr<nsIAudioChannelAgentCallback> callback = GetCallback();
  if (callback) {
    AudioChannelService *service = AudioChannelService::GetOrCreateAudioChannelService();
    callback->CanPlayChanged(service->GetState(this, !mVisible));
  }
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

  callback->WindowVolumeChanged();
}

NS_IMETHODIMP
AudioChannelAgent::GetWindowVolume(float* aVolume)
{
  NS_ENSURE_ARG_POINTER(aVolume);

  if (!mWindow) {
    *aVolume = 1.0f;
    return NS_OK;
  }

  *aVolume = mWindow->GetAudioGlobalVolume();
  return NS_OK;
}
