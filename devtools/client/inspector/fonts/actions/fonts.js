/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_FONTS,
} = require("resource://devtools/client/inspector/fonts/actions/index.js");

module.exports = {
  /**
   * Update the list of fonts in the font inspector
   */
  updateFonts(allFonts) {
    return {
      type: UPDATE_FONTS,
      allFonts,
    };
  },
};
