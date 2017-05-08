/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the clear button empties the request menu.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { EVENTS } = windowRequire("devtools/client/netmonitor/src/constants");
  let detailsPanelToggleButton = document.querySelector(".network-details-panel-toggle");
  let clearButton = document.querySelector(".requests-list-clear-button");

  store.dispatch(Actions.batchEnable(false));

  // Make sure we start in a sane state
  assertNoRequestState();

  // Load one request and assert it shows up in the list
  let networkEvent = monitor.panelWin.once(EVENTS.NETWORK_EVENT);
  tab.linkedBrowser.reload();
  yield networkEvent;

  assertSingleRequestState();

  // Click clear and make sure the requests are gone
  EventUtils.sendMouseEvent({ type: "click" }, clearButton);
  assertNoRequestState();

  // Load a second request and make sure they still show up
  networkEvent = monitor.panelWin.once(EVENTS.NETWORK_EVENT);
  tab.linkedBrowser.reload();
  yield networkEvent;

  assertSingleRequestState();

  // Make sure we can now open the network details panel
  EventUtils.sendMouseEvent({ type: "click" }, detailsPanelToggleButton);

  ok(document.querySelector(".network-details-panel") &&
    !detailsPanelToggleButton.classList.contains("pane-collapsed"),
    "The details pane should be visible after clicking the toggle button.");

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
    is(detailsPanelToggleButton.hasAttribute("disabled"), false,
      "The pane toggle button should be enabled after a request is made.");
  }

  /**
   * Asserts the state of the network monitor when no requests have loaded
   */
  function assertNoRequestState() {
    is(store.getState().requests.requests.size, 0,
      "The request menu should be empty at this point.");
    is(detailsPanelToggleButton.hasAttribute("disabled"), true,
      "The pane toggle button should be disabled when the request menu is cleared.");
  }
});
