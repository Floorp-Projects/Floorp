/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if toggling the details pane works as expected.
 */

add_task(function* () {
  let [tab, , monitor] = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { $, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;
  let { NETWORK_EVENT, TAB_UPDATED } = monitor.panelWin.EVENTS;
  RequestsMenu.lazyUpdate = false;

  let toggleButton = $("#details-pane-toggle");

  is(toggleButton.hasAttribute("disabled"), true,
    "The pane toggle button should be disabled when the frontend is opened.");
  is(toggleButton.classList.contains("pane-collapsed"), true,
    "The pane toggle button should indicate that the details pane is " +
    "collapsed when the frontend is opened.");
  is(NetMonitorView.detailsPaneHidden, true,
    "The details pane should be hidden when the frontend is opened.");
  is(RequestsMenu.selectedItem, null,
    "There should be no selected item in the requests menu.");

  let networkEvent = monitor.panelWin.once(NETWORK_EVENT);
  tab.linkedBrowser.reload();
  yield networkEvent;

  is(toggleButton.hasAttribute("disabled"), false,
    "The pane toggle button should be enabled after the first request.");
  is(toggleButton.classList.contains("pane-collapsed"), true,
    "The pane toggle button should still indicate that the details pane is " +
    "collapsed after the first request.");
  is(NetMonitorView.detailsPaneHidden, true,
    "The details pane should still be hidden after the first request.");
  is(RequestsMenu.selectedItem, null,
    "There should still be no selected item in the requests menu.");

  EventUtils.sendMouseEvent({ type: "mousedown" }, toggleButton);

  yield monitor.panelWin.once(TAB_UPDATED);

  is(toggleButton.hasAttribute("disabled"), false,
    "The pane toggle button should still be enabled after being pressed.");
  is(toggleButton.classList.contains("pane-collapsed"), false,
    "The pane toggle button should now indicate that the details pane is " +
    "not collapsed anymore after being pressed.");
  is(NetMonitorView.detailsPaneHidden, false,
    "The details pane should not be hidden after toggle button was pressed.");
  isnot(RequestsMenu.selectedItem, null,
    "There should be a selected item in the requests menu.");
  is(RequestsMenu.selectedIndex, 0,
    "The first item should be selected in the requests menu.");

  EventUtils.sendMouseEvent({ type: "mousedown" }, toggleButton);

  is(toggleButton.hasAttribute("disabled"), false,
    "The pane toggle button should still be enabled after being pressed again.");
  is(toggleButton.classList.contains("pane-collapsed"), true,
    "The pane toggle button should now indicate that the details pane is " +
    "collapsed after being pressed again.");
  is(NetMonitorView.detailsPaneHidden, true,
    "The details pane should now be hidden after the toggle button was pressed again.");
  is(RequestsMenu.selectedItem, null,
    "There should now be no selected item in the requests menu.");

  yield teardown(monitor);
});
