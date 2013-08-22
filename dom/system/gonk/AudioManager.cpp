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

#include "AudioChannelService.h"
#include "AudioManager.h"

#include "nsIObserverService.h"
#include "nsPrintfCString.h"

#include "mozilla/Hal.h"
#include "mozilla/Services.h"
#include "base/message_loop.h"

#include "BluetoothCommon.h"
#include "BluetoothProfileManagerBase.h"
#include "BluetoothHfpManager.h"

#include "nsJSUtils.h"
#include "nsCxPusher.h"

using namespace mozilla::dom::gonk;
using namespace android;
using namespace mozilla::hal;
using namespace mozilla;
using namespace mozilla::dom::bluetooth;

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AudioManager" , ## args)

#define HEADPHONES_STATUS_CHANGED "headphones-status-changed"
#define HEADPHONES_STATUS_HEADSET   NS_LITERAL_STRING("headset").get()
#define HEADPHONES_STATUS_HEADPHONE NS_LITERAL_STRING("headphone").get()
#define HEADPHONES_STATUS_OFF       NS_LITERAL_STRING("off").get()
#define HEADPHONES_STATUS_UNKNOWN   NS_LITERAL_STRING("unknown").get()

static void BinderDeadCallback(status_t aErr);
static void InternalSetAudioRoutes(SwitchState aState);
// Refer AudioService.java from Android
static int sMaxStreamVolumeTbl[AUDIO_STREAM_CNT] = {
  5,   // voice call
  15,  // system
  15,  // ring
  15,  // music
  15,  // alarm
  15,  // notification
  15,  // BT SCO
  15,  // enforced audible
  15,  // DTMF
  15,  // TTS
  15,  // FM
};
// A bitwise variable for recording what kind of headset is attached.
static int sHeadsetState;
static const int kBtSampleRate = 8000;
static bool sSwitchDone = true;

namespace mozilla {
namespace dom {
namespace gonk {
class RecoverTask : public nsRunnable
{
public:
  RecoverTask() {}
  NS_IMETHODIMP Run() {
    nsCOMPtr<nsIAudioManager> amService = do_GetService(NS_AUDIOMANAGER_CONTRACTID);
    NS_ENSURE_TRUE(amService, NS_OK);
    AudioManager *am = static_cast<AudioManager *>(amService.get());
    for (int loop = 0; loop < AUDIO_STREAM_CNT; loop++) {
      AudioSystem::initStreamVolume(static_cast<audio_stream_type_t>(loop), 0,
                                   sMaxStreamVolumeTbl[loop]);
      int32_t index;
      am->GetStreamVolumeIndex(loop, &index);
      am->SetStreamVolumeIndex(loop, index);
    }

    if (sHeadsetState & AUDIO_DEVICE_OUT_WIRED_HEADSET)
      InternalSetAudioRoutes(SWITCH_STATE_HEADSET);
    else if (sHeadsetState & AUDIO_DEVICE_OUT_WIRED_HEADPHONE)
      InternalSetAudioRoutes(SWITCH_STATE_HEADPHONE);
    else
      InternalSetAudioRoutes(SWITCH_STATE_OFF);

    int32_t phoneState = nsIAudioManager::PHONE_STATE_INVALID;
    am->GetPhoneState(&phoneState);
#if ANDROID_VERSION < 17
    AudioSystem::setPhoneState(phoneState);
#else
    AudioSystem::setPhoneState(static_cast<audio_mode_t>(phoneState));
#endif

    AudioSystem::get_audio_flinger();
    return NS_OK;
  }
};
} /* namespace gonk */
} /* namespace dom */
} /* namespace mozilla */

static void
BinderDeadCallback(status_t aErr)
{
  if (aErr == DEAD_OBJECT) {
    NS_DispatchToMainThread(new RecoverTask());
  }
}

static bool
IsDeviceOn(audio_devices_t device)
{
  if (static_cast<
      audio_policy_dev_state_t (*) (audio_devices_t, const char *)
      >(AudioSystem::getDeviceConnectionState))
    return AudioSystem::getDeviceConnectionState(device, "") ==
           AUDIO_POLICY_DEVICE_STATE_AVAILABLE;

  return false;
}

