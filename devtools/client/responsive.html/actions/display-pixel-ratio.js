/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { CHANGE_DISPLAY_PIXEL_RATIO } = require("./index");

module.exports = {

  /**
   * The pixel ratio of the display has changed. This may be triggered by the user
   * when changing the monitor resolution, or when the window is dragged to a different
   * display with a different pixel ratio.
   */
  changeDisplayPixelRatio(displayPixelRatio) {
    return {
      type: CHANGE_DISPLAY_PIXEL_RATIO,
      displayPixelRatio,
    };
  },

};
