/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if cached requests have the correct status code
 */

add_task(async function() {
  // Disable rcwn to make cache behavior deterministic.
  await pushPref("network.http.rcwn.enabled", false);

  const { tab, monitor } = await initNetMonitor(STATUS_CODES_URL, true);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  const REQUEST_DATA = [
    {
      method: "GET",
      uri: STATUS_CODES_SJS + "?sts=ok&cached",
      details: {
        status: 200,
        statusText: "OK",
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8"
      }
    },
    {
      method: "GET",
      uri: STATUS_CODES_SJS + "?sts=redirect&cached",
      details: {
        status: 301,
        statusText: "Moved Permanently",
        type: "html",
        fullMimeType: "text/html; charset=utf-8"
      }
    },
    {
      method: "GET",
      uri: "http://example.com/redirected",
      details: {
        status: 404,
        statusText: "Not Found",
        type: "html",
        fullMimeType: "text/html; charset=utf-8"
      }
    },
    {
      method: "GET",
      uri: STATUS_CODES_SJS + "?sts=ok&cached",
      details: {
        status: 200,
        statusText: "OK (cached)",
        displayedStatus: "cached",
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8"
      }
    },
    {
      method: "GET",
      uri: STATUS_CODES_SJS + "?sts=redirect&cached",
      details: {
        status: 301,
        statusText: "Moved Permanently (cached)",
        displayedStatus: "cached",
        type: "html",
        fullMimeType: "text/html; charset=utf-8"
      }
    },
    {
      method: "GET",
      uri: "http://example.com/redirected",
      details: {
        status: 404,
        statusText: "Not Found",
        type: "html",
        fullMimeType: "text/html; charset=utf-8"
      }
    }
  ];

  info("Performing requests #1...");
  await performRequestsAndWait();

  info("Performing requests #2...");
  await performRequestsAndWait();

  let index = 0;
  for (const request of REQUEST_DATA) {
    const requestItem = document.querySelectorAll(".request-list-item")[index];
    requestItem.scrollIntoView();
    const requestsListStatus = requestItem.querySelector(".status-code");
    EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
    await waitUntil(() => requestsListStatus.title);

    info("Verifying request #" + index);
    await verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState()).get(index),
      request.method,
      request.uri,
      request.details
    );

    index++;
  }

  await teardown(monitor);

  async function performRequestsAndWait() {
    const wait = waitForNetworkEvents(monitor, 3);
    await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
      content.wrappedJSObject.performCachedRequests();
    });
    await wait;
  }
});
