/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test rotating the viewport.

const { addViewport, rotateViewport } =
  require("devtools/client/responsive.html/actions/viewports");

add_task(async function () {
  let store = Store();
  const { getState, dispatch } = store;

  dispatch(addViewport());

  let viewport = getState().viewports[0];
  equal(viewport.width, 320, "Default width of 320");
  equal(viewport.height, 480, "Default height of 480");

  dispatch(rotateViewport(0));
  viewport = getState().viewports[0];
  equal(viewport.width, 480, "Rotated width of 480");
  equal(viewport.height, 320, "Rotated height of 320");
});
