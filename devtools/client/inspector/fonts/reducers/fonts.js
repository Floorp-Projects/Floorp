/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_FONTS,
} = require("resource://devtools/client/inspector/fonts/actions/index.js");

const INITIAL_FONT_DATA = {
  // All fonts on the current page.
  allFonts: [],
};

const reducers = {
  [UPDATE_FONTS](_, { allFonts }) {
    return { allFonts };
  },
};

module.exports = function (fontData = INITIAL_FONT_DATA, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return fontData;
  }
  return reducer(fontData, action);
};
