/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const Services = require("Services");

const { TAKE_SCREENSHOT_START, TAKE_SCREENSHOT_END } = require("./index");

const { getFormatStr } = require("../utils/l10n");
const { getTopLevelWindow } = require("../utils/window");
const e10s = require("../utils/e10s");

const CAMERA_AUDIO_URL = "resource://devtools/client/themes/audio/shutter.wav";

const animationFrame = () =>
  new Promise(resolve => {
    window.requestAnimationFrame(resolve);
  });

function getFileName() {
  const date = new Date();
  const month = ("0" + (date.getMonth() + 1)).substr(-2);
  const day = ("0" + date.getDate()).substr(-2);
  const dateString = [date.getFullYear(), month, day].join("-");
  const timeString = date
    .toTimeString()
    .replace(/:/g, ".")
    .split(" ")[0];

  return getFormatStr(
    "responsive.screenshotGeneratedFilename",
    dateString,
    timeString
  );
}

function createScreenshotFor(node) {
  const mm = node.frameLoader.messageManager;

  return e10s.request(mm, "RequestScreenshot");
}

function saveToFile(data, filename) {
  const chromeWindow = getTopLevelWindow(window);
  const chromeDocument = chromeWindow.document;

  // append .png extension to filename if it doesn't exist
  filename = filename.replace(/\.png$|$/i, ".png");

  // In chrome doc, we don't need to pass a referrer, just pass null
  chromeWindow.saveURL(data, filename, null, true, true, null, chromeDocument);
}

function simulateCameraEffects(node) {
  if (Services.prefs.getBoolPref("devtools.screenshot.audio.enabled")) {
    const cameraAudio = new window.Audio(CAMERA_AUDIO_URL);
    cameraAudio.play();
  }
  node.animate({ opacity: [0, 1] }, 500);
}

module.exports = {
  takeScreenshot() {
    return async function(dispatch, getState) {
      await dispatch({ type: TAKE_SCREENSHOT_START });

      // Waiting the next repaint, to ensure the react components
      // can be properly render after the action dispatched above
      await animationFrame();

      const iframe = document.querySelector("iframe");
      const data = await createScreenshotFor(iframe);

      simulateCameraEffects(iframe);

      saveToFile(data, getFileName());

      dispatch({ type: TAKE_SCREENSHOT_END });
    };
  },
};
