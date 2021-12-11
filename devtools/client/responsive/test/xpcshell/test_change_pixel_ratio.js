/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test changing the viewport pixel ratio.

const {
  addViewport,
  changePixelRatio,
} = require("devtools/client/responsive/actions/viewports");
const NEW_PIXEL_RATIO = 5.5;

add_task(async function() {
  const store = Store();
  const { getState, dispatch } = store;

  dispatch(addViewport());
  dispatch(changePixelRatio(0, NEW_PIXEL_RATIO));

  const viewport = getState().viewports[0];
  equal(
    viewport.pixelRatio,
    NEW_PIXEL_RATIO,
    `Viewport's pixel ratio changed to ${NEW_PIXEL_RATIO}`
  );
});
