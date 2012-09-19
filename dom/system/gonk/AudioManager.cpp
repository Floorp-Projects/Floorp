/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/log.h> 

#include "mozilla/Hal.h"
#include "AudioManager.h"
#include "gonk/AudioSystem.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"

using namespace mozilla::dom::gonk;
using namespace android;
using namespace mozilla::hal;
using namespace mozilla;

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AudioManager" , ## args) 

#define HEADPHONES_STATUS_CHANGED "headphones-status-changed"
#define HEADPHONES_STATUS_ON      NS_LITERAL_STRING("on").get()
#define HEADPHONES_STATUS_OFF     NS_LITERAL_STRING("off").get()
#define HEADPHONES_STATUS_UNKNOWN NS_LITERAL_STRING("unknown").get()

NS_IMPL_ISUPPORTS1(AudioManager, nsIAudioManager)

static AudioSystem::audio_devices
GetRoutingMode(int aType) {
  if (aType == nsIAudioManager::FORCE_SPEAKER) {
    return AudioSystem::DEVICE_OUT_SPEAKER;
  } else if (aType == nsIAudioManager::FORCE_HEADPHONES) {
    return AudioSystem::DEVICE_OUT_WIRED_HEADSET;
  } else if (aType == nsIAudioManager::FORCE_BT_SCO) {
    return AudioSystem::DEVICE_OUT_BLUETOOTH_SCO;
  } else if (aType == nsIAudioManager::FORCE_BT_A2DP) {
    return AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP;
  } else {
    return AudioSystem::DEVICE_IN_DEFAULT;
  }
}

static void
InternalSetAudioRoutes(SwitchState aState)
{
  if (aState == SWITCH_STATE_ON) {
    AudioManager::SetAudioRoute(nsIAudioManager::FORCE_HEADPHONES);
  } else if (aState == SWITCH_STATE_OFF) {
    AudioManager::SetAudioRoute(nsIAudioManager::FORCE_SPEAKER);
  }
}

static void
NotifyHeadphonesStatus(SwitchState aState)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    if (aState == SWITCH_STATE_ON) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_ON);
    } else if (aState == SWITCH_STATE_OFF) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_OFF);
    } else {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_UNKNOWN);
    }
  }
}

class HeadphoneSwitchObserver : public SwitchObserver
{
public:
  void Notify(const SwitchEvent& aEvent) {
    InternalSetAudioRoutes(aEvent.status());
    NotifyHeadphonesStatus(aEvent.status());
  }
};

AudioManager::AudioManager() : mPhoneState(PHONE_STATE_CURRENT),
                 mObserver(new HeadphoneSwitchObserver())
{
  RegisterSwitchObserver(SWITCH_HEADPHONES, mObserver);
  
  InternalSetAudioRoutes(GetCurrentSwitchState(SWITCH_HEADPHONES));
}

AudioManager::~AudioManager() {
  UnregisterSwitchObserver(SWITCH_HEADPHONES, mObserver);
}

NS_IMETHODIMP
AudioManager::GetMicrophoneMuted(bool* aMicrophoneMuted)
{
  if (AudioSystem::isMicrophoneMuted(aMicrophoneMuted)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetMicrophoneMuted(bool aMicrophoneMuted)
{
  if (AudioSystem::muteMicrophone(aMicrophoneMuted)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetMasterVolume(float* aMasterVolume)
{
  if (AudioSystem::getMasterVolume(aMasterVolume)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetMasterVolume(float aMasterVolume)
{
  if (AudioSystem::setMasterVolume(aMasterVolume)) {
    return NS_ERROR_FAILURE;
  }
  // For now, just set the voice volume at the same level
  if (AudioSystem::setVoiceVolume(aMasterVolume)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetMasterMuted(bool* aMasterMuted)
{
  if (AudioSystem::getMasterMute(aMasterMuted)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetMasterMuted(bool aMasterMuted)
{
  if (AudioSystem::setMasterMute(aMasterMuted)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetPhoneState(int32_t* aState)
{
  *aState = mPhoneState;
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetPhoneState(int32_t aState)
{
  if (AudioSystem::setPhoneState(aState)) {
    return NS_ERROR_FAILURE;
  }

  mPhoneState = aState;
  return NS_OK;
}

//
// Kids, don't try this at home.  We want this to link and work on
// both GB and ICS.  Problem is, the symbol exported by audioflinger
// is different on the two gonks.
//
// So what we do here is weakly link to both of them, and then call
// whichever symbol resolves at dynamic link time (if any).
//
NS_IMETHODIMP
AudioManager::SetForceForUse(int32_t aUsage, int32_t aForce)
{
  status_t status = 0;
  if (static_cast<
      status_t (*)(AudioSystem::force_use, AudioSystem::forced_config)
      >(AudioSystem::setForceUse)) {
    // Dynamically resolved the GB signature.
    status = AudioSystem::setForceUse((AudioSystem::force_use)aUsage,
                                      (AudioSystem::forced_config)aForce);
  } else if (static_cast<
             status_t (*)(audio_policy_force_use_t, audio_policy_forced_cfg_t)
             >(AudioSystem::setForceUse)) {
    // Dynamically resolved the ICS signature.
    status = AudioSystem::setForceUse((audio_policy_force_use_t)aUsage,
                                      (audio_policy_forced_cfg_t)aForce);
  }

  return status ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP
AudioManager::GetForceForUse(int32_t aUsage, int32_t* aForce) {
  if (static_cast<
      AudioSystem::forced_config (*)(AudioSystem::force_use)
      >(AudioSystem::getForceUse)) {
    // Dynamically resolved the GB signature.
    *aForce = AudioSystem::getForceUse((AudioSystem::force_use)aUsage);
  } else if (static_cast<
             audio_policy_forced_cfg_t (*)(audio_policy_force_use_t)
             >(AudioSystem::getForceUse)) {
    // Dynamically resolved the ICS signature.
    *aForce = AudioSystem::getForceUse((audio_policy_force_use_t)aUsage);
  }
  return NS_OK;
}

void
AudioManager::SetAudioRoute(int aRoutes) {
  if (static_cast<
      audio_io_handle_t (*)(AudioSystem::stream_type, uint32_t, uint32_t, uint32_t, AudioSystem::output_flags)
      >(AudioSystem::getOutput)) {
    audio_io_handle_t handle = 0;
    handle = AudioSystem::getOutput((AudioSystem::stream_type)AudioSystem::SYSTEM);
    String8 cmd;
    cmd.appendFormat("routing=%d", GetRoutingMode(aRoutes));
    AudioSystem::setParameters(handle, cmd);
  } else if (static_cast<
             status_t (*)(audio_devices_t, audio_policy_dev_state_t, const char*)
             >(AudioSystem::setDeviceConnectionState)) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_WIRED_HEADSET, 
        GetRoutingMode(aRoutes) == AudioSystem::DEVICE_OUT_WIRED_HEADSET ? 
        AUDIO_POLICY_DEVICE_STATE_AVAILABLE : AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE,
        "");
  }
}
