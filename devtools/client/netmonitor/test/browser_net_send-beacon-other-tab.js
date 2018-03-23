/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if beacons from other tabs are properly ignored.
 */

add_task(async function() {
  let { tab, monitor } = await initNetMonitor(SIMPLE_URL);
  let { store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  let beaconTab = await addTab(SEND_BEACON_URL);
  info("Beacon tab added successfully.");

  is(store.getState().requests.requests.size, 0, "The requests menu should be empty.");

  let wait = waitForNetworkEvents(monitor, 1);
  await ContentTask.spawn(beaconTab.linkedBrowser, {}, async function() {
    content.wrappedJSObject.performRequests();
  });
  tab.linkedBrowser.reload();
  await wait;

  is(store.getState().requests.requests.size, 1, "Only the reload should be recorded.");
  let request = getSortedRequests(store.getState()).get(0);
  is(request.method, "GET", "The method is correct.");
  is(request.status, "200", "The status is correct.");

  await removeTab(beaconTab);
  return teardown(monitor);
});
