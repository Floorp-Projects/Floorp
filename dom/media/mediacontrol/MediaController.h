/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLLER_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLLER_H_

#include "ContentMediaController.h"
#include "MediaEventSource.h"
#include "mozilla/dom/MediaSessionController.h"
#include "mozilla/LinkedList.h"
#include "nsDataHashtable.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

class BrowsingContext;
enum class MediaControlKeysEvent : uint32_t;

/**
 * MediaController is a class, which is used to control all media within a tab.
 * It can only be used in Chrome process and the controlled media are usually
 * in the content process (unless we disable e10s).
 *
 * Each tab would have only one media controller, they are 1-1 corresponding
 * relationship, we use tab's top-level browsing context ID to initialize the
 * controller and use that as its ID.
 *
 * Whenever controlled media started, we would notify the controller to increase
 * or decrease the amount of its controlled media when its controlled media
 * started or stopped.
 *
 * Once the controller started, which means it has controlled some media, then
 * we can use its controlling methods, such as `Play()`, `Pause()` to control
 * the media within the tab. If there is at least one controlled media playing
 * in the tab, then we would say the controller is `playing`. If there is at
 * least one controlled media is playing and audible, then we would say the
 * controller is `audible`.
 *
 * Note that, if we don't enable audio competition, then we might have multiple
 * tabs playing media at the same time, we can use the ID to query the specific
 * controller from `MediaControlService`.
 */
class MediaController final
    : public MediaSessionController,
      public LinkedListElement<RefPtr<MediaController>> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaController, override);

  explicit MediaController(uint64_t aContextId);

  // Focus the window currently playing media.
  void Focus();
  void Play();
  void Pause();
  void Stop();
  void PrevTrack();
  void NextTrack();
  void SeekBackward();
  void SeekForward();

  // If any of controlled media is being used in Picture-In-Picture mode, then
  // this function should be callled and set the value.
  void SetIsInPictureInPictureMode(bool aIsInPictureInPictureMode);

  // Reture true if any of controlled media is being used in Picture-In-Picture
  // mode.
  bool IsInPictureInPictureMode() const;

  // Calling this method explicitly would mark this controller as deprecated,
  // then calling any its method won't take any effect.
  void Shutdown();

  bool IsAudible() const;
  uint64_t ControlledMediaNum() const;
  MediaSessionPlaybackState GetState() const;

  void SetDeclaredPlaybackState(uint64_t aSessionContextId,
                                MediaSessionPlaybackState aState) override;

  // These methods are only being used to notify the state changes of controlled
  // media in ContentParent or MediaControlUtils.
  void NotifyMediaStateChanged(ControlledMediaState aState);
  void NotifyMediaAudibleChanged(bool aAudible);

 private:
  ~MediaController();

  void UpdateMediaControlKeysEventToContentMediaIfNeeded(
      MediaControlKeysEvent aEvent);
  void IncreaseControlledMediaNum();
  void DecreaseControlledMediaNum();
  void IncreasePlayingControlledMediaNum();
  void DecreasePlayingControlledMediaNum();

  void Activate();
  void Deactivate();

  void SetGuessedPlayState(MediaSessionPlaybackState aState);

  // Whenever the declared playback state (from media session controller) or the
  // guessed playback state changes, we should recompute actual playback state
  // to know if we need to update the virtual control interface.
  void UpdateActualPlaybackState();

  bool mAudible = false;
  bool mIsRegisteredToService = false;
  int64_t mControlledMediaNum = 0;
  int64_t mPlayingControlledMediaNum = 0;
  bool mShutdown = false;
  bool mIsInPictureInPictureMode = false;

  // This state can match to the `guessed playback state` in the spec [1], it
  // indicates if we have any media element playing within the tab which this
  // controller belongs to. But currently we only take media elements into
  // account, which is different from the way the spec recommends. In addition,
  // We don't support web audio and plugin and not consider audible state of
  // media.
  // [1] https://w3c.github.io/mediasession/#guessed-playback-state
  MediaSessionPlaybackState mGuessedPlaybackState =
      MediaSessionPlaybackState::None;

  // This playback state would be the final playback which can be used to know
  // if the controller is playing or not.
  // https://w3c.github.io/mediasession/#actual-playback-state
  MediaSessionPlaybackState mActualPlaybackState =
      MediaSessionPlaybackState::None;
};

}  // namespace dom
}  // namespace mozilla

#endif
