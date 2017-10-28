/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { UPDATE_ANIMATIONS } = require("../actions/index");

const INITIAL_ANIMATIONS = [];

const reducers = {
  [UPDATE_ANIMATIONS](_, { animations }) {
    return animations;
  }
};

module.exports = function (animations = INITIAL_ANIMATIONS, action) {
  const reducer = reducers[action.type];
  return reducer ? reducer(animations, action) : animations;
};
