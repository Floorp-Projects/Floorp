/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  isLive(video) {
    return !!document.querySelector(".ytp-live");
  }
  setMuted(video, shouldMute) {
    let muteButton = document.querySelector("#player .ytp-mute-button");

    if (video.muted !== shouldMute && muteButton) {
      muteButton.click();
    } else {
      video.muted = shouldMute;
    }
  }
  getDuration(video) {
    if (this.isLive(video)) {
      return Infinity;
    }
    return video.duration;
  }
  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document.getElementById("ytp-caption-window-container");

    if (container) {
      updateCaptionsFunction("");
      const callback = function (mutationsList, observer) {
        // eslint-disable-next-line no-unused-vars
        for (const mutation of mutationsList) {
          let textNodeList = container
            .querySelector(".captions-text")
            ?.querySelectorAll(".caption-visual-line");
          if (!textNodeList) {
            updateCaptionsFunction("");
            return;
          }

          updateCaptionsFunction(
            Array.from(textNodeList, x => x.textContent).join("\n")
          );
        }
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
  shouldHideToggle(video) {
    return !!video.closest(".ytd-video-preview");
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
