/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  play(video) {
    let playPauseButton = document.querySelector(
      `[data-purpose="play-button"]`
    );
    if (video.paused) {
      playPauseButton?.click();
    }
  }

  pause(video) {
    let playPauseButton = document.querySelector(
      `[data-purpose="pause-button"]`
    );
    if (!video.paused) {
      playPauseButton?.click();
    }
  }

  setMuted(video, shouldMute) {
    let muteButton = document.querySelector(
      `[data-purpose="volume-control-button"]`
    );
    if (video.muted !== shouldMute && muteButton) {
      muteButton.click();
    }
  }

  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = video.parentElement;

    if (container) {
      updateCaptionsFunction("");
      const callback = function (mutationsList, observer) {
        let text = container.querySelector(
          `[data-purpose="captions-cue-text"]`
        )?.innerText;
        if (!text) {
          updateCaptionsFunction("");
          return;
        }

        updateCaptionsFunction(text);
      };

      // immediately invoke the callback function to add subtitles to the PiP window
      callback([1], null);

      let captionsObserver = new MutationObserver(callback);

      captionsObserver.observe(container, {
        attributes: true,
        childList: true,
        subtree: true,
      });
    }
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
