/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  play(video) {
    let playPauseButton = document.querySelector(
      ".rcplayer-btn.rcplayer-smallPlayPauseBtn"
    );
    if (video.paused) {
      playPauseButton.click();
    }
  }

  pause(video) {
    let playPauseButton = document.querySelector(
      ".rcplayer-btn.rcplayer-smallPlayPauseBtn"
    );
    if (!video.paused) {
      playPauseButton.click();
    }
  }

  setMuted(video, shouldMute) {
    let muteButton = document.querySelector(
      ".rcplayer-bouton-with-panel--volume > button"
    );
    if (video.muted !== shouldMute && muteButton) {
      muteButton.click();
    }
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
