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
#include <cutils/properties.h>
#include <binder/IServiceManager.h>

#include "AudioChannelService.h"
#include "AudioManager.h"

#include "nsIObserverService.h"
#ifdef MOZ_B2G_RIL
#include "nsIRadioInterfaceLayer.h"
#endif
#include "nsISettingsService.h"
#include "nsPrintfCString.h"

#include "mozilla/Hal.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "base/message_loop.h"

#include "BluetoothCommon.h"
#include "BluetoothHfpManagerBase.h"

#include "nsJSUtils.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/SettingChangeNotificationBinding.h"

using namespace mozilla::dom;
using namespace mozilla::dom::gonk;
using namespace android;
using namespace mozilla;
using namespace mozilla::dom::bluetooth;

#undef LOG
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AudioManager" , ## args)

#define HEADPHONES_STATUS_HEADSET     u"headset"
#define HEADPHONES_STATUS_HEADPHONE   u"headphone"
#define HEADPHONES_STATUS_OFF         u"off"
#define HEADPHONES_STATUS_UNKNOWN     u"unknown"
#define HEADPHONES_STATUS_CHANGED     "headphones-status-changed"
#define MOZ_SETTINGS_CHANGE_ID        "mozsettings-changed"
#define AUDIO_CHANNEL_PROCESS_CHANGED "audio-channel-process-changed"
#define AUDIO_POLICY_SERVICE_NAME     "media.audio_policy"
#define SETTINGS_SERVICE              "@mozilla.org/settingsService;1"

// Refer AudioService.java from Android
static const uint32_t sMaxStreamVolumeTbl[AUDIO_STREAM_CNT] = {
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
#if ANDROID_VERSION < 19
  15,  // FM
#endif
};

static const uint32_t sDefaultStreamVolumeTbl[AUDIO_STREAM_CNT] = {
  3,  // voice call
  8,  // system
  8,  // ring
  8,  // music
  8,  // alarm
  8,  // notification
  8,  // BT SCO
  15, // enforced audible  // XXX Handle as fixed maximum audio setting
  8,  // DTMF
  8,  // TTS
#if ANDROID_VERSION < 19
  8,  // FM
#endif
};

static const int32_t sStreamVolumeAliasTbl[AUDIO_STREAM_CNT] = {
  AUDIO_STREAM_VOICE_CALL,      // voice call
  AUDIO_STREAM_NOTIFICATION,    // system
  AUDIO_STREAM_NOTIFICATION,    // ring
  AUDIO_STREAM_MUSIC,           // music
  AUDIO_STREAM_ALARM,           // alarm
  AUDIO_STREAM_NOTIFICATION,    // notification
  AUDIO_STREAM_BLUETOOTH_SCO,   // BT SCO
  AUDIO_STREAM_ENFORCED_AUDIBLE,// enforced audible
  AUDIO_STREAM_DTMF,            // DTMF
  AUDIO_STREAM_TTS,             // TTS
#if ANDROID_VERSION < 19
  AUDIO_STREAM_MUSIC,           // FM
#endif
};

static const uint32_t sChannelStreamTbl[NUMBER_OF_AUDIO_CHANNELS] = {
  AUDIO_STREAM_MUSIC,           // AudioChannel::Normal
  AUDIO_STREAM_MUSIC,           // AudioChannel::Content
  AUDIO_STREAM_NOTIFICATION,    // AudioChannel::Notification
  AUDIO_STREAM_ALARM,           // AudioChannel::Alarm
  AUDIO_STREAM_VOICE_CALL,      // AudioChannel::Telephony
  AUDIO_STREAM_RING,            // AudioChannel::Ringer
  AUDIO_STREAM_ENFORCED_AUDIBLE,// AudioChannel::Publicnotification
  AUDIO_STREAM_SYSTEM,          // AudioChannel::System
};


struct AudioDeviceInfo {
  /** The string the value maps to */
  const char* tag;
  /** The enum value that maps to this string */
  uint32_t value;
};

// Mappings audio output devices to strings.
static const AudioDeviceInfo kAudioDeviceInfos[] = {
  { "earpiece",        AUDIO_DEVICE_OUT_EARPIECE },
  { "speaker",         AUDIO_DEVICE_OUT_SPEAKER },
  { "wired_headset",   AUDIO_DEVICE_OUT_WIRED_HEADSET },
  { "wired_headphone", AUDIO_DEVICE_OUT_WIRED_HEADPHONE },
  { "bt_scoheadset",   AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET },
  { "bt_a2dp",         AUDIO_DEVICE_OUT_BLUETOOTH_A2DP },
};

static const int kBtSampleRate = 8000;

typedef MozPromise<bool, const char*, true> VolumeInitPromise;

namespace mozilla {
namespace dom {
namespace gonk {

/**
 * We have five sound volume settings from UX spec,
 * You can see more informations in Bug1068219.
 * (1) Media : music, video, FM ...
 * (2) Notification : ringer, notification ...
 * (3) Alarm : alarm
 * (4) Telephony : GSM call, WebRTC call
 * (5) Bluetooth SCO : SCO call
 **/
struct VolumeData {
  const char* mChannelName;
  int32_t mStreamType;
};

static const VolumeData gVolumeData[] = {
  {"audio.volume.content",      AUDIO_STREAM_MUSIC},
  {"audio.volume.notification", AUDIO_STREAM_NOTIFICATION},
  {"audio.volume.alarm",        AUDIO_STREAM_ALARM},
  {"audio.volume.telephony",    AUDIO_STREAM_VOICE_CALL},
  {"audio.volume.bt_sco",       AUDIO_STREAM_BLUETOOTH_SCO}
};

class RunnableCallTask : public Runnable
{
public:
  explicit RunnableCallTask(nsIRunnable* aRunnable)
    : mRunnable(aRunnable) {}

