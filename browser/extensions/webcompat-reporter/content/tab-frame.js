/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const TABDATA_MESSAGE = "WebCompat:SendTabData";

let getScreenshot = function(win) {
  return new Promise(resolve => {
    let url = win.location.href;
    try {
      let dpr = win.devicePixelRatio;
      let canvas = win.document.createElement("canvas");
      let ctx = canvas.getContext("2d");
      let x = win.document.documentElement.scrollLeft;
      let y = win.document.documentElement.scrollTop;
      let w = win.innerWidth;
      let h = win.innerHeight;
      canvas.width = dpr * w;
      canvas.height = dpr * h;
      ctx.scale(dpr, dpr);
      ctx.drawWindow(win, x, y, w, h, "#fff");
      canvas.toBlob(blob => {
        resolve({
          blob,
          hasMixedActiveContentBlocked: docShell.hasMixedActiveContentBlocked,
          hasMixedDisplayContentBlocked: docShell.hasMixedDisplayContentBlocked,
          url,
          hasTrackingContentBlocked: docShell.hasTrackingContentBlocked
        });
      });
    } catch (ex) {
      // CanvasRenderingContext2D.drawWindow can fail depending on memory or
      // surface size. Rather than reject, resolve the URL so the user can
      // file an issue without a screenshot.
      Cu.reportError(`WebCompatReporter: getting a screenshot failed: ${ex}`);
      resolve({url});
    }
  });
};

getScreenshot(content).then(data => sendAsyncMessage(TABDATA_MESSAGE, data));
