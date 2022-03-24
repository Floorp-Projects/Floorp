/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document.querySelector("#dv-web-player");

    if (container) {
      updateCaptionsFunction("");
      const callback = function(mutationsList, observer) {
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