static void ProcessDelayedAudioRoute(SwitchState aState)
{
  if (sSwitchDone)
    return;
  InternalSetAudioRoutes(aState);
  sSwitchDone = true;
}

NS_IMPL_ISUPPORTS2(AudioManager, nsIAudioManager, nsIObserver)

static void
InternalSetAudioRoutesICS(SwitchState aState)
{
  if (aState == SWITCH_STATE_HEADSET) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_WIRED_HEADSET,
                                          AUDIO_POLICY_DEVICE_STATE_AVAILABLE, "");
    sHeadsetState |= AUDIO_DEVICE_OUT_WIRED_HEADSET;
  } else if (aState == SWITCH_STATE_HEADPHONE) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_WIRED_HEADPHONE,
                                          AUDIO_POLICY_DEVICE_STATE_AVAILABLE, "");
    sHeadsetState |= AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
  } else if (aState == SWITCH_STATE_OFF) {
    AudioSystem::setDeviceConnectionState(static_cast<audio_devices_t>(sHeadsetState),
                                          AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, "");
    sHeadsetState = 0;
  }
}

static void
InternalSetAudioRoutes(SwitchState aState)
{
  if (static_cast<
    status_t (*)(audio_devices_t, audio_policy_dev_state_t, const char*)
    >(AudioSystem::setDeviceConnectionState)) {
    InternalSetAudioRoutesICS(aState);
  } else {
    NS_NOTREACHED("Doesn't support audio routing on GB version");
  }
}

void
AudioManager::HandleBluetoothStatusChanged(nsISupports* aSubject,
                                           const char* aTopic,
                                           const nsCString aAddress)
{
#ifdef MOZ_B2G_BT
  bool status;
  if (!strcmp(aTopic, BLUETOOTH_SCO_STATUS_CHANGED_ID)) {
    BluetoothHfpManager* hfp =
      static_cast<BluetoothHfpManager*>(aSubject);
    status = hfp->IsScoConnected();
  } else {
    BluetoothProfileManagerBase* profile =
      static_cast<BluetoothProfileManagerBase*>(aSubject);
    status = profile->IsConnected();
  }

  audio_policy_dev_state_t audioState = status ?
    AUDIO_POLICY_DEVICE_STATE_AVAILABLE :
    AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE;

  if (!strcmp(aTopic, BLUETOOTH_SCO_STATUS_CHANGED_ID)) {
    if (audioState == AUDIO_POLICY_DEVICE_STATE_AVAILABLE) {
      String8 cmd;
      cmd.appendFormat("bt_samplerate=%d", kBtSampleRate);
      AudioSystem::setParameters(0, cmd);
      SetForceForUse(nsIAudioManager::USE_COMMUNICATION, nsIAudioManager::FORCE_BT_SCO);
    } else {
      int32_t force;
      GetForceForUse(nsIAudioManager::USE_COMMUNICATION, &force);
      if (force == nsIAudioManager::FORCE_BT_SCO)
        SetForceForUse(nsIAudioManager::USE_COMMUNICATION, nsIAudioManager::FORCE_NONE);
    }
  } else if (!strcmp(aTopic, BLUETOOTH_A2DP_STATUS_CHANGED_ID)) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP,
                                          audioState, aAddress.get());
    if (audioState == AUDIO_POLICY_DEVICE_STATE_AVAILABLE) {
      String8 cmd("bluetooth_enabled=true");
      AudioSystem::setParameters(0, cmd);
      cmd.setTo("A2dpSuspended=false");
      AudioSystem::setParameters(0, cmd);
    } else {
      String8 cmd("bluetooth_enabled=false");
      AudioSystem::setParameters(0, cmd);
      cmd.setTo("A2dpSuspended=true");
      AudioSystem::setParameters(0, cmd);
    }
  } else if (!strcmp(aTopic, BLUETOOTH_HFP_STATUS_CHANGED_ID)) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET,
                                          audioState, aAddress.get());
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET,
                                          audioState, aAddress.get());
  }
