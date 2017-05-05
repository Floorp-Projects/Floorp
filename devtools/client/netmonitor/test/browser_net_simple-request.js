/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test whether the UI state properly reflects existence of requests
 * displayed in the Net panel. The following parts of the UI are
 * tested:
 * 1) Side panel visibility
 * 2) Side panel toggle button
 * 3) Empty user message visibility
 * 4) Number of requests displayed
 */
add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  is(document.querySelector(".network-details-panel-toggle").hasAttribute("disabled"),
    true,
    "The pane toggle button should be disabled when the frontend is opened.");
  ok(document.querySelector(".request-list-empty-notice"),
    "An empty notice should be displayed when the frontend is opened.");
  is(store.getState().requests.requests.size, 0,
    "The requests menu should be empty when the frontend is opened.");
  is(!!document.querySelector(".network-details-panel"), false,
    "The network details panel should be hidden when the frontend is opened.");

  yield reloadAndWait();

  is(document.querySelector(".network-details-panel-toggle").hasAttribute("disabled"),
    false,
    "The pane toggle button should be enabled after the first request.");
  ok(!document.querySelector(".request-list-empty-notice"),
    "The empty notice should be hidden after the first request.");
  is(store.getState().requests.requests.size, 1,
    "The requests menu should not be empty after the first request.");
  is(!!document.querySelector(".network-details-panel"), false,
    "The network details panel should still be hidden after the first request.");

  yield reloadAndWait();

  is(document.querySelector(".network-details-panel-toggle").hasAttribute("disabled"),
    false,
    "The pane toggle button should be still be enabled after a reload.");
  ok(!document.querySelector(".request-list-empty-notice"),
    "The empty notice should be still hidden after a reload.");
  is(store.getState().requests.requests.size, 1,
    "The requests menu should not be empty after a reload.");
  is(!!document.querySelector(".network-details-panel"), false,
    "The network details panel should still be hidden after a reload.");

  store.dispatch(Actions.clearRequests());

  is(document.querySelector(".network-details-panel-toggle").hasAttribute("disabled"),
    true,
    "The pane toggle button should be disabled when after clear.");
  ok(document.querySelector(".request-list-empty-notice"),
    "An empty notice should be displayed again after clear.");
  is(store.getState().requests.requests.size, 0,
    "The requests menu should be empty after clear.");
  is(!!document.querySelector(".network-details-panel"), false,
    "The network details panel should still be hidden after clear.");

  return teardown(monitor);

  function* reloadAndWait() {
    let wait = waitForNetworkEvents(monitor, 1);
    tab.linkedBrowser.reload();
    return wait;
  }
});
