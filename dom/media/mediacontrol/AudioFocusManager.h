/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_AUDIOFOCUSMANAGER_H_
#define DOM_MEDIA_MEDIACONTROL_AUDIOFOCUSMANAGER_H_

#include "base/basictypes.h"
#include "nsTArray.h"
#include "VideoUtils.h"

namespace mozilla::dom {

class IMediaController;
class MediaControlService;

/**
 * AudioFocusManager is used to assign the audio focus to different requester
 * and decide which requester can own audio focus when audio competing happens.
 * When the audio competing happens, the last request would be a winner who can
 * still own the audio focus, and all the other requesters would lose the audio
 * focus. Now MediaController is the onlt requester, it would request the audio
 * focus when it becomes audible and revoke the audio focus when the controller
 * is no longer active.
 */
class AudioFocusManager {
 public:
  void RequestAudioFocus(IMediaController* aController);
  void RevokeAudioFocus(IMediaController* aController);

  explicit AudioFocusManager() = default;
  ~AudioFocusManager();

  uint32_t GetAudioFocusNums() const;

 private:
  friend class MediaControlService;
  // Return true if we manage audio focus by clearing other controllers owning
  // audio focus before assigning audio focus to the new controller.
  bool ClearFocusControllersIfNeeded();

  void CreateTimerForUpdatingTelemetry();
  // This would check if user has managed audio focus by themselves and update
  // the result to telemetry. This method should only be called from timer.
  void UpdateTelemetryDataFromTimer(uint32_t aPrevFocusNum,
                                    uint64_t aPrevActiveControllerNum);

  nsTArray<RefPtr<IMediaController>> mOwningFocusControllers;
  RefPtr<SimpleTimer> mTelemetryTimer;
};

}  // namespace mozilla::dom

#endif  //  DOM_MEDIA_MEDIACONTROL_AUDIOFOCUSMANAGER_H_
