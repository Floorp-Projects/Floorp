/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/mediasession/#idl-index
 */

// TODO: Implement MediaSessionPlaybackState (bug 1582508)

// TODO: Implement the missing seek* (bug 1580623) and skipad (bug 1582569) actions
enum MediaSessionAction {
  "play",
  "pause",
  "previoustrack",
  "nexttrack",
  "stop",
};

callback MediaSessionActionHandler = void(MediaSessionActionDetails details);

[Exposed=Window, Pref="dom.media.mediasession.enabled"]
interface MediaSession {
  attribute MediaMetadata? metadata;

  // TODO: attribute MediaSessionPlaybackState playbackState; (bug 1582508)

  void setActionHandler(MediaSessionAction action, MediaSessionActionHandler? handler);

  // TODO: void setPositionState(optional MediaPositionState? state); (bug 1582509)

  // Fire the action handler. It's test-only for now.
  [ChromeOnly]
  void notifyHandler(MediaSessionActionDetails details);
};

[Exposed=Window, Pref="dom.media.mediasession.enabled"]
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

dictionary MediaSessionActionDetails {
  required MediaSessionAction action;
};

// TODO: Implement MediaPositionState (bug 1582509)
