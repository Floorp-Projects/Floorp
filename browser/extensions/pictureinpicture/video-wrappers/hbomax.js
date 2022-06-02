/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  constructor(video) {
    this.player = video.wrappedJSObject._dispNode._player;
  }
  play() {
    this.player.play();
  }
  pause() {
    this.player.pause();
  }
  setVolume(video, volume) {
    video.volume = volume;
  }
  isMuted(video) {
    return video.volume === 0;
  }
  setMuted(video, shouldMute) {
    if (shouldMute) {
      this.setVolume(video, 0);
    } else {
      this.setVolume(video, 1);
    }
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
