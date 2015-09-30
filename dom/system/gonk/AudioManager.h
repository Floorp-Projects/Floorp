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

#ifndef mozilla_dom_system_b2g_audiomanager_h__
#define mozilla_dom_system_b2g_audiomanager_h__

#include "mozilla/HalTypes.h"
#include "mozilla/Observer.h"
#include "nsAutoPtr.h"
#include "nsIAudioManager.h"
#include "nsIObserver.h"
#include "android_audio/AudioSystem.h"

// {b2b51423-502d-4d77-89b3-7786b562b084}
#define NS_AUDIOMANAGER_CID {0x94f6fd70, 0x7615, 0x4af9, \
      {0x89, 0x10, 0xf9, 0x3c, 0x55, 0xe6, 0x62, 0xec}}
#define NS_AUDIOMANAGER_CONTRACTID "@mozilla.org/telephony/audiomanager;1"

class nsISettingsServiceLock;

namespace mozilla {
namespace hal {
class SwitchEvent;
typedef Observer<SwitchEvent> SwitchObserver;
} // namespace hal

namespace dom {
namespace gonk {

/**
 * FxOS can remeber the separate volume settings on difference output profiles.
 * (1) Primary : speaker, receiver
 * (2) Headset : wired headphone/headset
 * (3) Bluetooth : BT SCO/A2DP devices
 **/
enum AudioOutputProfiles {
  DEVICE_ERROR        = -1,
  DEVICE_PRIMARY      = 0,
  DEVICE_HEADSET      = 1,
  DEVICE_BLUETOOTH    = 2,
  DEVICE_TOTAL_NUMBER = 3,
};

/**
 * We have five sound volume settings from UX spec,
 * You can see more informations in Bug1068219.
 * (1) Media : music, video, FM ...
 * (2) Notification : ringer, notification ...
 * (3) Alarm : alarm
 * (4) Telephony : GSM call, WebRTC call
 * (5) Bluetooth SCO : SCO call
 **/
enum AudioVolumeCategories {
  VOLUME_MEDIA         = 0,
  VOLUME_NOTIFICATION  = 1,
  VOLUME_ALARM         = 2,
  VOLUME_TELEPHONY     = 3,
  VOLUME_BLUETOOTH_SCO = 4,
  VOLUME_TOTAL_NUMBER  = 5,
};

struct VolumeData {
  const char* mChannelName;
  uint32_t mCategory;
};

class RecoverTask;
class VolumeInitCallback;
class AudioProfileData;

class AudioManager final : public nsIAudioManager
                         , public nsIObserver
{
public:
  static already_AddRefed<AudioManager> GetInstance();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUDIOMANAGER
  NS_DECL_NSIOBSERVER

  // When audio backend is dead, recovery task needs to read all volume
  // settings then set back into audio backend.
  friend class RecoverTask;
  friend class VolumeInitCallback;

  // Open or close the specific profile
  void SwitchProfileData(AudioOutputProfiles aProfile, bool aActive);

  // Validate whether the volume index is within the range
  nsresult ValidateVolumeIndex(uint32_t aCategory, uint32_t aIndex) const;

  // Called when android AudioFlinger in mediaserver is died
  void HandleAudioFlingerDied();

  void HandleHeadphoneSwitchEvent(const hal::SwitchEvent& aEvent);

protected:
  int32_t mPhoneState;

  // A bitwise variable for recording what kind of headset/headphone is attached.
  int32_t mHeadsetState;

  bool mSwitchDone;

#if defined(MOZ_B2G_BT) || ANDROID_VERSION >= 17
  bool mBluetoothA2dpEnabled;
#endif
#ifdef MOZ_B2G_BT
  bool mA2dpSwitchDone;
#endif
  uint32_t mCurrentStreamVolumeTbl[AUDIO_STREAM_CNT];

  nsresult SetStreamVolumeIndex(int32_t aStream, uint32_t aIndex);
  nsresult GetStreamVolumeIndex(int32_t aStream, uint32_t *aIndex);

private:
  nsAutoPtr<mozilla::hal::SwitchObserver> mObserver;
#ifdef MOZ_B2G_RIL
  bool                                    mMuteCallToRIL;
  // mIsMicMuted is only used for toggling mute call to RIL.
  bool                                    mIsMicMuted;
#endif
  nsTArray<nsAutoPtr<AudioProfileData>>   mAudioProfiles;
  AudioOutputProfiles mPresentProfile;

  void HandleBluetoothStatusChanged(nsISupports* aSubject,
                                    const char* aTopic,
                                    const nsCString aAddress);
  void HandleAudioChannelProcessChanged();

  void CreateAudioProfilesData();

  // Init the volume setting from the init setting callback
  void InitProfileVolume(AudioOutputProfiles aProfile,
                        uint32_t aCatogory, uint32_t aIndex);

  // Update volume data of profiles
  void UpdateVolumeToProfile(AudioProfileData* aProfileData);

  // Apply the volume data to device
  void UpdateVolumeFromProfile(AudioProfileData* aProfileData);

  // Send the volume changing event to Gaia
  void SendVolumeChangeNotification(AudioProfileData* aProfileData);

  // Update the mPresentProfile and profiles active status
  void UpdateProfileState(AudioOutputProfiles aProfile, bool aActive);

  // Volume control functions
  nsresult SetVolumeByCategory(uint32_t aCategory, uint32_t aIndex);
  uint32_t GetVolumeByCategory(uint32_t aCategory) const;
  uint32_t GetMaxVolumeByCategory(uint32_t aCategory) const;

  AudioProfileData* FindAudioProfileData(AudioOutputProfiles aProfile);

  // Append the profile to the volume setting string.
  nsAutoCString AppendProfileToVolumeSetting(const char* aName,
                                             AudioOutputProfiles aProfile);

  // We store the volume setting in the database, these are related functions.
  void InitVolumeFromDatabase();
  void UpdateVolumeSettingToDatabase(nsISettingsServiceLock* aLock,
                                     const char* aTopic,
                                     uint32_t aVolIndex);

  // Promise functions.
  void InitProfileVolumeSucceeded();
  void InitProfileVolumeFailed(const char* aError);

  void UpdateHeadsetConnectionState(hal::SwitchState aState);

  AudioManager();
  ~AudioManager();
};

} /* namespace gonk */
} /* namespace dom */
} /* namespace mozilla */

#endif // mozilla_dom_system_b2g_audiomanager_h__
