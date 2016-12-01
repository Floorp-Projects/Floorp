/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if requests display the correct status code and text in the UI.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/l10n");

  let { tab, monitor } = yield initNetMonitor(STATUS_CODES_URL);

  info("Starting test... ");

  let { document, EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;
  let requestItems = [];

  RequestsMenu.lazyUpdate = false;
  NetworkDetails._params.lazyEmpty = false;

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
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 0),
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
  yield testTab(0, testSummary);
  yield testTab(2, testParams);

  return teardown(monitor);

  /**
   * A helper that verifies all requests show the correct information and caches
   * RequestsMenu items to requestItems array.
   */
  function* verifyRequests() {
    info("Verifying requests contain correct information.");
    let index = 0;
    for (let request of REQUEST_DATA) {
      let item = RequestsMenu.getItemAtIndex(index);
      requestItems[index] = item;

      info("Verifying request #" + index);
      yield verifyRequestItemTarget(RequestsMenu, item,
        request.method, request.uri, request.details);

      index++;
    }
  }

  /**
   * A helper that opens a given tab of request details pane, selects and passes
   * all requests to the given test function.
   *
   * @param Number tabIdx
   *               The index of NetworkDetails tab to activate.
   * @param Function testFn(requestItem)
   *        A function that should perform all necessary tests. It's called once
   *        for every item of REQUEST_DATA with that item being selected in the
   *        NetworkMonitor.
   */
  function* testTab(tabIdx, testFn) {
    info("Testing tab #" + tabIdx);
    EventUtils.sendMouseEvent({ type: "mousedown" },
          document.querySelectorAll("#details-pane tab")[tabIdx]);

    let counter = 0;
    for (let item of REQUEST_DATA) {
      info("Waiting tab #" + tabIdx + " to update with request #" + counter);
      yield chooseRequest(counter);

      info("Tab updated. Performing checks");
      yield testFn(item);

      counter++;
    }
  }

  /**
   * A function that tests "Summary" contains correct information.
   */
  function* testSummary(data) {
    let tabpanel = document.querySelectorAll("#details-pane tabpanel")[0];

    let { method, uri, details: { status, statusText } } = data;
    is(tabpanel.querySelector("#headers-summary-url-value").getAttribute("value"),
      uri, "The url summary value is incorrect.");
    is(tabpanel.querySelector("#headers-summary-method-value").getAttribute("value"),
      method, "The method summary value is incorrect.");
    is(tabpanel.querySelector("#headers-summary-status-circle").getAttribute("data-code"),
      status, "The status summary code is incorrect.");
    is(tabpanel.querySelector("#headers-summary-status-value").getAttribute("value"),
      status + " " + statusText, "The status summary value is incorrect.");
  }

  /**
   * A function that tests "Params" tab contains correct information.
   */
  function* testParams(data) {
    let tabpanel = document.querySelectorAll("#details-pane tabpanel")[2];
    let statusParamValue = data.uri.split("=").pop();
    let statusParamShownValue = "\"" + statusParamValue + "\"";

    is(tabpanel.querySelectorAll(".variables-view-scope").length, 1,
      "There should be 1 param scope displayed in this tabpanel.");
    is(tabpanel.querySelectorAll(".variable-or-property").length, 1,
      "There should be 1 param value displayed in this tabpanel.");
    is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
      "The empty notice should not be displayed in this tabpanel.");

    let paramsScope = tabpanel.querySelectorAll(".variables-view-scope")[0];

    is(paramsScope.querySelector(".name").getAttribute("value"),
      L10N.getStr("paramsQueryString"),
      "The params scope doesn't have the correct title.");

    is(paramsScope.querySelectorAll(".variables-view-variable .name")[0]
      .getAttribute("value"),
      "sts", "The param name was incorrect.");
    is(paramsScope.querySelectorAll(".variables-view-variable .value")[0]
      .getAttribute("value"),
      statusParamShownValue, "The param value was incorrect.");

    is(tabpanel.querySelector("#request-params-box")
      .hasAttribute("hidden"), false,
      "The request params box should not be hidden.");
    is(tabpanel.querySelector("#request-post-data-textarea-box")
      .hasAttribute("hidden"), true,
      "The request post data textarea box should be hidden.");
  }

  /**
   * A helper that clicks on a specified request and returns a promise resolved
   * when NetworkDetails has been populated with the data of the given request.
   */
  function chooseRequest(index) {
    let onTabUpdated = monitor.panelWin.once(EVENTS.TAB_UPDATED);
    let target = getItemTarget(RequestsMenu, requestItems[index]);
    EventUtils.sendMouseEvent({ type: "mousedown" }, target);
    return onTabUpdated;
  }
});
