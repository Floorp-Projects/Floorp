/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that CORS preflight requests are displayed by network monitor
 */

add_task(function* () {
  let [tab, , monitor] = yield initNetMonitor(CORS_URL);
  let { RequestsMenu } = monitor.panelWin.NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1, 1);

  info("Performing a CORS request");
  let requestUrl = "http://test1.example.com" + CORS_SJS_PATH;
  yield ContentTask.spawn(tab.linkedBrowser, requestUrl, function* (url) {
    content.wrappedJSObject.performRequests(url, "triggering/preflight", "post-data");
  });

  info("Waiting until the requests appear in netmonitor");
  yield wait;

  info("Checking the preflight and flight methods");
  ["OPTIONS", "POST"].forEach((method, i) => {
    verifyRequestItemTarget(RequestsMenu.getItemAtIndex(i), method, requestUrl);
  });

  yield teardown(monitor);
});
