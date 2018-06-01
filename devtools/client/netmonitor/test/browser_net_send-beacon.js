/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if beacons are handled correctly.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SEND_BEACON_URL);
  const { store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getSortedRequests } =
    windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  is(store.getState().requests.requests.size, 0, "The requests menu should be empty.");

  // Execute requests.
  await performRequests(monitor, tab, 1);

  is(store.getState().requests.requests.size, 1, "The beacon should be recorded.");
  const request = getSortedRequests(store.getState()).get(0);
  is(request.method, "POST", "The method is correct.");
  ok(request.url.endsWith("beacon_request"), "The URL is correct.");
  is(request.status, "404", "The status is correct.");

  return teardown(monitor);
});
