/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_PLAYBACKCONTROLLER_H_
#define DOM_MEDIA_MEDIACONTROL_PLAYBACKCONTROLLER_H_

#include "MediaControlKeysEvent.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BrowsingContext.h"

namespace mozilla {
namespace dom {

class MediaSession;

/**
 * This interface is used to handle different playback control actions. The
 * methods are designed based on the MediaSessionAction values, which are
 * defined in the media session spec [1].
 *
 * If the active media session has a corresponding MediaSessionActionHandler,
 * then we would invoke it, or we would do nothing. However, for certain
 * actions, such as `play`, `pause` and `stop`, we have default action handling
 * in order to control playback correctly even if the website doesn't use media
 * session at all.
 *
 * The default aciton handling we perform is to change the outer window's media
 * suspended state, in order to control playback. Therefore, this class should
 * always be used in the same process where the outer window is. And we use
 * browsing context to get the outer window. (However, because of bug 1593826,
 * now we are not able to make sure that it actually has a window.)
 *
 * [1] https://w3c.github.io/mediasession/#enumdef-mediasessionaction
 */
class MOZ_STACK_CLASS PlaybackController {
 public:
  explicit PlaybackController(BrowsingContext* aContext);
  ~PlaybackController() = default;

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
  MediaSession* GetMediaSession();
  RefPtr<BrowsingContext> mBC;
};

class MediaActionHandler {
 public:
  static void HandleMediaControlKeysEvent(BrowsingContext* aContext,
                                          MediaControlKeysEvent aEvent);
};

}  // namespace dom
}  // namespace mozilla

#endif
