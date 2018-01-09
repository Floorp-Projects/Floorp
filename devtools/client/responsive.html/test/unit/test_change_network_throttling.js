/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test changing the network throttling state

const {
  changeNetworkThrottling,
} = require("devtools/client/responsive.html/actions/network-throttling");

add_task(async function () {
  let store = Store();
  const { getState, dispatch } = store;

  ok(!getState().networkThrottling.enabled,
    "Network throttling is disabled by default.");
  equal(getState().networkThrottling.profile, "",
    "Network throttling profile is empty by default.");

  dispatch(changeNetworkThrottling(true, "Bob"));

  ok(getState().networkThrottling.enabled,
    "Network throttling is enabled.");
  equal(getState().networkThrottling.profile, "Bob",
    "Network throttling profile is set.");
});
