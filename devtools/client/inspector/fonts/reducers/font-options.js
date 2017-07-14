/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_PREVIEW_TEXT,
  UPDATE_SHOW_ALL_FONTS
} = require("../actions/index");

const INITIAL_FONT_OPTIONS = {
  previewText: "Abc",
  showAllFonts: false,
};

let reducers = {

  [UPDATE_PREVIEW_TEXT](fontOptions, { previewText }) {
    return Object.assign({}, fontOptions, { previewText });
  },

  [UPDATE_SHOW_ALL_FONTS](fontOptions, { showAllFonts }) {
    return Object.assign({}, fontOptions, { showAllFonts });
  },

};

module.exports = function (fontOptions = INITIAL_FONT_OPTIONS, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return fontOptions;
  }
  return reducer(fontOptions, action);
};
