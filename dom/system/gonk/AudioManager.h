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

#include "mozilla/Observer.h"
#include "nsAutoPtr.h"
#include "nsIAudioManager.h"
#include "nsIObserver.h"
#include "AudioChannelAgent.h"
#include "android_audio/AudioSystem.h"

// {b2b51423-502d-4d77-89b3-7786b562b084}
#define NS_AUDIOMANAGER_CID {0x94f6fd70, 0x7615, 0x4af9, \
      {0x89, 0x10, 0xf9, 0x3c, 0x55, 0xe6, 0x62, 0xec}}
#define NS_AUDIOMANAGER_CONTRACTID "@mozilla.org/telephony/audiomanager;1"

namespace mozilla {
namespace hal {
class SwitchEvent;
typedef Observer<SwitchEvent> SwitchObserver;
} // namespace hal

namespace dom {
namespace gonk {
class RecoverTask;
class AudioChannelVolInitCallback;
class AudioManager : public nsIAudioManager
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
  friend class AudioChannelVolInitCallback;

protected:
  int32_t mPhoneState;
  int mCurrentStreamVolumeTbl[AUDIO_STREAM_CNT];

  nsresult SetStreamVolumeIndex(int32_t aStream, int32_t aIndex);
  nsresult GetStreamVolumeIndex(int32_t aStream, int32_t *aIndex);

private:
  nsAutoPtr<mozilla::hal::SwitchObserver> mObserver;
  nsCOMPtr<nsIAudioChannelAgent>          mPhoneAudioAgent;
#ifdef MOZ_B2G_RIL
  bool                                    mMuteCallToRIL;
  // mIsMicMuted is only used for toggling mute call to RIL.
  bool                                    mIsMicMuted;
#endif

  void HandleBluetoothStatusChanged(nsISupports* aSubject,
                                    const char* aTopic,
                                    const nsCString aAddress);
  void HandleAudioChannelProcessChanged();

  AudioManager();
  ~AudioManager();
};

} /* namespace gonk */
} /* namespace dom */
} /* namespace mozilla */

#endif // mozilla_dom_system_b2g_audiomanager_h__
