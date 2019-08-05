/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_audiofocusmanager_h__
#define mozilla_dom_audiofocusmanager_h__

#include "base/basictypes.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

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
  void RequestAudioFocus(uint64_t aId);
  void RevokeAudioFocus(uint64_t aId);

  explicit AudioFocusManager(MediaControlService* aService);
  ~AudioFocusManager() = default;

  uint32_t GetAudioFocusNums() const;

 private:
  friend class MediaControlService;
  void Shutdown();
  void HandleAudioCompetition(uint64_t aId);

  RefPtr<MediaControlService> mService;
  nsTArray<uint64_t> mOwningFocusControllers;
};

}  // namespace dom
}  // namespace mozilla

#endif  //  mozilla_dom_audiofocusmanager_h__
