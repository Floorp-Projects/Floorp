/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if requests display the correct status code and text in the UI.
 */

function test() {
  initNetMonitor(STATUS_CODES_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, L10N, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu, NetworkDetails } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;
    NetworkDetails._params.lazyEmpty = false;

    waitForNetworkEvents(aMonitor, 5).then(() => {
      let requestItems = [];

      verifyRequestItemTarget(requestItems[0] = RequestsMenu.getItemAtIndex(0),
        "GET", STATUS_CODES_SJS + "?sts=100", {
          status: 101,
          statusText: "Switching Protocols",
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0),
          time: true
        });
      verifyRequestItemTarget(requestItems[1] = RequestsMenu.getItemAtIndex(1),
        "GET", STATUS_CODES_SJS + "?sts=200", {
          status: 202,
          statusText: "Created",
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
          time: true
        });
      verifyRequestItemTarget(requestItems[2] = RequestsMenu.getItemAtIndex(2),
        "GET", STATUS_CODES_SJS + "?sts=300", {
          status: 303,
          statusText: "See Other",
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0),
          time: true
        });
      verifyRequestItemTarget(requestItems[3] = RequestsMenu.getItemAtIndex(3),
        "GET", STATUS_CODES_SJS + "?sts=400", {
          status: 404,
          statusText: "Not Found",
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
          time: true
        });
      verifyRequestItemTarget(requestItems[4] = RequestsMenu.getItemAtIndex(4),
        "GET", STATUS_CODES_SJS + "?sts=500", {
          status: 501,
          statusText: "Not Implemented",
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
          time: true
        });

      // Test summaries...
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[0]);

      EventUtils.sendMouseEvent({ type: "mousedown" }, requestItems[0].target);
      testSummary("GET", STATUS_CODES_SJS + "?sts=100", "101", "Switching Protocols");

      EventUtils.sendMouseEvent({ type: "mousedown" }, requestItems[1].target);
      testSummary("GET", STATUS_CODES_SJS + "?sts=200", "202", "Created");

      EventUtils.sendMouseEvent({ type: "mousedown" }, requestItems[2].target);
      testSummary("GET", STATUS_CODES_SJS + "?sts=300", "303", "See Other");

      EventUtils.sendMouseEvent({ type: "mousedown" }, requestItems[3].target);
      testSummary("GET", STATUS_CODES_SJS + "?sts=400", "404", "Not Found");

      EventUtils.sendMouseEvent({ type: "mousedown" }, requestItems[4].target);
      testSummary("GET", STATUS_CODES_SJS + "?sts=500", "501", "Not Implemented");

      // Test params...
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[2]);

      EventUtils.sendMouseEvent({ type: "mousedown" }, requestItems[0].target);
      testParamsTab("\"100\"");

      EventUtils.sendMouseEvent({ type: "mousedown" }, requestItems[1].target);
      testParamsTab("\"200\"");

      EventUtils.sendMouseEvent({ type: "mousedown" }, requestItems[2].target);
      testParamsTab("\"300\"");

      EventUtils.sendMouseEvent({ type: "mousedown" }, requestItems[3].target);
      testParamsTab("\"400\"");

      EventUtils.sendMouseEvent({ type: "mousedown" }, requestItems[4].target);
      testParamsTab("\"500\"");

      // We're done here.
      teardown(aMonitor).then(finish);

      function testSummary(aMethod, aUrl, aStatus, aStatusText) {
        let tab = document.querySelectorAll("#details-pane tab")[0];
        let tabpanel = document.querySelectorAll("#details-pane tabpanel")[0];

        is(tabpanel.querySelector("#headers-summary-url-value").getAttribute("value"),
          aUrl, "The url summary value is incorrect.");
        is(tabpanel.querySelector("#headers-summary-method-value").getAttribute("value"),
          aMethod, "The method summary value is incorrect.");
        is(tabpanel.querySelector("#headers-summary-status-circle").getAttribute("code"),
          aStatus, "The status summary code is incorrect.");
        is(tabpanel.querySelector("#headers-summary-status-value").getAttribute("value"),
          aStatus + " " + aStatusText, "The status summary value is incorrect.");
      }

      function testParamsTab(aStatusParamValue) {
        let tab = document.querySelectorAll("#details-pane tab")[2];
        let tabpanel = document.querySelectorAll("#details-pane tabpanel")[2];

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

        is(paramsScope.querySelectorAll(".variables-view-variable .name")[0].getAttribute("value"),
          "sts", "The param name was incorrect.");
        is(paramsScope.querySelectorAll(".variables-view-variable .value")[0].getAttribute("value"),
          aStatusParamValue, "The param value was incorrect.");

        is(tabpanel.querySelector("#request-params-box")
          .hasAttribute("hidden"), false,
          "The request params box should not be hidden.");
        is(tabpanel.querySelector("#request-post-data-textarea-box")
          .hasAttribute("hidden"), true,
          "The request post data textarea box should be hidden.");
      }
    });

    aDebuggee.performRequests();
  });
}
