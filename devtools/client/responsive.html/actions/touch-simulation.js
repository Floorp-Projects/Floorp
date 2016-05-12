/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const {
  UPDATE_TOUCH_SIMULATION_ENABLED
} = require("./index");

module.exports = {

  updateTouchSimulationEnabled(enabled) {
    return {
      type: UPDATE_TOUCH_SIMULATION_ENABLED,
      enabled,
    };
  },

};
