/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals browser, communication, getZoomFactor, shot, main, auth, catcher, analytics, blobConverters, thumbnailGenerator */

"use strict";

this.takeshot = (function() {
  const exports = {};
  const MAX_CANVAS_DIMENSION = 32767;

  communication.register(
    "screenshotPage",
    (sender, selectedPos, screenshotType, devicePixelRatio) => {
      return screenshotPage(selectedPos, screenshotType, devicePixelRatio);
    }
  );

  communication.register("getZoomFactor", sender => {
    return getZoomFactor();
  });

  function screenshotPage(pos, screenshotType, devicePixelRatio) {
    let zoomFactor = getZoomFactor();
    pos.width = Math.min(pos.right - pos.left, MAX_CANVAS_DIMENSION);
    pos.height = Math.min(pos.bottom - pos.top, MAX_CANVAS_DIMENSION);

    // If we are printing the full page or a truncated full page,
    // we must pass in this rectangle to preview the entire image
    let options = { format: "png" };
    if (
      screenshotType === "fullPage" ||
      screenshotType === "fullPageTruncated"
    ) {
      let rectangle = {
        x: 0,
        y: 0,
        width: pos.width,
        height: pos.height,
      };
      options.rect = rectangle;
      options.resetScrollPosition = true;

      // To avoid creating extremely large images (which causes
      // performance problems), we set the devicePixelRatio to 1.
      devicePixelRatio = 1;
      options.scale = 1 / zoomFactor;
    } else if (screenshotType != "visible") {
      let rectangle = {
        x: pos.left,
        y: pos.top,
        width: pos.width,
        height: pos.height,
      };
      options.rect = rectangle;
    }

    return catcher.watchPromise(
      browser.tabs.captureTab(null, options).then(dataUrl => {
        const image = new Image();
        image.src = dataUrl;
        return new Promise((resolve, reject) => {
          image.onload = catcher.watchFunction(() => {
            const xScale = devicePixelRatio;
            const yScale = devicePixelRatio;
            const canvas = document.createElement("canvas");
            canvas.height = pos.height * yScale;
            canvas.width = pos.width * xScale;
            const context = canvas.getContext("2d");
            context.drawImage(
              image,
              0,
              0,
              pos.width * xScale,
              pos.height * yScale,
              0,
              0,
              pos.width * xScale,
              pos.height * yScale
            );
            const result = canvas.toDataURL();
            resolve(result);
          });
        });
      })
    );
  }

  return exports;
})();
