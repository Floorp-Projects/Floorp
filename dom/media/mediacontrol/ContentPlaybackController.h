/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_CONTENTPLAYBACKCONTROLLER_H_
#define DOM_MEDIA_MEDIACONTROL_CONTENTPLAYBACKCONTROLLER_H_

#include "MediaControlKeysEvent.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BrowsingContext.h"

namespace mozilla {
namespace dom {

class MediaSession;

/**
 * This interface is used to handle different playback control actions in the
 * content process. Most of the methods are designed based on the
 * MediaSessionAction values, which are defined in the media session spec [1].
 *
 * The reason we need that is to explicitly separate the implementation from all
 * different defined methods. If we want to add a new method in the future, we
 * can do that without modifying other methods.
 *
 * If the active media session has a corresponding MediaSessionActionHandler,
 * then we would invoke it, or we would do nothing. However, for certain
 * actions, such as `play`, `pause` and `stop`, we have default action handling
 * in order to control playback correctly even if the website doesn't use media
 * session at all or the media session doesn't have correspending action handler
 * [2].
 *
 * [1] https://w3c.github.io/mediasession/#enumdef-mediasessionaction
 * [2]
 * https://w3c.github.io/mediasession/#ref-for-active-media-session%E2%91%A1%E2%93%AA
 */
class MOZ_STACK_CLASS ContentPlaybackController {
 public:
  explicit ContentPlaybackController(BrowsingContext* aContext);
  ~ContentPlaybackController() = default;

  void Focus();
  void Play();
  void Pause();
  void SeekBackward();
  void SeekForward();
  void PreviousTrack();
  void NextTrack();
  void SkipAd();
  void Stop();
  void SeekTo();

 private:
  void NotifyContentControlKeyEventReceiver(MediaControlKeysEvent aEvent);
  void NotifyMediaSession(MediaSessionAction aAction);
  void NotifyMediaSessionWhenActionIsSupported(MediaSessionAction aAction);
  bool IsMediaSessionActionSupported(MediaSessionAction aAction) const;
  MediaSession* GetMediaSession() const;

  RefPtr<BrowsingContext> mBC;
};

class ContentMediaActionHandler {
 public:
  static void HandleMediaControlKeysEvent(BrowsingContext* aContext,
                                          MediaControlKeysEvent aEvent);
};

}  // namespace dom
}  // namespace mozilla

#endif
