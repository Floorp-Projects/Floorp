/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if requests display the correct status code and text in the UI.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  let { tab, monitor } = yield initNetMonitor(STATUS_CODES_URL);

  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  let requestItems = [];

  const REQUEST_DATA = [
    {
      // request #0
      method: "GET",
      uri: STATUS_CODES_SJS + "?sts=100",
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
      uri: STATUS_CODES_SJS + "?sts=200",
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

  let wait = waitForNetworkEvents(monitor, 5);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  info("Performing tests");
  yield verifyRequests();
  yield testTab(0, testHeaders);
  yield testTab(2, testParams);

  return teardown(monitor);

  /**
   * A helper that verifies all requests show the correct information and caches
   * request list items to requestItems array.
   */
  function* verifyRequests() {
    info("Verifying requests contain correct information.");
    let index = 0;
    for (let request of REQUEST_DATA) {
      let item = getSortedRequests(store.getState()).get(index);
      requestItems[index] = item;

      info("Verifying request #" + index);
      yield verifyRequestItemTarget(
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
  function* testTab(tabIdx, testFn) {
    let counter = 0;
    for (let item of REQUEST_DATA) {
      info("Testing tab #" + tabIdx + " to update with request #" + counter);
      yield testFn(item, counter);

      counter++;
    }
  }

  /**
   * A function that tests "Headers" panel contains correct information.
   */
  function* testHeaders(data, index) {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]);

    let panel = document.querySelector("#headers-panel");
    let summaryValues = panel.querySelectorAll(".tabpanel-summary-value.textbox-input");
    let { method, uri, details: { status, statusText } } = data;

    is(summaryValues[0].value, uri, "The url summary value is incorrect.");
    is(summaryValues[1].value, method, "The method summary value is incorrect.");
    is(panel.querySelector(".requests-list-status-icon").dataset.code, status,
      "The status summary code is incorrect.");
    is(summaryValues[3].value, status + " " + statusText,
      "The status summary value is incorrect.");
  }

  /**
   * A function that tests "Params" panel contains correct information.
   */
  function* testParams(data, index) {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]);
    EventUtils.sendMouseEvent({ type: "click" },
      document.querySelector("#params-tab"));

    let panel = document.querySelector("#params-panel");
    let statusParamValue = data.uri.split("=").pop();
    let treeSections = panel.querySelectorAll(".tree-section");

    is(treeSections.length, 1,
      "There should be 1 param section displayed in this panel.");
    is(panel.querySelectorAll("tr:not(.tree-section).treeRow").length, 1,
      "There should be 1 param row displayed in this panel.");
    is(panel.querySelectorAll(".empty-notice").length, 0,
      "The empty notice should not be displayed in this panel.");

    let labels = panel
      .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
    let values = panel
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
