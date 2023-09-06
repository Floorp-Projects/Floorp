/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/mediasession/#idl-index
 */

enum MediaSessionPlaybackState {
  "none",
  "paused",
  "playing"
};

enum MediaSessionAction {
  "play",
  "pause",
  "seekbackward",
  "seekforward",
  "previoustrack",
  "nexttrack",
  "skipad",
  "seekto",
  "stop",
};

callback MediaSessionActionHandler = undefined(MediaSessionActionDetails details);

[Exposed=Window]
interface MediaSession {
  attribute MediaMetadata? metadata;

  attribute MediaSessionPlaybackState playbackState;

  undefined setActionHandler(MediaSessionAction action, MediaSessionActionHandler? handler);

  [Throws]
  undefined setPositionState(optional MediaPositionState state = {});

  // Fire the action handler. It's test-only for now.
  [ChromeOnly]
  undefined notifyHandler(MediaSessionActionDetails details);
};

[Exposed=Window]
interface MediaMetadata {
  [Throws]
  constructor(optional MediaMetadataInit init = {});

  attribute DOMString title;
  attribute DOMString artist;
  attribute DOMString album;
  // https://github.com/w3c/mediasession/issues/237
  // Take and return `MediaImage` on setter and getter.
  [Frozen, Cached, Pure, Throws]
  attribute sequence<object> artwork;
};

dictionary MediaMetadataInit {
  DOMString title = "";
  DOMString artist = "";
  DOMString album = "";
  sequence<MediaImage> artwork = [];
};

dictionary MediaImage {
  required USVString src;
  DOMString sizes = "";
  DOMString type = "";
};

// Spec issue https://github.com/w3c/mediasession/issues/254
dictionary MediaSessionActionDetails {
  required MediaSessionAction action;
  double seekOffset;
  double seekTime;
  boolean fastSeek;
};

dictionary MediaPositionState {
  double duration;
  double playbackRate;
  double position;
};