  NS_IMETHOD Run() override
  {
    return mRunnable->Run();
  }
protected:
  nsCOMPtr<nsIRunnable> mRunnable;
};

nsCOMPtr<nsISettingsServiceLock>
GetSettingServiceLock()
{
  nsresult rv;
  nsCOMPtr<nsISettingsService> service = do_GetService(SETTINGS_SERVICE, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsCOMPtr<nsISettingsServiceLock> lock;
  rv = service->CreateLock(nullptr, getter_AddRefs(lock));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return lock.forget();
}

#if ANDROID_VERSION >= 21
class GonkAudioPortCallback : public AudioSystem::AudioPortCallback
{
public:
  virtual void onAudioPortListUpdate()
  {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction([]() {
        MOZ_ASSERT(NS_IsMainThread());
        RefPtr<AudioManager> audioManager = AudioManager::GetInstance();
        NS_ENSURE_TRUE(audioManager.get(), );
        audioManager->UpdateCachedActiveDevicesForStreams();
        audioManager->MaybeUpdateVolumeSettingToDatabase();
      });
    NS_DispatchToMainThread(runnable);
  }
  virtual void onAudioPatchListUpdate() { }
  virtual void onServiceDied() { }
};
#endif

void
AudioManager::HandleAudioFlingerDied()
{
  //Disable volume change notification
  mIsVolumeInited = false;

  uint32_t attempt;
  for (attempt = 0; attempt < 50; attempt++) {
    if (defaultServiceManager()->checkService(String16(AUDIO_POLICY_SERVICE_NAME)) != 0) {
      break;
    }
    LOG("AudioPolicyService is dead! attempt=%d", attempt);
    usleep(1000 * 200);
  }

  MOZ_RELEASE_ASSERT(attempt < 50);

  // Indicate to audio HAL that we start the reconfiguration phase after a media
  // server crash
  AudioSystem::setParameters(0, String8("restarting=true"));

  // Restore device connection states
  SetAllDeviceConnectionStates();

  // Restore call state
#if ANDROID_VERSION < 17
  AudioSystem::setPhoneState(mPhoneState);
#else
  AudioSystem::setPhoneState(static_cast<audio_mode_t>(mPhoneState));
#endif

  // Restore master volume
  AudioSystem::setMasterVolume(1.0);

  // Restore stream volumes
  for (uint32_t streamType = 0; streamType < AUDIO_STREAM_CNT; ++streamType) {
    mStreamStates[streamType]->InitStreamVolume();
    mStreamStates[streamType]->RestoreVolumeIndexToAllDevices();
  }

  // Indicate the end of reconfiguration phase to audio HAL
  AudioSystem::setParameters(0, String8("restarting=true"));

  // Enable volume change notification
  mIsVolumeInited = true;
  mAudioOutDevicesUpdated = 0;
  MaybeUpdateVolumeSettingToDatabase(true);
}

class VolumeInitCallback final : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  VolumeInitCallback()
    : mInitCounter(0)
  {
    mPromise = mPromiseHolder.Ensure(__func__);
  }

  RefPtr<VolumeInitPromise> GetPromise() const
  {
    return mPromise;
  }

  NS_IMETHOD Handle(const nsAString& aName, JS::Handle<JS::Value> aResult)
  {
    RefPtr<AudioManager> audioManager = AudioManager::GetInstance();
    MOZ_ASSERT(audioManager);
    for (uint32_t idx = 0; idx < MOZ_ARRAY_LENGTH(gVolumeData); ++idx) {
      NS_ConvertASCIItoUTF16 volumeType(gVolumeData[idx].mChannelName);
      if (StringBeginsWith(aName, volumeType)) {
        uint32_t device = GetDeviceFromSettingName(aName);
        MOZ_ASSERT(device != AUDIO_DEVICE_NONE);
        if (aResult.isInt32()) {
          int32_t stream = gVolumeData[idx].mStreamType;
          uint32_t volIndex = aResult.toInt32();
          nsresult rv = audioManager->ValidateVolumeIndex(stream, volIndex);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            mPromiseHolder.Reject("Error : invalid volume index.", __func__);
            return rv;
          }
          audioManager->SetStreamVolumeForDevice(stream, volIndex, device);
        }

        if (++mInitCounter == MOZ_ARRAY_LENGTH(kAudioDeviceInfos) * MOZ_ARRAY_LENGTH(gVolumeData)) {
          mPromiseHolder.Resolve(true, __func__);
        }
        return NS_OK;
      }
    }
    mPromiseHolder.Reject("Error : unexpected audio init event.", __func__);
    return NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString& aName)
  {
    mPromiseHolder.Reject(NS_ConvertUTF16toUTF8(aName).get(), __func__);
    return NS_OK;
  }

protected:
  ~VolumeInitCallback() {}

  uint32_t GetDeviceFromSettingName(const nsAString& aName) const
  {
    for (uint32_t idx = 0; idx < MOZ_ARRAY_LENGTH(kAudioDeviceInfos); ++idx) {
      NS_ConvertASCIItoUTF16 device(kAudioDeviceInfos[idx].tag);
      if (StringEndsWith(aName, device)) {
        return kAudioDeviceInfos[idx].value;
      }
    }
    return AUDIO_DEVICE_NONE;
  }

