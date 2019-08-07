/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test resizing the viewport.

const {
  addViewport,
  resizeViewport,
} = require("devtools/client/responsive/actions/viewports");
const {
  toggleTouchSimulation,
} = require("devtools/client/responsive/actions/ui");

add_task(async function() {
  const store = Store();
  const { getState, dispatch } = store;

  dispatch(addViewport());
  dispatch(resizeViewport(0, 500, 500));

  let viewport = getState().viewports[0];
  equal(viewport.width, 500, "Resized width of 500");
  equal(viewport.height, 500, "Resized height of 500");

  dispatch(toggleTouchSimulation(true));
  dispatch(resizeViewport(0, 400, 400));

  viewport = getState().viewports[0];
  equal(viewport.width, 400, "Resized width of 400 (with touch simulation on)");
  equal(
    viewport.height,
    400,
    "Resized height of 400 (with touch simulation on)"
  );
});
