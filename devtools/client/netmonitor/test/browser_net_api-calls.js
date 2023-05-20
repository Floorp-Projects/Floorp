/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether API call URLs (without a filename) are correctly displayed
 * (including Unicode)
 */

add_task(async function () {
  const { tab, monitor } = await initNetMonitor(API_CALLS_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequests, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  const REQUEST_URIS = [
    "https://example.com/api/fileName.xml",
    "https://example.com/api/file%E2%98%A2.xml",
    "https://example.com/api/ascii/get/",
    "https://example.com/api/unicode/%E2%98%A2/",
    "https://example.com/api/search/?q=search%E2%98%A2",
  ];

  // Execute requests.
  await performRequests(monitor, tab, 5);

  REQUEST_URIS.forEach(function (uri, index) {
    verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState())[index],
      "GET",
      uri
    );
  });

  await teardown(monitor);
});
