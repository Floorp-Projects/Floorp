/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether API call URLs (without a filename) are correctly displayed
 * (including Unicode)
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(API_CALLS_URL);
  info("Starting test... ");

  let { NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  const REQUEST_URIS = [
    "http://example.com/api/fileName.xml",
    "http://example.com/api/file%E2%98%A2.xml",
    "http://example.com/api/ascii/get/",
    "http://example.com/api/unicode/%E2%98%A2/",
    "http://example.com/api/search/?q=search%E2%98%A2"
  ];

  let wait = waitForNetworkEvents(monitor, 5);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  REQUEST_URIS.forEach(function (uri, index) {
    verifyRequestItemTarget(RequestsMenu.getItemAtIndex(index), "GET", uri);
  });

  yield teardown(monitor);
});
