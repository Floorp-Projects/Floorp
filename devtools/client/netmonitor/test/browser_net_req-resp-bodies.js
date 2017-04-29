/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if request and response body logging stays on after opening the console.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  let { tab, monitor } = yield initNetMonitor(JSON_LONG_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  // Perform first batch of requests.
  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  verifyRequest(0);

  // Switch to the webconsole.
  let onWebConsole = monitor.toolbox.once("webconsole-selected");
  monitor.toolbox.selectTool("webconsole");
  yield onWebConsole;

  // Switch back to the netmonitor.
  let onNetMonitor = monitor.toolbox.once("netmonitor-selected");
  monitor.toolbox.selectTool("netmonitor");
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

  function verifyRequest(index) {
    verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState()).get(index),
      "GET",
      CONTENT_TYPE_SJS + "?fmt=json-long",
      {
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
