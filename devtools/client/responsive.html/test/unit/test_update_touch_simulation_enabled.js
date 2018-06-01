/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test updating the touch simulation `enabled` property

const {
  changeTouchSimulation,
} = require("devtools/client/responsive.html/actions/touch-simulation");

add_task(async function() {
  const store = Store();
  const { getState, dispatch } = store;

  ok(!getState().touchSimulation.enabled,
    "Touch simulation is disabled by default.");

  dispatch(changeTouchSimulation(true));

  ok(getState().touchSimulation.enabled,
    "Touch simulation is enabled.");
});
