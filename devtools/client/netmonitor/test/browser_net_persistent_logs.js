/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the network monitor leaks on initialization and sudden destruction.
 * You can also use this initialization format as a template for other tests.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SINGLE_GET_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  Services.prefs.setBoolPref("devtools.netmonitor.persistlog", false);

  yield reloadAndWait();

  // Using waitUntil in the test is necessary to ensure all requests are added correctly.
  // Because reloadAndWait call may catch early uncaught requests from initNetMonitor, so
  // the actual number of requests after reloadAndWait could be wrong since all requests
  // haven't finished.
  yield waitUntil(() => document.querySelectorAll(".request-list-item").length === 2);
  is(document.querySelectorAll(".request-list-item").length, 2,
    "The request list should have two items at this point.");

  yield reloadAndWait();

  yield waitUntil(() => document.querySelectorAll(".request-list-item").length === 2);
  // Since the reload clears the log, we still expect two requests in the log
  is(document.querySelectorAll(".request-list-item").length, 2,
    "The request list should still have two items at this point.");

  // Now we toggle the persistence logs on
  Services.prefs.setBoolPref("devtools.netmonitor.persistlog", true);

  yield reloadAndWait();

  yield waitUntil(() => document.querySelectorAll(".request-list-item").length === 4);
  // Since we togged the persistence logs, we expect four items after the reload
  is(document.querySelectorAll(".request-list-item").length, 4,
    "The request list should now have four items at this point.");

  Services.prefs.setBoolPref("devtools.netmonitor.persistlog", false);
  return teardown(monitor);

  /**
   * Reload the page and wait for 2 GET requests. Race-free.
   */
  function reloadAndWait() {
    let wait = waitForNetworkEvents(monitor, 2);
    tab.linkedBrowser.reload();
    return wait;
  }
});
