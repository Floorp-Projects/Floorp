/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test that security details tab is no longer selected if an insecure request
 * is selected.
 */

add_task(function* () {
  let [tab, debuggee, monitor] = yield initNetMonitor(CUSTOM_GET_URL);
  let { $, EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  info("Performing requests.");
  debuggee.performRequests(1, "https://example.com" + CORS_SJS_PATH);
  debuggee.performRequests(1, "http://example.com" + CORS_SJS_PATH);
  yield waitForNetworkEvents(monitor, 2);

  info("Selecting secure request.");
  RequestsMenu.selectedIndex = 0;

  info("Selecting security tab.");
  NetworkDetails.widget.selectedIndex = 5;

  info("Selecting insecure request.");
  RequestsMenu.selectedIndex = 1;

  info("Waiting for security tab to be updated.");
  yield monitor.panelWin.once(EVENTS.NETWORKDETAILSVIEW_POPULATED);

  is(NetworkDetails.widget.selectedIndex, 0,
    "Selected tab was reset when selected security tab was hidden.");

  yield teardown(monitor);
});
