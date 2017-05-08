/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that security details tab is no longer selected if an insecure request
 * is selected.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

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
  wait = waitForDOM(document, ".tabs");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  yield wait;

  info("Selecting security tab.");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelector("#security-tab"));

  info("Selecting insecure request.");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[1]);

  ok(document.querySelector("#headers-tab[aria-selected=true]"),
    "Selected tab was reset when selected security tab was hidden.");

  return teardown(monitor);
});
