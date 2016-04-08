/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test adding viewports to the page.

const { addViewport } =
  require("devtools/client/responsive.html/actions/viewports");

add_task(function* () {
  let store = Store();
  const { getState, dispatch } = store;

  equal(getState().viewports.length, 0, "Defaults to no viewpots at startup");

  dispatch(addViewport());
  equal(getState().viewports.length, 1, "One viewport total");

  // For the moment, there can be at most one viewport.
  dispatch(addViewport());
  equal(getState().viewports.length, 1, "One viewport total, again");
});