  RefPtr<VolumeInitPromise> mPromise;
  MozPromiseHolder<VolumeInitPromise> mPromiseHolder;
  uint32_t mInitCounter;
};

NS_IMPL_ISUPPORTS(VolumeInitCallback, nsISettingsServiceCallback)

static void
BinderDeadCallback(status_t aErr)
{
  if (aErr != DEAD_OBJECT) {
    return;
  }

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableFunction([]() {
      MOZ_ASSERT(NS_IsMainThread());
      RefPtr<AudioManager> audioManager = AudioManager::GetInstance();
      NS_ENSURE_TRUE(audioManager.get(), );
      audioManager->HandleAudioFlingerDied();
    });

  NS_DispatchToMainThread(runnable);
}

bool
AudioManager::IsFmOutConnected()
{
  return mConnectedDevices.Get(AUDIO_DEVICE_OUT_FM, nullptr);
}

NS_IMPL_ISUPPORTS(AudioManager, nsIAudioManager, nsIObserver)

void
AudioManager::AudioOutDeviceUpdated(uint32_t aDevice)
{
  MOZ_ASSERT(audio_is_output_device(aDevice));
  mAudioOutDevicesUpdated |= aDevice;
}

void
AudioManager::UpdateHeadsetConnectionState(hal::SwitchState aState)
{
  bool headphoneConnected = mConnectedDevices.Get(AUDIO_DEVICE_OUT_WIRED_HEADPHONE,
                                                  nullptr);
  bool headsetConnected = mConnectedDevices.Get(AUDIO_DEVICE_OUT_WIRED_HEADSET,
                                                nullptr);
  if (aState == hal::SWITCH_STATE_HEADSET) {
    UpdateDeviceConnectionState(true,
                                AUDIO_DEVICE_OUT_WIRED_HEADSET,
                                NS_LITERAL_CSTRING(""));
  } else if (aState == hal::SWITCH_STATE_HEADPHONE) {
    UpdateDeviceConnectionState(true,
                                AUDIO_DEVICE_OUT_WIRED_HEADPHONE,
                                NS_LITERAL_CSTRING(""));
  } else if (aState == hal::SWITCH_STATE_OFF) {
    if (headsetConnected) {
      UpdateDeviceConnectionState(false,
                                  AUDIO_DEVICE_OUT_WIRED_HEADSET,
                                  NS_LITERAL_CSTRING(""));
    }
    if (headphoneConnected) {
      UpdateDeviceConnectionState(false,
                                  AUDIO_DEVICE_OUT_WIRED_HEADPHONE,
                                  NS_LITERAL_CSTRING(""));
    }
  }
}

void
AudioManager::UpdateDeviceConnectionState(bool aIsConnected, uint32_t aDevice, const nsCString& aDeviceName)
{
#if ANDROID_VERSION >= 15
  bool isConnected = mConnectedDevices.Get(aDevice, nullptr);
  if (isConnected && !aIsConnected) {
    AudioSystem::setDeviceConnectionState(static_cast<audio_devices_t>(aDevice),
                                          AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE,
                                          aDeviceName.get());
    mConnectedDevices.Remove(aDevice);
  } else if(!isConnected && aIsConnected) {
    AudioSystem::setDeviceConnectionState(static_cast<audio_devices_t>(aDevice),
                                          AUDIO_POLICY_DEVICE_STATE_AVAILABLE,
                                          aDeviceName.get());
    mConnectedDevices.Put(aDevice, aDeviceName);
  }
#if ANDROID_VERSION < 21
  // Manually call it, since AudioPortCallback is not supported.
  // Current volumes might be changed by updating active devices in android
  // AudioPolicyManager.
  MaybeUpdateVolumeSettingToDatabase();
#endif
#else
  NS_NOTREACHED("Doesn't support audio routing on GB version");
#endif
}

void
AudioManager::SetAllDeviceConnectionStates()
{
  for (auto iter = mConnectedDevices.Iter(); !iter.Done(); iter.Next()) {
    const uint32_t& device = iter.Key();
    nsCString& deviceAddress = iter.Data();
    AudioSystem::setDeviceConnectionState(static_cast<audio_devices_t>(device),
                                          AUDIO_POLICY_DEVICE_STATE_AVAILABLE,
                                          deviceAddress.get());
  }
#if ANDROID_VERSION < 21
  // Manually call it, since AudioPortCallback is not supported.
  // Current volumes might be changed by updating active devices in android
  // AudioPolicyManager.
  MaybeUpdateVolumeSettingToDatabase(true);
#endif
}

void
AudioManager::HandleBluetoothStatusChanged(nsISupports* aSubject,
                                           const char* aTopic,
                                           const nsCString aAddress)
{
#ifdef MOZ_B2G_BT
  bool isConnected = false;
  if (!strcmp(aTopic, BLUETOOTH_SCO_STATUS_CHANGED_ID)) {
    BluetoothHfpManagerBase* hfp =
      static_cast<BluetoothHfpManagerBase*>(aSubject);
    isConnected = hfp->IsScoConnected();
  } else {
    BluetoothProfileManagerBase* profile =
      static_cast<BluetoothProfileManagerBase*>(aSubject);
    isConnected = profile->IsConnected();
  }

  if (!strcmp(aTopic, BLUETOOTH_SCO_STATUS_CHANGED_ID)) {
    if (isConnected) {
      String8 cmd;
      cmd.appendFormat("bt_samplerate=%d", kBtSampleRate);
      AudioSystem::setParameters(0, cmd);
      SetForceForUse(nsIAudioManager::USE_COMMUNICATION, nsIAudioManager::FORCE_BT_SCO);
    } else {
      int32_t force;
      GetForceForUse(nsIAudioManager::USE_COMMUNICATION, &force);
      if (force == nsIAudioManager::FORCE_BT_SCO) {
        SetForceForUse(nsIAudioManager::USE_COMMUNICATION, nsIAudioManager::FORCE_NONE);
      }
    }
  } else if (!strcmp(aTopic, BLUETOOTH_A2DP_STATUS_CHANGED_ID)) {
    if (!isConnected && mA2dpSwitchDone) {
      RefPtr<AudioManager> self = this;
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction([self, isConnected, aAddress]() {
          if (self->mA2dpSwitchDone) {
            return;
          }
          self->UpdateDeviceConnectionState(isConnected,
                                            AUDIO_DEVICE_OUT_BLUETOOTH_A2DP,
                                            aAddress);

          String8 cmd("bluetooth_enabled=false");
          AudioSystem::setParameters(0, cmd);
          cmd.setTo("A2dpSuspended=true");
          AudioSystem::setParameters(0, cmd);
          self->mA2dpSwitchDone = true;
        });
      MessageLoop::current()->PostDelayedTask(
        MakeAndAddRef<RunnableCallTask>(runnable), 1000);

      mA2dpSwitchDone = false;
    } else {
      UpdateDeviceConnectionState(isConnected,
                                  AUDIO_DEVICE_OUT_BLUETOOTH_A2DP,
                                  aAddress);
      String8 cmd("bluetooth_enabled=true");
      AudioSystem::setParameters(0, cmd);
      cmd.setTo("A2dpSuspended=false");
      AudioSystem::setParameters(0, cmd);
      mA2dpSwitchDone = true;
#if ANDROID_VERSION >= 17
      if (AudioSystem::getForceUse(AUDIO_POLICY_FORCE_FOR_MEDIA) == AUDIO_POLICY_FORCE_NO_BT_A2DP) {
        SetForceForUse(AUDIO_POLICY_FORCE_FOR_MEDIA, AUDIO_POLICY_FORCE_NONE);
      }
#endif
    }
    mBluetoothA2dpEnabled = isConnected;
  } else if (!strcmp(aTopic, BLUETOOTH_HFP_STATUS_CHANGED_ID)) {
    UpdateDeviceConnectionState(isConnected,
                                AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET,
                                aAddress);
    UpdateDeviceConnectionState(isConnected,
                                AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET,
                                aAddress);
  } else if (!strcmp(aTopic, BLUETOOTH_HFP_NREC_STATUS_CHANGED_ID)) {
      String8 cmd;
      BluetoothHfpManagerBase* hfp =
          static_cast<BluetoothHfpManagerBase*>(aSubject);
      if (hfp->IsNrecEnabled()) {
          cmd.setTo("bt_headset_name=<unknown>;bt_headset_nrec=on");
          AudioSystem::setParameters(0, cmd);
      } else {
          cmd.setTo("bt_headset_name=<unknown>;bt_headset_nrec=off");
          AudioSystem::setParameters(0, cmd);
      }
  }
#endif
}

void
AudioManager::HandleAudioChannelProcessChanged()
{
  // Note: If the user answers a VoIP call (e.g. WebRTC calls) during the
  // telephony call (GSM/CDMA calls) the audio manager won't set the
  // PHONE_STATE_IN_COMMUNICATION audio state. Once the telephony call finishes
  // the RIL plumbing sets the PHONE_STATE_NORMAL audio state. This seems to be
  // an issue for the VoIP call but it is not. Once the RIL plumbing sets the
  // the PHONE_STATE_NORMAL audio state the AudioManager::mPhoneAudioAgent
  // member will call the NotifyStoppedPlaying() method causing that this function will
  // be called again and therefore the audio manager sets the
  // PHONE_STATE_IN_COMMUNICATION audio state.

  if ((mPhoneState == PHONE_STATE_IN_CALL) ||
      (mPhoneState == PHONE_STATE_RINGTONE)) {
    return;
  }

  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  bool telephonyChannelIsActive = service && service->TelephonyChannelIsActive();
  telephonyChannelIsActive ? SetPhoneState(PHONE_STATE_IN_COMMUNICATION) :
                             SetPhoneState(PHONE_STATE_NORMAL);
}

nsresult
AudioManager::Observe(nsISupports* aSubject,
                      const char* aTopic,
                      const char16_t* aData)
{
  if ((strcmp(aTopic, BLUETOOTH_SCO_STATUS_CHANGED_ID) == 0) ||
      (strcmp(aTopic, BLUETOOTH_HFP_STATUS_CHANGED_ID) == 0) ||
      (strcmp(aTopic, BLUETOOTH_HFP_NREC_STATUS_CHANGED_ID) == 0) ||
      (strcmp(aTopic, BLUETOOTH_A2DP_STATUS_CHANGED_ID) == 0)) {
    nsCString address = NS_ConvertUTF16toUTF8(nsDependentString(aData));
    if (address.IsEmpty()) {
      NS_WARNING(nsPrintfCString("Invalid address of %s", aTopic).get());
      return NS_ERROR_FAILURE;
    }

    HandleBluetoothStatusChanged(aSubject, aTopic, address);
    return NS_OK;
  }

  else if (!strcmp(aTopic, AUDIO_CHANNEL_PROCESS_CHANGED)) {
    HandleAudioChannelProcessChanged();
    return NS_OK;
  }

  // To process the volume control on each volume categories according to
  // change of settings
  else if (!strcmp(aTopic, MOZ_SETTINGS_CHANGE_ID)) {
    RootedDictionary<dom::SettingChangeNotification> setting(RootingCx());
    if (!WrappedJSToDictionary(aSubject, setting)) {
      return NS_OK;
    }
    if (!StringBeginsWith(setting.mKey, NS_LITERAL_STRING("audio.volume."))) {
      return NS_OK;
    }
    if (!setting.mValue.isNumber()) {
      return NS_OK;
    }

    uint32_t volIndex = setting.mValue.toNumber();
    for (uint32_t idx = 0; idx < MOZ_ARRAY_LENGTH(gVolumeData); ++idx) {
      if (setting.mKey.EqualsASCII(gVolumeData[idx].mChannelName)) {
        SetStreamVolumeIndex(gVolumeData[idx].mStreamType, volIndex);
        return NS_OK;
      }
    }
  }

  NS_WARNING("Unexpected topic in AudioManager");
  return NS_ERROR_FAILURE;
}

static void
NotifyHeadphonesStatus(hal::SwitchState aState)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    if (aState == hal::SWITCH_STATE_HEADSET) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_HEADSET);
    } else if (aState == hal::SWITCH_STATE_HEADPHONE) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_HEADPHONE);
    } else if (aState == hal::SWITCH_STATE_OFF) {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_OFF);
    } else {
      obs->NotifyObservers(nullptr, HEADPHONES_STATUS_CHANGED, HEADPHONES_STATUS_UNKNOWN);
    }
  }
}

