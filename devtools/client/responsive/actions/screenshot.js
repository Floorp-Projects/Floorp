/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const {
  TAKE_SCREENSHOT_START,
  TAKE_SCREENSHOT_END,
} = require("resource://devtools/client/responsive/actions/index.js");

const message = require("resource://devtools/client/responsive/utils/message.js");

const animationFrame = () =>
  new Promise(resolve => {
    window.requestAnimationFrame(resolve);
  });

module.exports = {
  takeScreenshot() {
    return async function ({ dispatch }) {
      await dispatch({ type: TAKE_SCREENSHOT_START });

      // Waiting the next repaint, to ensure the react components
      // can be properly render after the action dispatched above
      await animationFrame();

      window.postMessage({ type: "screenshot" }, "*");
      await message.wait(window, "screenshot-captured");

      dispatch({ type: TAKE_SCREENSHOT_END });
    };
  },
};
