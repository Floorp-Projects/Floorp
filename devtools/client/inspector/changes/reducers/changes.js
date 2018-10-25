/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  RESET_CHANGES,
  TRACK_CHANGE,
} = require("../actions/index");

const INITIAL_STATE = {};

const reducers = {

  [TRACK_CHANGE](state, { data }) {
    return state;
  },

  [RESET_CHANGES](state) {
    return INITIAL_STATE;
  },

};

module.exports = function(state = INITIAL_STATE, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return state;
  }
  return reducer(state, action);
};
