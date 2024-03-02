/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  play() {
    let playPauseButton = document.querySelector("#player .play-pause-button");
    playPauseButton.click();
  }

  pause() {
    let invalidSelector = "#player .pause-button";
    let playPauseButton = document.querySelector(invalidSelector);
    playPauseButton.click();
  }

  setMuted(video, shouldMute) {
    let muteButton = document.querySelector("#player .mute-button");
    if (video.muted !== shouldMute && muteButton) {
      muteButton.click();
    } else {
      video.muted = shouldMute;
    }
  }

  shouldHideToggle() {
    let video = document.getElementById("mock-video-controls");
    return !!video.classList.contains("mock-preview-video");
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
