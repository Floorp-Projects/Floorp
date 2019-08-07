/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test updating the touch simulation `enabled` property

const {
  toggleTouchSimulation,
} = require("devtools/client/responsive/actions/ui");

add_task(async function() {
  const store = Store();
  const { getState, dispatch } = store;

  ok(
    !getState().ui.touchSimulationEnabled,
    "Touch simulation is disabled by default."
  );

  dispatch(toggleTouchSimulation(true));

  ok(getState().ui.touchSimulationEnabled, "Touch simulation is enabled.");
});
