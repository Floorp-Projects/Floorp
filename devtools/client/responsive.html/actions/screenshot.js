/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const {
  TAKE_SCREENSHOT_START,
  TAKE_SCREENSHOT_END,
} = require("./index");

const { getFormatStr } = require("../utils/l10n");
const { getToplevelWindow } = require("sdk/window/utils");
const { Task: { spawn } } = require("devtools/shared/task");
const e10s = require("../utils/e10s");

const BASE_URL = "resource://devtools/client/responsive.html";
const audioCamera = new window.Audio(`${BASE_URL}/audio/camera-click.mp3`);

const animationFrame = () => new Promise(resolve => {
  window.requestAnimationFrame(resolve);
});

function getFileName() {
  let date = new Date();
  let month = ("0" + (date.getMonth() + 1)).substr(-2);
  let day = ("0" + date.getDate()).substr(-2);
  let dateString = [date.getFullYear(), month, day].join("-");
  let timeString = date.toTimeString().replace(/:/g, ".").split(" ")[0];

  return getFormatStr("responsive.screenshotGeneratedFilename", dateString,
                      timeString);
}

function createScreenshotFor(node) {
  let mm = node.frameLoader.messageManager;

  return e10s.request(mm, "RequestScreenshot");
}

function saveToFile(data, filename) {
  return spawn(function* () {
    const chromeWindow = getToplevelWindow(window);
    const chromeDocument = chromeWindow.document;

    // append .png extension to filename if it doesn't exist
    filename = filename.replace(/\.png$|$/i, ".png");

    chromeWindow.saveURL(data, filename, null,
                         true, true,
                         chromeDocument.documentURIObject, chromeDocument);
  });
}

function simulateCameraEffects(node) {
  audioCamera.play();
  node.animate({ opacity: [ 0, 1 ] }, 500);
}

module.exports = {

  takeScreenshot() {
    return function* (dispatch, getState) {
      yield dispatch({ type: TAKE_SCREENSHOT_START });

      // Waiting the next repaint, to ensure the react components
      // can be properly render after the action dispatched above
      yield animationFrame();

      let iframe = document.querySelector("iframe");
      let data = yield createScreenshotFor(iframe);

      simulateCameraEffects(iframe);

      yield saveToFile(data, getFileName());

      dispatch({ type: TAKE_SCREENSHOT_END });
    };
  }
};
