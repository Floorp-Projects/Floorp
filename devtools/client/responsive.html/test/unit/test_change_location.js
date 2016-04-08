/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test changing the location of the displayed page.

const { changeLocation } =
  require("devtools/client/responsive.html/actions/location");

const TEST_URL = "http://example.com";

add_task(function* () {
  let store = Store();
  const { getState, dispatch } = store;

  equal(getState().location, "about:blank",
        "Defaults to about:blank at startup");

  dispatch(changeLocation(TEST_URL));
  equal(getState().location, TEST_URL, "Location changed to TEST_URL");
});
