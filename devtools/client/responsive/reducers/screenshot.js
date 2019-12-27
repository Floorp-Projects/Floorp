/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TAKE_SCREENSHOT_END,
  TAKE_SCREENSHOT_START,
} = require("../actions/index");

const INITIAL_SCREENSHOT = {
  isCapturing: false,
};

const reducers = {
  [TAKE_SCREENSHOT_END](screenshot, action) {
    return {
      ...screenshot,
      isCapturing: false,
    };
  },

  [TAKE_SCREENSHOT_START](screenshot, action) {
    return {
      ...screenshot,
      isCapturing: true,
    };
  },
};

module.exports = function(screenshot = INITIAL_SCREENSHOT, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return screenshot;
  }
  return reducer(screenshot, action);
};
