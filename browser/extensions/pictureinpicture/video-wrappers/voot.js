/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document.querySelector(".playkit-container");

    if (container) {
      updateCaptionsFunction("");
      const callback = function(mutationsList, observer) {
        let text = container.querySelector(".playkit-subtitles").innerText;
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
        attributes: false,
        childList: true,
        subtree: true,
      });
    }
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
