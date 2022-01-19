/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function toggleMute() {
  let video = document.getElementById("mock-video-controls");
  let muteButton = document.querySelector(".mute-button");
  let isMuted = video.muted;
  video.muted = !isMuted;

  if (video.muted) {
    muteButton.setAttribute("isMuted", true);
  } else {
    muteButton.removeAttribute("isMuted");
  }
}

function playPause() {
  let video = document.getElementById("mock-video-controls");
  let playPauseButton = document.querySelector(".play-pause-button");

  if (video.paused) {
    video.play();
    playPauseButton.removeAttribute("isPaused");
  } else {
    video.setAttribute("isPaused", true);
    video.pause();
  }
}