#endif
}

nsresult
AudioManager::Observe(nsISupports* aSubject,
                      const char* aTopic,
                      const PRUnichar* aData)
{
  if ((strcmp(aTopic, BLUETOOTH_SCO_STATUS_CHANGED_ID) == 0) ||
      (strcmp(aTopic, BLUETOOTH_HFP_STATUS_CHANGED_ID) == 0) ||
      (strcmp(aTopic, BLUETOOTH_A2DP_STATUS_CHANGED_ID) == 0)) {
    nsCString address = NS_ConvertUTF16toUTF8(nsDependentString(aData));
    if (address.IsEmpty()) {
      NS_WARNING(nsPrintfCString("Invalid address of %s", aTopic).get());
      return NS_ERROR_FAILURE;
    }

    HandleBluetoothStatusChanged(aSubject, aTopic, address);
    return NS_OK;
  }

  // To process the volume control on each audio channel according to
  // change of settings
  else if (!strcmp(aTopic, "mozsettings-changed")) {
    AutoSafeJSContext cx;
    nsDependentString dataStr(aData);
    JS::Rooted<JS::Value> val(cx);
    if (!JS_ParseJSON(cx, dataStr.get(), dataStr.Length(), &val) ||
        !val.isObject()) {
      return NS_OK;
    }

    JS::Rooted<JSObject*> obj(cx, &val.toObject());
    JS::Rooted<JS::Value> key(cx);
    if (!JS_GetProperty(cx, obj, "key", &key) ||
        !key.isString()) {
      return NS_OK;
    }

    JS::RootedString jsKey(cx, JS_ValueToString(cx, key));
    if (!jsKey) {
      return NS_OK;
    }
    nsDependentJSString keyStr;
    if (!keyStr.init(cx, jsKey) || keyStr.EqualsLiteral("audio.volume.bt_sco")) {
      return NS_OK;
    }

    JS::Rooted<JS::Value> value(cx);
    if (!JS_GetProperty(cx, obj, "value", &value) || !value.isInt32()) {
      return NS_OK;
    }

    int32_t index = value.toInt32();
    SetStreamVolumeIndex(AUDIO_STREAM_BLUETOOTH_SCO, index);

    return NS_OK;
  }

  NS_WARNING("Unexpected topic in AudioManager");
  return NS_ERROR_FAILURE;
}

static void
NotifyHeadphonesStatus(SwitchState aState)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    if (aState == SWITCH_STATE_HEADSET) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_HEADSET);
    } else if (aState == SWITCH_STATE_HEADPHONE) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_HEADPHONE);
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
    NotifyHeadphonesStatus(aEvent.status());
    // When user pulled out the headset, a delay of routing here can avoid the leakage of audio from speaker.
    if (aEvent.status() == SWITCH_STATE_OFF && sSwitchDone) {
      MessageLoop::current()->PostDelayedTask(
        FROM_HERE, NewRunnableFunction(&ProcessDelayedAudioRoute, SWITCH_STATE_OFF), 1000);
      sSwitchDone = false;
    } else if (aEvent.status() != SWITCH_STATE_OFF) {
      InternalSetAudioRoutes(aEvent.status());
      sSwitchDone = true;
    }
  }
};

