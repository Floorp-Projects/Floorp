/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_TOUCH_SIMULATION_ENABLED,
} = require("../actions/index");

const INITIAL_TOUCH_SIMULATION = { enabled: false };

let reducers = {

  [UPDATE_TOUCH_SIMULATION_ENABLED](touchSimulation, { enabled }) {
    return Object.assign({}, touchSimulation, {
      enabled,
    });
  },

};

module.exports = function (touchSimulation = INITIAL_TOUCH_SIMULATION, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return touchSimulation;
  }
  return reducer(touchSimulation, action);
};
