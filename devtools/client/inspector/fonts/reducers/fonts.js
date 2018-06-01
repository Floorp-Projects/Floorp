/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_FONTS,
} = require("../actions/index");

const INITIAL_FONT_DATA = {
  fonts: [],
  otherFonts: []
};

const reducers = {

  [UPDATE_FONTS](_, { fonts, otherFonts }) {
    return { fonts, otherFonts };
  },

};

module.exports = function(fontData = INITIAL_FONT_DATA, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return fontData;
  }
  return reducer(fontData, action);
};