AudioManager::AudioManager() : mPhoneState(PHONE_STATE_CURRENT),
                 mObserver(new HeadphoneSwitchObserver())
{
  RegisterSwitchObserver(SWITCH_HEADPHONES, mObserver);

  InternalSetAudioRoutes(GetCurrentSwitchState(SWITCH_HEADPHONES));
  NotifyHeadphonesStatus(GetCurrentSwitchState(SWITCH_HEADPHONES));

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->AddObserver(this, BLUETOOTH_SCO_STATUS_CHANGED_ID, false))) {
    NS_WARNING("Failed to add bluetooth sco status changed observer!");
  }
  if (NS_FAILED(obs->AddObserver(this, BLUETOOTH_A2DP_STATUS_CHANGED_ID, false))) {
    NS_WARNING("Failed to add bluetooth a2dp status changed observer!");
  }
  if (NS_FAILED(obs->AddObserver(this, "mozsettings-changed", false))) {
    NS_WARNING("Failed to add mozsettings-changed oberver!");
  }
  if (NS_FAILED(obs->AddObserver(this, BLUETOOTH_HFP_STATUS_CHANGED_ID, false))) {
    NS_WARNING("Failed to add bluetooth hfp status changed observer!");
  }

  for (int loop = 0; loop < AUDIO_STREAM_CNT; loop++) {
    AudioSystem::initStreamVolume(static_cast<audio_stream_type_t>(loop), 0,
                                  sMaxStreamVolumeTbl[loop]);
    mCurrentStreamVolumeTbl[loop] = sMaxStreamVolumeTbl[loop];
  }
  // Force publicnotification to output at maximal volume
  SetStreamVolumeIndex(AUDIO_STREAM_ENFORCED_AUDIBLE,
                       sMaxStreamVolumeTbl[AUDIO_STREAM_ENFORCED_AUDIBLE]);
  // Gecko only control stream volume not master so set to default value
  // directly.
  AudioSystem::setMasterVolume(1.0);

  AudioSystem::setErrorCallback(BinderDeadCallback);
}

