/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_AUDIOFOCUSMANAGER_H_
#define DOM_MEDIA_MEDIACONTROL_AUDIOFOCUSMANAGER_H_

#include "base/basictypes.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class MediaController;
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
  void RequestAudioFocus(MediaController* aController);
  void RevokeAudioFocus(MediaController* aController);

  explicit AudioFocusManager() = default;
  ~AudioFocusManager() = default;

  uint32_t GetAudioFocusNums() const;

 private:
  friend class MediaControlService;
  void HandleAudioCompetition(MediaController* aController);

  nsTArray<RefPtr<MediaController>> mOwningFocusControllers;
};

}  // namespace dom
}  // namespace mozilla

#endif  //  DOM_MEDIA_MEDIACONTROL_AUDIOFOCUSMANAGER_H_
