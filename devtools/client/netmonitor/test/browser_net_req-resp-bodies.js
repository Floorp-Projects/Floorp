/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if request and response body logging stays on after opening the console.
 */

add_task(function* () {
  let [tab, , monitor] = yield initNetMonitor(JSON_LONG_URL);
  info("Starting test... ");

  let { L10N, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  // Perform first batch of requests.
  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  verifyRequest(0);

  // Switch to the webconsole.
  let onWebConsole = monitor._toolbox.once("webconsole-selected");
  monitor._toolbox.selectTool("webconsole");
  yield onWebConsole;

  // Switch back to the netmonitor.
  let onNetMonitor = monitor._toolbox.once("netmonitor-selected");
  monitor._toolbox.selectTool("netmonitor");
  yield onNetMonitor;

  // Reload debugee.
  wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  yield wait;

  // Perform another batch of requests.
  wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  verifyRequest(1);

  return teardown(monitor);

  function verifyRequest(offset) {
    verifyRequestItemTarget(RequestsMenu.getItemAtIndex(offset),
      "GET", CONTENT_TYPE_SJS + "?fmt=json-long", {
        status: 200,
        statusText: "OK",
        type: "json",
        fullMimeType: "text/json; charset=utf-8",
        size: L10N.getFormatStr("networkMenu.sizeKB",
          L10N.numberWithDecimals(85975 / 1024, 2)),
        time: true
      });
  }
});
