/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test changing the display pixel ratio.

const {
  changeDisplayPixelRatio,
} = require("devtools/client/responsive/actions/ui");

const NEW_PIXEL_RATIO = 5.5;

add_task(async function() {
  const store = Store();
  const { getState, dispatch } = store;

  equal(getState().ui.displayPixelRatio, 0, "Defaults to 0 at startup");

  dispatch(changeDisplayPixelRatio(NEW_PIXEL_RATIO));
  equal(
    getState().ui.displayPixelRatio,
    NEW_PIXEL_RATIO,
    `Display Pixel Ratio changed to ${NEW_PIXEL_RATIO}`
  );
});
