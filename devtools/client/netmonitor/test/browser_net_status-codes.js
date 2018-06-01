/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if requests display the correct status code and text in the UI.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { tab, monitor } = await initNetMonitor(STATUS_CODES_URL);

  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

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
        time: true
      }
    },
    {
      // request #1
      method: "GET",
      uri: STATUS_CODES_SJS + "?sts=200#doh",
      correctUri: STATUS_CODES_SJS + "?sts=200",
      details: {
        status: 202,
        statusText: "Created",
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8",
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 22),
        time: true
      }
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
        time: true
      }
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
        time: true
      }
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
        time: true
      }
    }
  ];

  // Execute requests.
  await performRequests(monitor, tab, 5);

  info("Performing tests");
  await verifyRequests();
  await testTab(0, testHeaders);
  await testTab(2, testParams);

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
      const item = getSortedRequests(store.getState()).get(index);
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
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]);

    await waitUntil(() => document.querySelector(
      "#headers-panel .tabpanel-summary-value.textbox-input"));

    const panel = document.querySelector("#headers-panel");
    const summaryValues = panel.querySelectorAll(".tabpanel-summary-value.textbox-input");
    const { method, correctUri, details: { status, statusText } } = data;
    const statusCode = panel.querySelector(".status-code");
    EventUtils.sendMouseEvent({ type: "mouseover" }, statusCode);
    await waitUntil(() => statusCode.title);

    is(summaryValues[0].value, correctUri,
      "The url summary value is incorrect.");
    is(summaryValues[1].value, method, "The method summary value is incorrect.");
    is(statusCode.dataset.code, status,
      "The status summary code is incorrect.");
    is(statusCode.getAttribute("title"), status + " " + statusText,
      "The status summary value is incorrect.");
  }

  /**
   * A function that tests "Params" panel contains correct information.
   */
  function testParams(data, index) {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]);
    EventUtils.sendMouseEvent({ type: "click" },
      document.querySelector("#params-tab"));

    const panel = document.querySelector("#params-panel");
    // Bug 1414981 - Request URL should not show #hash
    const statusParamValue = data.uri.split("=").pop().split("#")[0];
    const treeSections = panel.querySelectorAll(".tree-section");

    is(treeSections.length, 1,
      "There should be 1 param section displayed in this panel.");
    is(panel.querySelectorAll("tr:not(.tree-section).treeRow").length, 1,
      "There should be 1 param row displayed in this panel.");
    is(panel.querySelectorAll(".empty-notice").length, 0,
      "The empty notice should not be displayed in this panel.");

    const labels = panel
      .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
    const values = panel
      .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

    is(treeSections[0].querySelector(".treeLabel").textContent,
      L10N.getStr("paramsQueryString"),
      "The params scope doesn't have the correct title.");

    is(labels[0].textContent, "sts", "The param name was incorrect.");
    is(values[0].textContent, statusParamValue, "The param value was incorrect.");

    ok(panel.querySelector(".treeTable"),
      "The request params tree view should be displayed.");
    is(panel.querySelector(".editor-mount") === null,
      true,
      "The request post data editor should be hidden.");
  }
});