class HeadphoneSwitchObserver : public hal::SwitchObserver
{
public:
  void Notify(const hal::SwitchEvent& aEvent) {
    RefPtr<AudioManager> audioManager = AudioManager::GetInstance();
    MOZ_ASSERT(audioManager);
    audioManager->HandleHeadphoneSwitchEvent(aEvent);
  }
};

void
AudioManager::HandleHeadphoneSwitchEvent(const hal::SwitchEvent& aEvent)
{
  NotifyHeadphonesStatus(aEvent.status());
  // When user pulled out the headset, a delay of routing here can avoid the leakage of audio from speaker.
  if (aEvent.status() == hal::SWITCH_STATE_OFF && mSwitchDone) {

    RefPtr<AudioManager> self = this;
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction([self]() {
        if (self->mSwitchDone) {
          return;
        }
        self->UpdateHeadsetConnectionState(hal::SWITCH_STATE_OFF);
        self->mSwitchDone = true;
    });
    MessageLoop::current()->PostDelayedTask(
      MakeAndAddRef<RunnableCallTask>(runnable), 1000);
    mSwitchDone = false;
  } else if (aEvent.status() != hal::SWITCH_STATE_OFF) {
    UpdateHeadsetConnectionState(aEvent.status());
    mSwitchDone = true;
  }
  // Handle the coexistence of a2dp / headset device, latest one wins.
#if ANDROID_VERSION >= 17
  int32_t forceUse = 0;
  GetForceForUse(AUDIO_POLICY_FORCE_FOR_MEDIA, &forceUse);
  if (aEvent.status() != hal::SWITCH_STATE_OFF && mBluetoothA2dpEnabled) {
    SetForceForUse(AUDIO_POLICY_FORCE_FOR_MEDIA, AUDIO_POLICY_FORCE_NO_BT_A2DP);
  } else if (forceUse == AUDIO_POLICY_FORCE_NO_BT_A2DP) {
    SetForceForUse(AUDIO_POLICY_FORCE_FOR_MEDIA, AUDIO_POLICY_FORCE_NONE);
  }
#endif
}

AudioManager::AudioManager()
  : mPhoneState(PHONE_STATE_CURRENT)
  , mIsVolumeInited(false)
  , mAudioOutDevicesUpdated(0)
  , mSwitchDone(true)
#if defined(MOZ_B2G_BT) || ANDROID_VERSION >= 17
  , mBluetoothA2dpEnabled(false)
#endif
#ifdef MOZ_B2G_BT
  , mA2dpSwitchDone(true)
#endif
  , mObserver(new HeadphoneSwitchObserver())
#ifdef MOZ_B2G_RIL
  , mMuteCallToRIL(false)