AudioManager::~AudioManager() {
  UnregisterSwitchObserver(SWITCH_HEADPHONES, mObserver);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->RemoveObserver(this, BLUETOOTH_SCO_STATUS_CHANGED_ID))) {
    NS_WARNING("Failed to remove bluetooth sco status changed observer!");
  }
  if (NS_FAILED(obs->RemoveObserver(this, BLUETOOTH_A2DP_STATUS_CHANGED_ID))) {
    NS_WARNING("Failed to remove bluetooth a2dp status changed observer!");
  }
  if (NS_FAILED(obs->RemoveObserver(this, BLUETOOTH_HFP_STATUS_CHANGED_ID))) {
    NS_WARNING("Failed to remove bluetooth hfp status changed observer!");
  }
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
AudioManager::GetPhoneState(int32_t* aState)
{
  *aState = mPhoneState;
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetPhoneState(int32_t aState)
{
  if (mPhoneState == aState) {
    return NS_OK;
  }
  // follow the switch audio path logic for android, Bug 897364
  int usage;
  GetForceForUse(nsIAudioManager::USE_COMMUNICATION, &usage);
  if (aState == PHONE_STATE_NORMAL && usage == nsIAudioManager::FORCE_BT_SCO) {
    SetForceForUse(nsIAudioManager::USE_COMMUNICATION, nsIAudioManager::FORCE_NONE);
  }
#if ANDROID_VERSION < 17
  if (AudioSystem::setPhoneState(aState)) {
#else
  if (AudioSystem::setPhoneState(static_cast<audio_mode_t>(aState))) {
#endif
    return NS_ERROR_FAILURE;
  }

  mPhoneState = aState;

  if (mPhoneAudioAgent) {
    mPhoneAudioAgent->StopPlaying();
    mPhoneAudioAgent = nullptr;
  }

  if (aState == PHONE_STATE_IN_CALL || aState == PHONE_STATE_RINGTONE) {
    mPhoneAudioAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1");
    MOZ_ASSERT(mPhoneAudioAgent);
    if (aState == PHONE_STATE_IN_CALL) {
      // Telephony doesn't be paused by any other channels.
      mPhoneAudioAgent->Init(AUDIO_CHANNEL_TELEPHONY, nullptr);
    } else {
      mPhoneAudioAgent->Init(AUDIO_CHANNEL_RINGER, nullptr);
    }

    // Telephony can always play.
    bool canPlay;
    mPhoneAudioAgent->StartPlaying(&canPlay);
  }

  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetForceForUse(int32_t aUsage, int32_t aForce)
{
  if (static_cast<
             status_t (*)(audio_policy_force_use_t, audio_policy_forced_cfg_t)
             >(AudioSystem::setForceUse)) {
    // Dynamically resolved the ICS signature.
    status_t status = AudioSystem::setForceUse(
                        (audio_policy_force_use_t)aUsage,
                        (audio_policy_forced_cfg_t)aForce);
    return status ? NS_ERROR_FAILURE : NS_OK;
  }

  NS_NOTREACHED("Doesn't support force routing on GB version");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
AudioManager::GetForceForUse(int32_t aUsage, int32_t* aForce) {
  if (static_cast<
      audio_policy_forced_cfg_t (*)(audio_policy_force_use_t)
      >(AudioSystem::getForceUse)) {
    // Dynamically resolved the ICS signature.
    *aForce = AudioSystem::getForceUse((audio_policy_force_use_t)aUsage);
    return NS_OK;
  }

  NS_NOTREACHED("Doesn't support force routing on GB version");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
AudioManager::GetFmRadioAudioEnabled(bool *aFmRadioAudioEnabled)
{
  *aFmRadioAudioEnabled = IsDeviceOn(AUDIO_DEVICE_OUT_FM);
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetFmRadioAudioEnabled(bool aFmRadioAudioEnabled)
{
  if (static_cast<
      status_t (*) (AudioSystem::audio_devices, AudioSystem::device_connection_state, const char *)
      >(AudioSystem::setDeviceConnectionState)) {
    AudioSystem::setDeviceConnectionState(AUDIO_DEVICE_OUT_FM,
      aFmRadioAudioEnabled ? AUDIO_POLICY_DEVICE_STATE_AVAILABLE :
      AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, "");
    InternalSetAudioRoutes(GetCurrentSwitchState(SWITCH_HEADPHONES));
    // sync volume with music after powering on fm radio
    if (aFmRadioAudioEnabled) {
      int32_t volIndex = mCurrentStreamVolumeTbl[AUDIO_STREAM_MUSIC];
      SetStreamVolumeIndex(AUDIO_STREAM_FM, volIndex);
      mCurrentStreamVolumeTbl[AUDIO_STREAM_FM] = volIndex;
    }
    return NS_OK;
  } else {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP
AudioManager::SetAudioChannelVolume(int32_t aChannel, int32_t aIndex) {
  status_t status;

  switch (aChannel) {
    case AUDIO_CHANNEL_CONTENT:
      status = SetStreamVolumeIndex(AUDIO_STREAM_MUSIC, aIndex);
      status += SetStreamVolumeIndex(AUDIO_STREAM_SYSTEM, aIndex);
      // sync FMRadio's volume with content channel.
      if (IsDeviceOn(AUDIO_DEVICE_OUT_FM)) {
        status += SetStreamVolumeIndex(AUDIO_STREAM_FM, aIndex);
      }
      break;
    case AUDIO_CHANNEL_NOTIFICATION:
      status = SetStreamVolumeIndex(AUDIO_STREAM_NOTIFICATION, aIndex);
      status += SetStreamVolumeIndex(AUDIO_STREAM_RING, aIndex);
      break;
    case AUDIO_CHANNEL_ALARM:
      status = SetStreamVolumeIndex(AUDIO_STREAM_ALARM, aIndex);
      break;
    case AUDIO_CHANNEL_TELEPHONY:
      status = SetStreamVolumeIndex(AUDIO_STREAM_VOICE_CALL, aIndex);
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  return status ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP
AudioManager::GetAudioChannelVolume(int32_t aChannel, int32_t* aIndex) {
  if (!aIndex) {
    return NS_ERROR_NULL_POINTER;
  }

  switch (aChannel) {
    case AUDIO_CHANNEL_CONTENT:
      MOZ_ASSERT(mCurrentStreamVolumeTbl[AUDIO_STREAM_MUSIC] ==
                 mCurrentStreamVolumeTbl[AUDIO_STREAM_SYSTEM]);
      *aIndex = mCurrentStreamVolumeTbl[AUDIO_STREAM_MUSIC];
      break;
    case AUDIO_CHANNEL_NOTIFICATION:
      MOZ_ASSERT(mCurrentStreamVolumeTbl[AUDIO_STREAM_NOTIFICATION] ==
                 mCurrentStreamVolumeTbl[AUDIO_STREAM_RING]);
      *aIndex = mCurrentStreamVolumeTbl[AUDIO_STREAM_NOTIFICATION];
      break;
    case AUDIO_CHANNEL_ALARM:
      *aIndex = mCurrentStreamVolumeTbl[AUDIO_STREAM_ALARM];
      break;
    case AUDIO_CHANNEL_TELEPHONY:
      *aIndex = mCurrentStreamVolumeTbl[AUDIO_STREAM_VOICE_CALL];
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetMaxAudioChannelVolume(int32_t aChannel, int32_t* aMaxIndex) {
  if (!aMaxIndex) {
    return NS_ERROR_NULL_POINTER;
  }

  int32_t stream;
  switch (aChannel) {
    case AUDIO_CHANNEL_CONTENT:
      MOZ_ASSERT(sMaxStreamVolumeTbl[AUDIO_STREAM_MUSIC] ==
                 sMaxStreamVolumeTbl[AUDIO_STREAM_SYSTEM]);
      stream = AUDIO_STREAM_MUSIC;
      break;
    case AUDIO_CHANNEL_NOTIFICATION:
      MOZ_ASSERT(sMaxStreamVolumeTbl[AUDIO_STREAM_NOTIFICATION] ==
                 sMaxStreamVolumeTbl[AUDIO_STREAM_RING]);
      stream = AUDIO_STREAM_NOTIFICATION;
      break;
    case AUDIO_CHANNEL_ALARM:
      stream = AUDIO_STREAM_ALARM;
      break;
    case AUDIO_CHANNEL_TELEPHONY:
      stream = AUDIO_STREAM_VOICE_CALL;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  *aMaxIndex = sMaxStreamVolumeTbl[stream];
   return NS_OK;
}

status_t
AudioManager::SetStreamVolumeIndex(int32_t aStream, int32_t aIndex) {
  if (aIndex < 0 || aIndex > sMaxStreamVolumeTbl[aStream]) {
    return BAD_VALUE;
  }

  mCurrentStreamVolumeTbl[aStream] = aIndex;
#if ANDROID_VERSION < 17
  return AudioSystem::setStreamVolumeIndex(
           static_cast<audio_stream_type_t>(aStream),
           aIndex);
#else
  int device = 0;

  if (aStream == AUDIO_STREAM_BLUETOOTH_SCO) {
    device = AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET;
  } else if (aStream == AUDIO_STREAM_FM) {
    device = AUDIO_DEVICE_OUT_FM;
  }

  if (device != 0) {
    return AudioSystem::setStreamVolumeIndex(
             static_cast<audio_stream_type_t>(aStream),
             aIndex,
             device);
  }

  status_t status = AudioSystem::setStreamVolumeIndex(
                      static_cast<audio_stream_type_t>(aStream),
                      aIndex,
                      AUDIO_DEVICE_OUT_SPEAKER);
  status += AudioSystem::setStreamVolumeIndex(
              static_cast<audio_stream_type_t>(aStream),
              aIndex,
              AUDIO_DEVICE_OUT_WIRED_HEADSET);
  status += AudioSystem::setStreamVolumeIndex(
              static_cast<audio_stream_type_t>(aStream),
              aIndex,
              AUDIO_DEVICE_OUT_WIRED_HEADPHONE);
  status += AudioSystem::setStreamVolumeIndex(
              static_cast<audio_stream_type_t>(aStream),
              aIndex,
              AUDIO_DEVICE_OUT_EARPIECE);
  return status;
#endif
}

status_t
AudioManager::GetStreamVolumeIndex(int32_t aStream, int32_t *aIndex) {
  if (!aIndex) {
    return BAD_VALUE;
  }

  if (aStream <= AUDIO_STREAM_DEFAULT || aStream >= AUDIO_STREAM_MAX) {
    return BAD_VALUE;
  }

  *aIndex = mCurrentStreamVolumeTbl[aStream];

  return NO_ERROR;
}
