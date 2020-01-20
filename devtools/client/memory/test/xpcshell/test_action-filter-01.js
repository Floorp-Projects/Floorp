/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test setting the filter string.

const { setFilterString } = require("devtools/client/memory/actions/filter");

add_task(async function() {
  const store = Store();
  const { getState, dispatch } = store;

  equal(getState().filter, null, "no filter by default");

  dispatch(setFilterString("my filter"));
  equal(getState().filter, "my filter", "now we have the expected filter");

  dispatch(setFilterString(""));
  equal(getState().filter, null, "no filter again");
});