#endif
{
  for (uint32_t idx = 0; idx < MOZ_ARRAY_LENGTH(kAudioDeviceInfos); ++idx) {
    mAudioDeviceTableIdMaps.Put(kAudioDeviceInfos[idx].value, idx);
  }

  AudioSystem::setErrorCallback(BinderDeadCallback);
#if ANDROID_VERSION >= 21
  android::sp<GonkAudioPortCallback> callback = new GonkAudioPortCallback();
  AudioSystem::setAudioPortCallback(callback);
#endif

  // Create VolumeStreamStates
  for (uint32_t loop = 0; loop < AUDIO_STREAM_CNT; ++loop) {
    VolumeStreamState* streamState =
      new VolumeStreamState(*this, static_cast<audio_stream_type_t>(loop));
    mStreamStates.AppendElement(streamState);
  }
  // Initialize stream volumes with default values
  for (int32_t streamType = 0; streamType < AUDIO_STREAM_MAX; streamType++) {
      uint32_t volIndex = sDefaultStreamVolumeTbl[streamType];
      SetStreamVolumeForDevice(streamType, volIndex, AUDIO_DEVICE_OUT_DEFAULT);
  }
  UpdateCachedActiveDevicesForStreams();

  RegisterSwitchObserver(hal::SWITCH_HEADPHONES, mObserver);
  // Initialize headhone/heaset status
  UpdateHeadsetConnectionState(hal::GetCurrentSwitchState(hal::SWITCH_HEADPHONES));
  NotifyHeadphonesStatus(hal::GetCurrentSwitchState(hal::SWITCH_HEADPHONES));

  // Get the initial volume index from settings DB during boot up.
  InitVolumeFromDatabase();

  // Gecko only control stream volume not master so set to default value
  // directly.
  AudioSystem::setMasterVolume(1.0);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->AddObserver(this, BLUETOOTH_SCO_STATUS_CHANGED_ID, false))) {
    NS_WARNING("Failed to add bluetooth sco status changed observer!");
  }
  if (NS_FAILED(obs->AddObserver(this, BLUETOOTH_A2DP_STATUS_CHANGED_ID, false))) {
    NS_WARNING("Failed to add bluetooth a2dp status changed observer!");
  }
  if (NS_FAILED(obs->AddObserver(this, BLUETOOTH_HFP_STATUS_CHANGED_ID, false))) {
    NS_WARNING("Failed to add bluetooth hfp status changed observer!");
  }
  if (NS_FAILED(obs->AddObserver(this, BLUETOOTH_HFP_NREC_STATUS_CHANGED_ID, false))) {
    NS_WARNING("Failed to add bluetooth hfp NREC status changed observer!");
  }
  if (NS_FAILED(obs->AddObserver(this, MOZ_SETTINGS_CHANGE_ID, false))) {
    NS_WARNING("Failed to add mozsettings-changed observer!");
  }
  if (NS_FAILED(obs->AddObserver(this, AUDIO_CHANNEL_PROCESS_CHANGED, false))) {
    NS_WARNING("Failed to add audio-channel-process-changed observer!");
  }

#ifdef MOZ_B2G_RIL
  char value[PROPERTY_VALUE_MAX];
  property_get("ro.moz.mute.call.to_ril", value, "false");
  if (!strcmp(value, "true")) {
    mMuteCallToRIL = true;
  }
#endif
}

AudioManager::~AudioManager() {
  AudioSystem::setErrorCallback(nullptr);
#if ANDROID_VERSION >= 21
  AudioSystem::setAudioPortCallback(nullptr);
#endif
  hal::UnregisterSwitchObserver(hal::SWITCH_HEADPHONES, mObserver);

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
  if (NS_FAILED(obs->RemoveObserver(this, BLUETOOTH_HFP_NREC_STATUS_CHANGED_ID))) {
    NS_WARNING("Failed to remove bluetooth hfp NREC status changed observer!");
  }
  if (NS_FAILED(obs->RemoveObserver(this, MOZ_SETTINGS_CHANGE_ID))) {
    NS_WARNING("Failed to remove mozsettings-changed observer!");
  }
  if (NS_FAILED(obs->RemoveObserver(this,  AUDIO_CHANNEL_PROCESS_CHANGED))) {
    NS_WARNING("Failed to remove audio-channel-process-changed!");
  }
}

static StaticRefPtr<AudioManager> sAudioManager;

already_AddRefed<AudioManager>
AudioManager::GetInstance()
{
  // Avoid createing AudioManager from content process.
  if (!XRE_IsParentProcess()) {
    MOZ_CRASH("Non-chrome processes should not get here.");
  }

  // Avoid createing multiple AudioManager instance inside main process.
  if (!sAudioManager) {
    sAudioManager = new AudioManager();
    ClearOnShutdown(&sAudioManager);
  }

  RefPtr<AudioManager> audioMgr = sAudioManager.get();
  return audioMgr.forget();
}

