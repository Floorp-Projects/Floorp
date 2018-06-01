/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { CHANGE_DISPLAY_PIXEL_RATIO } = require("../actions/index");
const INITIAL_DISPLAY_PIXEL_RATIO = 0;

const reducers = {

  [CHANGE_DISPLAY_PIXEL_RATIO](_, action) {
    return action.displayPixelRatio;
  },

};

module.exports = function(displayPixelRatio = INITIAL_DISPLAY_PIXEL_RATIO, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return displayPixelRatio;
  }
  return reducer(displayPixelRatio, action);
};
