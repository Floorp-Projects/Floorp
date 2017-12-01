/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that CORS preflight requests are displayed by network monitor
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CORS_URL);

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1, 1);

  info("Performing a CORS request");
  let requestUrl = "http://test1.example.com" + CORS_SJS_PATH;
  yield ContentTask.spawn(tab.linkedBrowser, requestUrl, function* (url) {
    content.wrappedJSObject.performRequests(url, "triggering/preflight", "post-data");
  });

  info("Waiting until the requests appear in netmonitor");
  yield wait;

  info("Checking the preflight and flight methods");
  ["OPTIONS", "POST"].forEach((method, index) => {
    verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState()).get(index),
      method,
      requestUrl
    );
  });

  yield teardown(monitor);
});
