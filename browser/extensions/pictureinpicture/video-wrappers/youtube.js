/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  setMuted(video, isMuted) {
    let muteButton = document.querySelector("#player .ytp-mute-button");
    if (muteButton) {
      muteButton.click();
    } else {
      video.muted = isMuted;
    }
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
