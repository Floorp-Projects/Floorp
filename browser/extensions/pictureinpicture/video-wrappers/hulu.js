/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  constructor(video) {
    this.player = video.wrappedJSObject.__HuluDashPlayer__;
  }
  play() {
    this.player.play();
  }
  pause() {
    this.player.pause();
  }
  isMuted(video) {
    return video.volume === 0;
  }
  setMuted() {
    let muteButton = document.querySelector(".VolumeControl > div");
    muteButton.click();
  }
  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document.querySelector(".ClosedCaption");

    if (container) {
      updateCaptionsFunction("");
      const callback = function(mutationsList, observer) {
        let text = container.querySelector(".CaptionBox").innerText;
        updateCaptionsFunction(text);
      };

      // immediately invoke the callback function to add subtitles to the PiP window
      callback([1], null);

      let captionsObserver = new MutationObserver(callback);

      captionsObserver.observe(container, {
        attributes: false,
        childList: true,
        subtree: true,
      });
    }
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
