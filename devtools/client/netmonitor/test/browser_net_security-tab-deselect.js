/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that security details tab is no longer selected if an insecure request
 * is selected.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  let { EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  info("Performing requests.");
  let wait = waitForNetworkEvents(monitor, 2);
  const REQUEST_URLS = [
    "https://example.com" + CORS_SJS_PATH,
    "http://example.com" + CORS_SJS_PATH,
  ];
  yield ContentTask.spawn(tab.linkedBrowser, REQUEST_URLS, function* (urls) {
    for (let url of urls) {
      content.wrappedJSObject.performRequests(1, url);
    }
  });
  yield wait;

  info("Selecting secure request.");
  RequestsMenu.selectedIndex = 0;

  info("Selecting security tab.");
  NetworkDetails.widget.selectedIndex = 5;

  wait = monitor.panelWin.once(EVENTS.NETWORKDETAILSVIEW_POPULATED);
  info("Selecting insecure request.");
  RequestsMenu.selectedIndex = 1;

  info("Waiting for security tab to be updated.");
  yield wait;

  is(NetworkDetails.widget.selectedIndex, 0,
    "Selected tab was reset when selected security tab was hidden.");

  return teardown(monitor);
});
