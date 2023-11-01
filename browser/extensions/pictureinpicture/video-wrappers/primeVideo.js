/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  /**
   * Playing the video when the readyState is HAVE_METADATA (1) can cause play
   * to fail but it will load the video and trying to play again allows enough
   * time for the second play to successfully play the video.
   *
   * @param {HTMLVideoElement} video
   *  The original video element
   */
  play(video) {
    video.play().catch(() => {
      video.play();
    });
  }
  /**
   * Seeking large amounts of time can cause the video readyState to
   * HAVE_METADATA (1) and it will throw an error when trying to play the video.
   * To combat this, after seeking we check if the readyState changed and if so,
   * we will play to video to "load" the video at the new time and then play or
   * pause the video depending on if the video was playing before we seeked.
   *
   * @param {HTMLVideoElement} video
   *  The original video element
   * @param {number} position
   *  The new time to set the video to
   * @param {boolean} wasPlaying
   *  True if the video was playing before seeking else false
   */
  setCurrentTime(video, position, wasPlaying) {
    if (wasPlaying === undefined) {
      this.wasPlaying = !video.paused;
    }
    video.currentTime = position;
    if (video.readyState < video.HAVE_CURRENT_DATA) {
      video
        .play()
        .then(() => {
          if (!wasPlaying) {
            video.pause();
          }
        })
        .catch(() => {
          if (wasPlaying) {
            this.play(video);
          }
        });
    }
  }
  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document?.querySelector("#dv-web-player");

    if (container) {
      updateCaptionsFunction("");
      const callback = function (mutationsList, observer) {
        // eslint-disable-next-line no-unused-vars
        for (const mutation of mutationsList) {
          let text;
          // windows, mac
          if (container?.querySelector(".atvwebplayersdk-player-container")) {
            text = container
              ?.querySelector(".f35bt6a")
              ?.querySelector(".atvwebplayersdk-captions-text")?.innerText;
          } else {
            // linux
            text = container
              ?.querySelector(".persistentPanel")
              ?.querySelector("span")?.innerText;
          }

          if (!text) {
            updateCaptionsFunction("");
            return;
          }

          updateCaptionsFunction(text);
        }
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

  shouldHideToggle(video) {
    return !!video.classList.contains("tst-video-overlay-player-html5");
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
