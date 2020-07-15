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
  [Frozen, Cached, Pure]
  readonly attribute sequence<MediaControlKey> supportedKeys;

  attribute EventHandler onpositionstatechange;
  attribute EventHandler onsupportedkeyschange;

  // TODO : expose other media controller methods to webidl in order to support
  // the plan of controlling media directly from the chrome JS.
  // eg. play(), pause().
  void seekTo(double seekTime, optional boolean fastSeek = false);
};
