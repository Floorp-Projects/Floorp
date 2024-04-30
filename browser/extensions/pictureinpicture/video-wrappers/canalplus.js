/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  isLive() {
    let documentURI = document.documentURI;
    return documentURI.includes("/live/") || documentURI.includes("/TV/");
  }

  getDuration(video) {
    if (this.isLive(video)) {
      return Infinity;
    }
    return video.duration;
  }

  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container =
      document.querySelector(`[data-testid="playerRoot"]`) ||
      document.querySelector(`[player-root="true"]`);

    if (container) {
      updateCaptionsFunction("");
      const callback = function (mutationsList) {
        // eslint-disable-next-line no-unused-vars
        for (const mutation of mutationsList) {
          let text = container.querySelector(
            ".rxp-texttrack-region"
          )?.innerText;
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
        attributes: false,
        childList: true,
        subtree: true,
      });
    }
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
