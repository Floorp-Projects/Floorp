/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the clear button empties the request menu.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const clearButton = document.querySelector(".requests-list-clear-button");

  store.dispatch(Actions.batchEnable(false));

  // Make sure we start in a sane state
  assertNoRequestState();

  // Load one request and assert it shows up in the list
  let onMonitorUpdated = waitForAllRequestsFinished(monitor);
  tab.linkedBrowser.reload();
  await onMonitorUpdated;

  assertSingleRequestState();

  // Click clear and make sure the requests are gone
  EventUtils.sendMouseEvent({ type: "click" }, clearButton);
  assertNoRequestState();

  // Load a second request and make sure they still show up
  onMonitorUpdated = waitForAllRequestsFinished(monitor);
  tab.linkedBrowser.reload();
  await onMonitorUpdated;

  assertSingleRequestState();

  // Make sure we can now open the network details panel
  store.dispatch(Actions.toggleNetworkDetails());
  const detailsPanelToggleButton = document.querySelector(".sidebar-toggle");
  ok(detailsPanelToggleButton &&
    !detailsPanelToggleButton.classList.contains("pane-collapsed"),
    "The details pane should be visible.");

  // Click clear and make sure the details pane closes
  EventUtils.sendMouseEvent({ type: "click" }, clearButton);

  assertNoRequestState();
  ok(!document.querySelector(".network-details-panel"),
    "The details pane should not be visible clicking 'clear'.");

  return teardown(monitor);

  /**
   * Asserts the state of the network monitor when one request has loaded
   */
  function assertSingleRequestState() {
    is(store.getState().requests.requests.size, 1,
      "The request menu should have one item at this point.");
  }

  /**
   * Asserts the state of the network monitor when no requests have loaded
   */
  function assertNoRequestState() {
    is(store.getState().requests.requests.size, 0,
      "The request menu should be empty at this point.");
  }
});
