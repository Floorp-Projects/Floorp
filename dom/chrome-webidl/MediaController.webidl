/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * This enum lists all supported behaviors on the media controller.
 */
enum MediaControlKey {
  "focus",
  "play",
  "pause",
  "playpause",
  "previoustrack",
  "nexttrack",
  "seekbackward",
  "seekforward",
  "skipad",
  "seekto",
  "stop",
};

/**
 * MediaController is used to control media playback for a tab, and each tab
 * would only have one media controller, which can be accessed from the
 * canonical browsing context.
 */
[Exposed=Window, ChromeOnly]
interface MediaController : EventTarget {
  readonly attribute unsigned long long id;
  readonly attribute boolean isActive;
  readonly attribute boolean isAudible;
  readonly attribute boolean isPlaying;

  [Frozen, Cached, Pure]
  readonly attribute sequence<MediaControlKey> supportedKeys;

  attribute EventHandler onactivated;
  attribute EventHandler ondeactivated;
  attribute EventHandler onpositionstatechange;
  attribute EventHandler onsupportedkeyschange;

  void focus();
  void play();
  void pause();
  void stop();
  void prevTrack();
  void nextTrack();
  void seekBackward();
  void seekForward();
  void skipAd();
  void seekTo(double seekTime, optional boolean fastSeek = false);
};

[ChromeOnly,Exposed=Window,HeaderFile="mozilla/dom/MediaControlService.h"]
namespace MediaControlService {
  // This is used to generate fake media control keys event in testing.
  void generateMediaControlKey(MediaControlKey aKey);

  // This is used to get the media metadata from the current main controller in
  // testing.
  MediaMetadataInit getCurrentActiveMediaMetadata();

  // This is used to get the actual media playback state from the current main
  // controller in testing.
  MediaSessionPlaybackState getCurrentMediaSessionPlaybackState();
};
