/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if requests display the correct status code and text in the UI.
 */

add_task(async function () {
  const {
    L10N,
  } = require("resource://devtools/client/netmonitor/src/utils/l10n.js");

  const { tab, monitor } = await initNetMonitor(STATUS_CODES_URL, {
    requestCount: 1,
  });

  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequests, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  const requestItems = [];

  const REQUEST_DATA = [
    {
      // request #0
      method: "GET",
      uri: STATUS_CODES_SJS + "?sts=100",
      correctUri: STATUS_CODES_SJS + "?sts=100",
      details: {
        status: 101,
        statusText: "Switching Protocols",
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8",
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 0),
        time: true,
      },
    },
    {
      // request #1
      method: "GET",
      uri: STATUS_CODES_SJS + "?sts=200",
      correctUri: STATUS_CODES_SJS + "?sts=200",
      details: {
        status: 202,
        statusText: "Created",
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8",
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 22),
        time: true,
      },
    },
    {
      // request #2
      method: "GET",
      uri: STATUS_CODES_SJS + "?sts=300",
      correctUri: STATUS_CODES_SJS + "?sts=300",
      details: {
        status: 303,
        statusText: "See Other",
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8",
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 22),
        time: true,
      },
    },
    {
      // request #3
      method: "GET",
      uri: STATUS_CODES_SJS + "?sts=400",
      correctUri: STATUS_CODES_SJS + "?sts=400",
      details: {
        status: 404,
        statusText: "Not Found",
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8",
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 22),
        time: true,
      },
    },
    {
      // request #4
      method: "GET",
      uri: STATUS_CODES_SJS + "?sts=500",
      correctUri: STATUS_CODES_SJS + "?sts=500",
      details: {
        status: 501,
        statusText: "Not Implemented",
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8",
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 22),
        time: true,
      },
    },
  ];

  // Execute requests.
  await performRequests(monitor, tab, 5);

  info("Performing tests");
  await verifyRequests();
  await testTab(0, testHeaders);

  return teardown(monitor);

  /**
   * A helper that verifies all requests show the correct information and caches
   * request list items to requestItems array.
   */
  async function verifyRequests() {
    const requestListItems = document.querySelectorAll(".request-list-item");
    for (const requestItem of requestListItems) {
      requestItem.scrollIntoView();
      const requestsListStatus = requestItem.querySelector(".status-code");
      EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
      await waitUntil(() => requestsListStatus.title);
    }

    info("Verifying requests contain correct information.");
    let index = 0;
    for (const request of REQUEST_DATA) {
      const item = getSortedRequests(store.getState())[index];
      requestItems[index] = item;

      info("Verifying request #" + index);
      await verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        item,
        request.method,
        request.uri,
        request.details
      );

      index++;
    }
  }

  /**
   * A helper that opens a given tab of request details pane, selects and passes
   * all requests to the given test function.
   *
   * @param Number tabIdx
   *               The index of tab to activate.
   * @param Function testFn(requestItem)
   *        A function that should perform all necessary tests. It's called once
   *        for every item of REQUEST_DATA with that item being selected in the
   *        NetworkMonitor.
   */
  async function testTab(tabIdx, testFn) {
    let counter = 0;
    for (const item of REQUEST_DATA) {
      info("Testing tab #" + tabIdx + " to update with request #" + counter);
      await testFn(item, counter);

      counter++;
    }
  }

  /**
   * A function that tests "Headers" panel contains correct information.
   */
  async function testHeaders(data, index) {
    EventUtils.sendMouseEvent(
      { type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]
    );

    // wait till all the summary section is loaded
    await waitUntil(() =>
      document.querySelector("#headers-panel .tabpanel-summary-value")
    );
    const panel = document.querySelector("#headers-panel");
    const {
      method,
      correctUri,
      details: { status, statusText },
    } = data;

    const statusCode = panel.querySelector(".status-code");

    EventUtils.sendMouseEvent({ type: "mouseover" }, statusCode);
    await waitUntil(() => statusCode.title);

    is(
      panel.querySelector(".url-preview .url").textContent,
      correctUri,
      "The url summary value is incorrect."
    );
    is(
      panel.querySelectorAll(".treeLabel")[0].textContent,
      method,
      "The method value is incorrect."
    );
    is(
      parseInt(statusCode.dataset.code, 10),
      status,
      "The status summary code is incorrect."
    );
    is(
      statusCode.getAttribute("title"),
      status + " " + statusText,
      "The status summary value is incorrect."
    );
  }
});
