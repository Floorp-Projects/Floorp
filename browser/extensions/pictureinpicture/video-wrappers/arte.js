/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document.querySelector(".avp-captions");

    if (container) {
      updateCaptionsFunction("");
      const callback = function (mutationsList, observer) {
        let textNodeList = container.querySelectorAll(".avp-captions-line");
        if (!textNodeList.length) {
          updateCaptionsFunction("");
          return;
        }
        updateCaptionsFunction(
          Array.from(textNodeList, x => x.textContent).join("\n")
        );
      };
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
