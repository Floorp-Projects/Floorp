/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_FONTS,
} = require("../actions/index");

const INITIAL_FONTS = [];

let reducers = {

  [UPDATE_FONTS](_, { fonts }) {
    return fonts;
  },

};

module.exports = function (fonts = INITIAL_FONTS, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return fonts;
  }
  return reducer(fonts, action);
};
