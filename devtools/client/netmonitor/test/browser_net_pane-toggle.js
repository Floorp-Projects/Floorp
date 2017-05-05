/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if toggling the details pane works as expected.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { EVENTS } = windowRequire("devtools/client/netmonitor/src/constants");
  let {
    getSelectedRequest,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  let toggleButton = document.querySelector(".network-details-panel-toggle");

  is(toggleButton.hasAttribute("disabled"), true,
    "The pane toggle button should be disabled when the frontend is opened.");
  is(toggleButton.classList.contains("pane-collapsed"), true,
    "The pane toggle button should indicate that the details pane is " +
    "collapsed when the frontend is opened.");
  is(!!document.querySelector(".network-details-panel"), false,
    "The details pane should be hidden when the frontend is opened.");
  is(getSelectedRequest(store.getState()), null,
    "There should be no selected item in the requests menu.");

  let networkEvent = monitor.panelWin.once(EVENTS.NETWORK_EVENT);
  tab.linkedBrowser.reload();
  yield networkEvent;

  is(toggleButton.hasAttribute("disabled"), false,
    "The pane toggle button should be enabled after the first request.");
  is(toggleButton.classList.contains("pane-collapsed"), true,
    "The pane toggle button should still indicate that the details pane is " +
    "collapsed after the first request.");
  is(!!document.querySelector(".network-details-panel"), false,
    "The details pane should still be hidden after the first request.");
  is(getSelectedRequest(store.getState()), null,
    "There should still be no selected item in the requests menu.");

  EventUtils.sendMouseEvent({ type: "click" }, toggleButton);

  is(toggleButton.hasAttribute("disabled"), false,
    "The pane toggle button should still be enabled after being pressed.");
  is(toggleButton.classList.contains("pane-collapsed"), false,
    "The pane toggle button should now indicate that the details pane is " +
    "not collapsed anymore after being pressed.");
  is(!!document.querySelector(".network-details-panel"), true,
    "The details pane should not be hidden after toggle button was pressed.");
  isnot(getSelectedRequest(store.getState()), null,
    "There should be a selected item in the requests menu.");
  is(getSelectedIndex(store.getState()), 0,
    "The first item should be selected in the requests menu.");

  EventUtils.sendMouseEvent({ type: "click" }, toggleButton);

  is(toggleButton.hasAttribute("disabled"), false,
    "The pane toggle button should still be enabled after being pressed again.");
  is(toggleButton.classList.contains("pane-collapsed"), true,
    "The pane toggle button should now indicate that the details pane is " +
    "collapsed after being pressed again.");
  is(!!document.querySelector(".network-details-panel"), false,
    "The details pane should now be hidden after the toggle button was pressed again.");
  is(getSelectedRequest(store.getState()), null,
    "There should now be no selected item in the requests menu.");

  yield teardown(monitor);

  function getSelectedIndex(state) {
    if (!state.requests.selectedId) {
      return -1;
    }
    return getSortedRequests(state).findIndex(r => r.id === state.requests.selectedId);
  }
});
