/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CHANGE_RELOAD_CONDITION,
  LOAD_RELOAD_CONDITIONS_END,
} = require("../actions/index");

const Types = require("../types");

const INITIAL_RELOAD_CONDITIONS = {
  touchSimulation: false,
  userAgent: false,
  state: Types.loadableState.INITIALIZED,
};

const reducers = {

  [CHANGE_RELOAD_CONDITION](conditions, { id, value }) {
    return Object.assign({}, conditions, {
      [id]: value,
    });
  },

  [LOAD_RELOAD_CONDITIONS_END](conditions) {
    return Object.assign({}, conditions, {
      state: Types.loadableState.LOADED,
    });
  },

};

module.exports = function(conditions = INITIAL_RELOAD_CONDITIONS, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return conditions;
  }
  return reducer(conditions, action);
};
