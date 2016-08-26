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
  let [tab, , monitor] = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  is(document.querySelector("#details-pane-toggle").hasAttribute("disabled"), true,
    "The pane toggle button should be disabled when the frontend is opened.");
  is(document.querySelector("#requests-menu-empty-notice").hasAttribute("hidden"), false,
    "An empty notice should be displayed when the frontend is opened.");
  is(RequestsMenu.itemCount, 0,
    "The requests menu should be empty when the frontend is opened.");
  is(NetMonitorView.detailsPaneHidden, true,
    "The details pane should be hidden when the frontend is opened.");

  yield reloadAndWait();

  is(document.querySelector("#details-pane-toggle").hasAttribute("disabled"), false,
    "The pane toggle button should be enabled after the first request.");
  is(document.querySelector("#requests-menu-empty-notice").hasAttribute("hidden"), true,
    "The empty notice should be hidden after the first request.");
  is(RequestsMenu.itemCount, 1,
    "The requests menu should not be empty after the first request.");
  is(NetMonitorView.detailsPaneHidden, true,
    "The details pane should still be hidden after the first request.");

  yield reloadAndWait();

  is(document.querySelector("#details-pane-toggle").hasAttribute("disabled"), false,
    "The pane toggle button should be still be enabled after a reload.");
  is(document.querySelector("#requests-menu-empty-notice").hasAttribute("hidden"), true,
    "The empty notice should be still hidden after a reload.");
  is(RequestsMenu.itemCount, 1,
    "The requests menu should not be empty after a reload.");
  is(NetMonitorView.detailsPaneHidden, true,
    "The details pane should still be hidden after a reload.");

  RequestsMenu.clear();

  is(document.querySelector("#details-pane-toggle").hasAttribute("disabled"), true,
    "The pane toggle button should be disabled when after clear.");
  is(document.querySelector("#requests-menu-empty-notice").hasAttribute("hidden"), false,
    "An empty notice should be displayed again after clear.");
  is(RequestsMenu.itemCount, 0,
    "The requests menu should be empty after clear.");
  is(NetMonitorView.detailsPaneHidden, true,
    "The details pane should be hidden after clear.");

  return teardown(monitor);

  function* reloadAndWait() {
    let wait = waitForNetworkEvents(monitor, 1);
    tab.linkedBrowser.reload();
    return wait;
  }
});