NS_IMETHODIMP
AudioManager::GetMicrophoneMuted(bool* aMicrophoneMuted)
{
#ifdef MOZ_B2G_RIL
  if (mMuteCallToRIL) {
    // Simply return cached mIsMicMuted if mute call go via RIL.
    *aMicrophoneMuted = mIsMicMuted;
    return NS_OK;
  }
#endif

  if (AudioSystem::isMicrophoneMuted(aMicrophoneMuted)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetMicrophoneMuted(bool aMicrophoneMuted)
{
  if (!AudioSystem::muteMicrophone(aMicrophoneMuted)) {
#ifdef MOZ_B2G_RIL
    if (mMuteCallToRIL) {
      // Extra mute request to RIL for specific platform.
      nsCOMPtr<nsIRadioInterfaceLayer> ril = do_GetService("@mozilla.org/ril;1");
      NS_ENSURE_TRUE(ril, NS_ERROR_FAILURE);
      ril->SetMicrophoneMuted(aMicrophoneMuted);
      mIsMicMuted = aMicrophoneMuted;
    }
#endif
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    nsString state;
    state.AppendInt(aState);
    obs->NotifyObservers(nullptr, "phone-state-changed", state.get());
  }

#if ANDROID_VERSION < 17
  if (AudioSystem::setPhoneState(aState)) {
#else
  if (AudioSystem::setPhoneState(static_cast<audio_mode_t>(aState))) {
#endif
    return NS_ERROR_FAILURE;
  }

#if ANDROID_VERSION < 21
  // Manually call it, since AudioPortCallback is not supported.
  // Current volumes might be changed by updating active devices in android
  // AudioPolicyManager.
  MaybeUpdateVolumeSettingToDatabase();
#endif
  mPhoneState = aState;
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetForceForUse(int32_t aUsage, int32_t aForce)
{
#if ANDROID_VERSION >= 15
  status_t status = AudioSystem::setForceUse(
                      (audio_policy_force_use_t)aUsage,
                      (audio_policy_forced_cfg_t)aForce);
#if ANDROID_VERSION < 21
  // Manually call it, since AudioPortCallback is not supported.
  // Current volumes might be changed by updating active devices in android
  // AudioPolicyManager.
  MaybeUpdateVolumeSettingToDatabase();
#endif
  return status ? NS_ERROR_FAILURE : NS_OK;
#else
  NS_NOTREACHED("Doesn't support force routing on GB version");
  return NS_ERROR_UNEXPECTED;
#endif
}

NS_IMETHODIMP
AudioManager::GetForceForUse(int32_t aUsage, int32_t* aForce) {
#if ANDROID_VERSION >= 15
   *aForce = AudioSystem::getForceUse((audio_policy_force_use_t)aUsage);
   return NS_OK;
#else
  NS_NOTREACHED("Doesn't support force routing on GB version");
  return NS_ERROR_UNEXPECTED;
#endif
}

NS_IMETHODIMP
AudioManager::GetFmRadioAudioEnabled(bool *aFmRadioAudioEnabled)
{
  *aFmRadioAudioEnabled = IsFmOutConnected();
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetFmRadioAudioEnabled(bool aFmRadioAudioEnabled)
{
  UpdateDeviceConnectionState(aFmRadioAudioEnabled,
                              AUDIO_DEVICE_OUT_FM,
                              NS_LITERAL_CSTRING(""));
  // AUDIO_STREAM_FM is not used on recent gonk.
  // AUDIO_STREAM_MUSIC is used for FM radio volume control.
#if ANDROID_VERSION < 19
  // sync volume with music after powering on fm radio
  if (aFmRadioAudioEnabled) {
    uint32_t volIndex = mStreamStates[AUDIO_STREAM_MUSIC]->GetVolumeIndex();
    nsresult rv = mStreamStates[AUDIO_STREAM_FM]->
      SetVolumeIndex(volIndex, AUDIO_DEVICE_OUT_FM);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetAudioChannelVolume(uint32_t aChannel, uint32_t aIndex)
{
  if (aChannel >= NUMBER_OF_AUDIO_CHANNELS) {
    return NS_ERROR_INVALID_ARG;
  }

  return SetStreamVolumeIndex(sChannelStreamTbl[aChannel], aIndex);
}

NS_IMETHODIMP
AudioManager::GetAudioChannelVolume(uint32_t aChannel, uint32_t* aIndex)
{
  if (aChannel >= NUMBER_OF_AUDIO_CHANNELS) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!aIndex) {
    return NS_ERROR_NULL_POINTER;
  }

  return GetStreamVolumeIndex(sChannelStreamTbl[aChannel], aIndex);
}

NS_IMETHODIMP
AudioManager::GetMaxAudioChannelVolume(uint32_t aChannel, uint32_t* aMaxIndex)
{
  if (aChannel >= NUMBER_OF_AUDIO_CHANNELS) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!aMaxIndex) {
    return NS_ERROR_NULL_POINTER;
  }

  *aMaxIndex = mStreamStates[sChannelStreamTbl[aChannel]]->GetMaxIndex();
   return NS_OK;
}

nsresult
AudioManager::ValidateVolumeIndex(int32_t aStream, uint32_t aIndex) const
{
  if (aStream <= AUDIO_STREAM_DEFAULT || aStream >= AUDIO_STREAM_MAX) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t maxIndex = mStreamStates[aStream]->GetMaxIndex();
  if (aIndex > maxIndex) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
AudioManager::SetStreamVolumeForDevice(int32_t aStream,
                                       uint32_t aIndex,
                                       uint32_t aDevice)
{
  if (aStream <= AUDIO_STREAM_DEFAULT || aStream >= AUDIO_STREAM_MAX) {
    return NS_ERROR_INVALID_ARG;
  }

  int32_t streamAlias = sStreamVolumeAliasTbl[aStream];
  VolumeStreamState* streamState = mStreamStates[streamAlias].get();
  return streamState->SetVolumeIndexToAliasStreams(aIndex, aDevice);
}

nsresult
AudioManager::SetStreamVolumeIndex(int32_t aStream, uint32_t aIndex)
{
  if (aStream <= AUDIO_STREAM_DEFAULT || aStream >= AUDIO_STREAM_MAX) {
    return NS_ERROR_INVALID_ARG;
  }

  int32_t streamAlias = sStreamVolumeAliasTbl[aStream];

  nsresult rv;
  for (int32_t streamType = 0; streamType < AUDIO_STREAM_MAX; streamType++) {
    if (streamAlias == sStreamVolumeAliasTbl[streamType]) {
      rv = mStreamStates[streamType]->SetVolumeIndexToActiveDevices(aIndex);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  // AUDIO_STREAM_FM is not used on recent gonk.
  // AUDIO_STREAM_MUSIC is used for FM radio volume control.
#if ANDROID_VERSION < 19
  if (streamAlias == AUDIO_STREAM_MUSIC && IsFmOutConnected()) {
    rv = mStreamStates[AUDIO_STREAM_FM]->
      SetVolumeIndex(aIndex, AUDIO_DEVICE_OUT_FM);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
#endif

  MaybeUpdateVolumeSettingToDatabase();
  return NS_OK;
}

nsresult
AudioManager::GetStreamVolumeIndex(int32_t aStream, uint32_t *aIndex)
{
  if (!aIndex) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aStream <= AUDIO_STREAM_DEFAULT || aStream >= AUDIO_STREAM_MAX) {
    return NS_ERROR_INVALID_ARG;
  }

  *aIndex = mStreamStates[aStream]->GetVolumeIndex();
  return NS_OK;
}

nsAutoCString
AudioManager::AppendDeviceToVolumeSetting(const char* aName, uint32_t aDevice)
{
  nsAutoCString topic;
  topic.Assign(aName);
  topic.Append(".");
  uint32_t index = 0;
  DebugOnly<bool> exist = mAudioDeviceTableIdMaps.Get(aDevice, &index);
  MOZ_ASSERT(exist);
  topic.Append(kAudioDeviceInfos[index].tag);
  return topic;
}

void
AudioManager::InitVolumeFromDatabase()
{
  nsresult rv;
  nsCOMPtr<nsISettingsService> service = do_GetService(SETTINGS_SERVICE, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsISettingsServiceLock> lock;
  rv = service->CreateLock(nullptr, getter_AddRefs(lock));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  RefPtr<VolumeInitCallback> callback = new VolumeInitCallback();
  MOZ_ASSERT(callback);
  callback->GetPromise()->Then(AbstractThread::MainThread(), __func__, this,
                               &AudioManager::InitDeviceVolumeSucceeded,
                               &AudioManager::InitDeviceVolumeFailed);

  for (uint32_t idx = 0; idx < MOZ_ARRAY_LENGTH(gVolumeData); ++idx) {
    for (uint32_t idx2 = 0; idx2 < MOZ_ARRAY_LENGTH(kAudioDeviceInfos); ++idx2) {
      lock->Get(AppendDeviceToVolumeSetting(gVolumeData[idx].mChannelName,
                                            kAudioDeviceInfos[idx2].value).get(),
                callback);
    }
  }
}

void
AudioManager::InitDeviceVolumeSucceeded()
{
  mIsVolumeInited = true;
  MaybeUpdateVolumeSettingToDatabase(true);
}

void
AudioManager::InitDeviceVolumeFailed(const char* aError)
{
  // Default volume of AUDIO_DEVICE_OUT_DEFAULT is already set.
  mIsVolumeInited = true;
  MaybeUpdateVolumeSettingToDatabase(true);
  NS_WARNING(aError);
}

void
AudioManager::MaybeUpdateVolumeSettingToDatabase(bool aForce)
{
  if (!mIsVolumeInited) {
    return;
  }

  nsCOMPtr<nsISettingsServiceLock> lock = GetSettingServiceLock();
  if (NS_WARN_IF(!lock)) {
    return;
  }

  // Send events to update the Gaia volumes
  JS::Rooted<JS::Value> value(RootingCx());
  uint32_t volume = 0;
  for (uint32_t idx = 0; idx < MOZ_ARRAY_LENGTH(gVolumeData); ++idx) {
    int32_t streamType = gVolumeData[idx].mStreamType;
    VolumeStreamState* streamState = mStreamStates[streamType].get();
    if(!aForce && !streamState->IsDevicesChanged()) {
      continue;
    }
    // Get volume index of active device.
    volume = streamState->GetVolumeIndex();
    value.setInt32(volume);
    lock->Set(gVolumeData[idx].mChannelName, value, nullptr, nullptr);
  }

  // For reducing the code dependency, Gaia doesn't need to know the
  // device volume, it only need to care about different volume categories.
  // However, we need to send the setting volume to the permanent database,
  // so that we can store the volume setting even if the phone reboots.

  for (uint32_t idx = 0; idx < MOZ_ARRAY_LENGTH(gVolumeData); ++idx) {
    int32_t  streamType = gVolumeData[idx].mStreamType;
    VolumeStreamState* streamState = mStreamStates[streamType].get();

    if(!streamState->IsVolumeIndexesChanged()) {
        continue;
    }

    uint32_t remainingDevices = mAudioOutDevicesUpdated;
    for (uint32_t i = 0; remainingDevices != 0; i++) {
      uint32_t device = (1 << i);
      if ((device & remainingDevices) == 0) {
        continue;
      }
      remainingDevices &= ~device;
      if (!mAudioDeviceTableIdMaps.Get(device, nullptr)) {
        continue;
      }
      volume = streamState->GetVolumeIndex(device);
      value.setInt32(volume);
      lock->Set(AppendDeviceToVolumeSetting(gVolumeData[idx].mChannelName,
                                            device).get(),
                value, nullptr, nullptr);
    }
  }

  // Clear changed flags
  for (uint32_t idx = 0; idx < MOZ_ARRAY_LENGTH(gVolumeData); ++idx) {
    int32_t  streamType = gVolumeData[idx].mStreamType;
    mStreamStates[streamType]->ClearDevicesChanged();
    mStreamStates[streamType]->ClearVolumeIndexesChanged();
  }
  // Clear mAudioOutDevicesUpdated
  mAudioOutDevicesUpdated = 0;
}

void
AudioManager::UpdateCachedActiveDevicesForStreams()
{
  // This function updates cached active devices for streams.
  // It is used for optimization of GetDevicesForStream() since L.
  // AudioManager could know when active devices
  // are changed in AudioPolicyManager by onAudioPortListUpdate().
  // Except it, AudioManager normally do not need to ask AuidoPolicyManager
  // about current active devices of streams and could use cached values.
  // Before L, onAudioPortListUpdate() does not exist and GetDevicesForStream()
  // does not use the cache. Therefore this function do nothing.
#if ANDROID_VERSION >= 21
  for (int32_t streamType = 0; streamType < AUDIO_STREAM_MAX; streamType++) {
    // Update cached active devices of stream
    mStreamStates[streamType]->IsDevicesChanged(false /* aFromCache */);
  }
#endif
}

uint32_t
AudioManager::GetDevicesForStream(int32_t aStream, bool aFromCache)
{
#if ANDROID_VERSION >= 21
  // Since Lollipop, devices update could be notified by AudioPortCallback.
  // Cached values can be used if there is no update.
  if (aFromCache) {
    return mStreamStates[aStream]->GetLastDevices();
  }
#endif

#if ANDROID_VERSION >= 17
  audio_devices_t devices =
    AudioSystem::getDevicesForStream(static_cast<audio_stream_type_t>(aStream));

  return static_cast<uint32_t>(devices);
#else
  // Per audio out device volume is not supported.
  // Use AUDIO_DEVICE_OUT_SPEAKER just to store audio volume to DB.
  return AUDIO_DEVICE_OUT_SPEAKER;
#endif
}

uint32_t
AudioManager::GetDeviceForStream(int32_t aStream)
{
  uint32_t devices =
    GetDevicesForStream(static_cast<audio_stream_type_t>(aStream));
  uint32_t device = SelectDeviceFromDevices(devices);
  return device;
}

/* static */ uint32_t
AudioManager::SelectDeviceFromDevices(uint32_t aOutDevices)
{
  uint32_t device = aOutDevices;

  // See android AudioService.getDeviceForStream().
  // AudioPolicyManager expects it.
  // See also android AudioPolicyManager::getDeviceForVolume().
  if ((device & (device - 1)) != 0) {
    // Multiple device selection.
    if ((device & AUDIO_DEVICE_OUT_SPEAKER) != 0) {
      device = AUDIO_DEVICE_OUT_SPEAKER;
#if ANDROID_VERSION >= 21
    } else if ((device & AUDIO_DEVICE_OUT_HDMI_ARC) != 0) {
      device = AUDIO_DEVICE_OUT_HDMI_ARC;
    } else if ((device & AUDIO_DEVICE_OUT_SPDIF) != 0) {
      device = AUDIO_DEVICE_OUT_SPDIF;
    } else if ((device & AUDIO_DEVICE_OUT_AUX_LINE) != 0) {
       device = AUDIO_DEVICE_OUT_AUX_LINE;
#endif
    } else {
       device &= AUDIO_DEVICE_OUT_ALL_A2DP;
    }
  }
  MOZ_ASSERT(audio_is_output_device(device));
  return device;
}
AudioManager::VolumeStreamState::VolumeStreamState(AudioManager& aManager,
                                                   int32_t aStreamType)
  : mManager(aManager)
  , mStreamType(aStreamType)
  , mLastDevices(0)
  , mIsDevicesChanged(true)
  , mIsVolumeIndexesChanged(true)
{
  InitStreamVolume();
}

bool
AudioManager::VolumeStreamState::IsDevicesChanged(bool aFromCache)
{
  uint32_t devices = mManager.GetDevicesForStream(mStreamType, aFromCache);
  if (devices != mLastDevices) {
    mLastDevices = devices;
    mIsDevicesChanged = true;
  }
  return mIsDevicesChanged;
}

void
AudioManager::VolumeStreamState::ClearDevicesChanged()
{
  mIsDevicesChanged = false;
}

bool
AudioManager::VolumeStreamState::IsVolumeIndexesChanged()
{
  return mIsVolumeIndexesChanged;
}

void
AudioManager::VolumeStreamState::ClearVolumeIndexesChanged()
{
  mIsVolumeIndexesChanged = false;
}

void
AudioManager::VolumeStreamState::InitStreamVolume()
{
  AudioSystem::initStreamVolume(static_cast<audio_stream_type_t>(mStreamType),
                                0,
                                GetMaxIndex());
}

uint32_t
AudioManager::VolumeStreamState::GetMaxIndex()
{
  return sMaxStreamVolumeTbl[mStreamType];
}

uint32_t
AudioManager::VolumeStreamState::GetDefaultIndex()
{
  return sDefaultStreamVolumeTbl[mStreamType];
}

uint32_t
AudioManager::VolumeStreamState::GetVolumeIndex()
{
  uint32_t device = mManager.GetDeviceForStream(mStreamType);
  return GetVolumeIndex(device);
}

uint32_t
AudioManager::VolumeStreamState::GetVolumeIndex(uint32_t aDevice)
{
  uint32_t index = 0;
  bool ret = mVolumeIndexes.Get(aDevice, &index);
  if (!ret) {
    index = mVolumeIndexes.Get(AUDIO_DEVICE_OUT_DEFAULT);
  }
  return index;
}

nsresult
AudioManager::VolumeStreamState::SetVolumeIndexToActiveDevices(uint32_t aIndex)
{
  uint32_t device = mManager.GetDeviceForStream(mStreamType);

  // Update volume index for device
  uint32_t oldVolumeIndex = 0;
  bool exist = mVolumeIndexes.Get(device, &oldVolumeIndex);
  if (exist && aIndex == oldVolumeIndex) {
    // No update
    return NS_OK;
  }

  // AudioPolicyManager::setStreamVolumeIndex() set volumes of all active
  // devices for stream.
  nsresult rv;
  rv = SetVolumeIndexToConsistentDeviceIfNeeded(aIndex, device);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
AudioManager::VolumeStreamState::SetVolumeIndexToAliasStreams(uint32_t aIndex,
                                                              uint32_t aDevice)
{
  uint32_t oldVolumeIndex = 0;
  bool exist = mVolumeIndexes.Get(aDevice, &oldVolumeIndex);
  if (exist && aIndex == oldVolumeIndex) {
    // No update
    return NS_OK;
  }

  nsresult rv = SetVolumeIndexToConsistentDeviceIfNeeded(aIndex, aDevice);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (int32_t streamType = 0; streamType < AUDIO_STREAM_MAX; streamType++) {
    if ((streamType != mStreamType) &&
         sStreamVolumeAliasTbl[streamType] == mStreamType) {
      // Rescaling of index is not necessary.
      rv = mManager.mStreamStates[streamType]->
        SetVolumeIndexToAliasStreams(aIndex, aDevice);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  return NS_OK;
}

nsresult
AudioManager::VolumeStreamState::SetVolumeIndexToConsistentDeviceIfNeeded(uint32_t aIndex, uint32_t aDevice)
{
  nsresult rv;
  if (aDevice == AUDIO_DEVICE_OUT_SPEAKER || aDevice == AUDIO_DEVICE_OUT_EARPIECE) {
    // Set AUDIO_DEVICE_OUT_SPEAKER and AUDIO_DEVICE_OUT_EARPIECE to same volume.
    rv = SetVolumeIndex(aIndex, AUDIO_DEVICE_OUT_SPEAKER);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    rv = SetVolumeIndex(aIndex, AUDIO_DEVICE_OUT_EARPIECE);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    // No alias device
    rv = SetVolumeIndex(aIndex, aDevice);
  }
  return rv;
}

nsresult
AudioManager::VolumeStreamState::SetVolumeIndex(uint32_t aIndex,
                                                uint32_t aDevice,
                                                bool aUpdateCache)
{
  status_t rv;
#if ANDROID_VERSION >= 17
  if (aUpdateCache) {
    mVolumeIndexes.Put(aDevice, aIndex);
    mIsVolumeIndexesChanged = true;
    mManager.AudioOutDeviceUpdated(aDevice);
  }

  rv = AudioSystem::setStreamVolumeIndex(
         static_cast<audio_stream_type_t>(mStreamType),
         aIndex,
         aDevice);
  return rv ? NS_ERROR_FAILURE : NS_OK;
#else
  if (aUpdateCache) {
    // Per audio out device volume is not supported.
    // Use AUDIO_DEVICE_OUT_SPEAKER just to store audio volume to DB.
    mVolumeIndexes.Put(AUDIO_DEVICE_OUT_SPEAKER, aIndex);
    mIsVolumeIndexesChanged = true;
    mManager.AudioOutDeviceUpdated(AUDIO_DEVICE_OUT_SPEAKER);
  }
  rv = AudioSystem::setStreamVolumeIndex(
         static_cast<audio_stream_type_t>(mStreamType),
         aIndex);
  return rv ? NS_ERROR_FAILURE : NS_OK;
#endif
}

void
AudioManager::VolumeStreamState::RestoreVolumeIndexToAllDevices()
{
  for (auto iter = mVolumeIndexes.Iter(); !iter.Done(); iter.Next()) {
    const uint32_t& key = iter.Key();
    uint32_t& index = iter.Data();
    SetVolumeIndex(key, index, /* aUpdateCache */ false);
  }
}

} /* namespace gonk */
} /* namespace dom */
} /* namespace mozilla */
