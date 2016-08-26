/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the clear button empties the request menu.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { $, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;
  let detailsPane = $("#details-pane");
  let detailsPaneToggleButton = $("#details-pane-toggle");
  let clearButton = $("#requests-menu-clear-button");

  RequestsMenu.lazyUpdate = false;

  // Make sure we start in a sane state
  assertNoRequestState(RequestsMenu, detailsPaneToggleButton);

  // Load one request and assert it shows up in the list
  let networkEvent = monitor.panelWin.once(monitor.panelWin.EVENTS.NETWORK_EVENT);
  tab.linkedBrowser.reload();
  yield networkEvent;

  assertSingleRequestState();

  // Click clear and make sure the requests are gone
  EventUtils.sendMouseEvent({ type: "click" }, clearButton);
  assertNoRequestState();

  // Load a second request and make sure they still show up
  networkEvent = monitor.panelWin.once(monitor.panelWin.EVENTS.NETWORK_EVENT);
  tab.linkedBrowser.reload();
  yield networkEvent;

  assertSingleRequestState();

  // Make sure we can now open the details pane
  NetMonitorView.toggleDetailsPane({ visible: true, animated: false });
  ok(!detailsPane.classList.contains("pane-collapsed") &&
    !detailsPaneToggleButton.classList.contains("pane-collapsed"),
    "The details pane should be visible after clicking the toggle button.");

  // Click clear and make sure the details pane closes
  EventUtils.sendMouseEvent({ type: "click" }, clearButton);
  assertNoRequestState();
  ok(detailsPane.classList.contains("pane-collapsed") &&
    detailsPaneToggleButton.classList.contains("pane-collapsed"),
    "The details pane should not be visible clicking 'clear'.");

  return teardown(monitor);

  /**
   * Asserts the state of the network monitor when one request has loaded
   */
  function assertSingleRequestState() {
    is(RequestsMenu.itemCount, 1,
      "The request menu should have one item at this point.");
    is(detailsPaneToggleButton.hasAttribute("disabled"), false,
      "The pane toggle button should be enabled after a request is made.");
  }

  /**
   * Asserts the state of the network monitor when no requests have loaded
   */
  function assertNoRequestState() {
    is(RequestsMenu.itemCount, 0,
      "The request menu should be empty at this point.");
    is(detailsPaneToggleButton.hasAttribute("disabled"), true,
      "The pane toggle button should be disabled when the request menu is cleared.");
  }
});
