/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_PREVIEW_TEXT,
  UPDATE_SHOW_ALL_FONTS,
} = require("./index");

module.exports = {

  /**
   * Update the preview text in the font inspector
   */
  updatePreviewText(previewText) {
    return {
      type: UPDATE_PREVIEW_TEXT,
      previewText,
    };
  },

  /**
   * Update whether to show all fonts in the font inspector
   */
  updateShowAllFonts(showAllFonts) {
    return {
      type: UPDATE_SHOW_ALL_FONTS,
      showAllFonts,
    };
  },

};
