/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  accessibility: { SIMULATION_TYPE },
} = require("devtools/shared/constants");
const { SIMULATE } = require("devtools/client/accessibility/constants");

/**
 * Initial state definition
 */
function getInitialState() {
  return {
    [SIMULATION_TYPE.PROTANOMALY]: false,
    [SIMULATION_TYPE.DEUTERANOMALY]: false,
    [SIMULATION_TYPE.TRITANOMALY]: false,
    [SIMULATION_TYPE.PROTANOPIA]: false,
    [SIMULATION_TYPE.DEUTERANOPIA]: false,
    [SIMULATION_TYPE.TRITANOPIA]: false,
    [SIMULATION_TYPE.CONTRAST_LOSS]: false,
  };
}

function simulation(state = getInitialState(), action) {
  switch (action.type) {
    case SIMULATE:
      if (action.error) {
        console.warn("Error running simulation", action.error);
        return state;
      }

      const simTypes = action.simTypes;

      if (simTypes.length === 0) {
        return getInitialState();
      }

      const updatedState = getInitialState();
      simTypes.forEach(simType => {
        updatedState[simType] = true;
      });

      return updatedState;
    default:
      return state;
  }
}

exports.simulation = simulation;
