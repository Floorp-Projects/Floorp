/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if very long JSON responses are handled correctly.
 */

function test() {
  initNetMonitor(JSON_LONG_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    // This is receiving over 80 KB of json and will populate over 6000 items
    // in a variables view instance. Debug builds are slow.
    requestLongerTimeout(2);

    let { document, L10N, SourceEditor, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 1).then(() => {
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(0),
        "GET", CONTENT_TYPE_SJS + "?fmt=json-long", {
          status: 200,
          statusText: "OK",
          type: "json",
          fullMimeType: "text/json; charset=utf-8",
          size: L10N.getFormatStr("networkMenu.sizeKB", 83.96),
          time: true
        });

      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.getElementById("details-pane-toggle"));
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[3]);

      aMonitor.panelWin.once("NetMonitor:ResponseBodyAvailable", () => {
        testResponseTab();
        teardown(aMonitor).then(finish);
      });

      function testResponseTab() {
        let tab = document.querySelectorAll("#details-pane tab")[3];
        let tabpanel = document.querySelectorAll("#details-pane tabpanel")[3];

        is(tab.getAttribute("selected"), "true",
          "The response tab in the network details pane should be selected.");

        is(tabpanel.querySelector("#response-content-json-box")
          .hasAttribute("hidden"), false,
          "The response content json box doesn't have the intended visibility.");

        is(tabpanel.querySelector("#response-content-textarea-box")
          .hasAttribute("hidden"), true,
          "The response content textarea box doesn't have the intended visibility.");

        is(tabpanel.querySelector("#response-content-image-box")
          .hasAttribute("hidden"), true,
          "The response content image box doesn't have the intended visibility.");

        is(tabpanel.querySelectorAll(".variables-view-scope").length, 1,
          "There should be 1 json scope displayed in this tabpanel.");
        is(tabpanel.querySelectorAll(".variables-view-property").length, 6057,
          "There should be 6057 json properties displayed in this tabpanel.");
        is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
          "The empty notice should not be displayed in this tabpanel.");

        let jsonScope = tabpanel.querySelectorAll(".variables-view-scope")[0];
        let names = ".variables-view-property .name";
        let values = ".variables-view-property .value";

        is(jsonScope.querySelector(".name").getAttribute("value"),
          L10N.getStr("jsonScopeName"),
          "The json scope doesn't have the correct title.");

        is(jsonScope.querySelectorAll(names)[0].getAttribute("value"),
          "0", "The first json property name was incorrect.");
        is(jsonScope.querySelectorAll(values)[0].getAttribute("value"),
          "[object Object]", "The first json property value was incorrect.");

        is(jsonScope.querySelectorAll(names)[1].getAttribute("value"),
          "greeting", "The second json property name was incorrect.");
        is(jsonScope.querySelectorAll(values)[1].getAttribute("value"),
          "\"Hello long string JSON!\"", "The second json property value was incorrect.");

        is(Array.slice(jsonScope.querySelectorAll(names), -1).shift().getAttribute("value"),
          "__proto__", "The last json property name was incorrect.");
        is(Array.slice(jsonScope.querySelectorAll(values), -1).shift().getAttribute("value"),
          "[object Object]", "The last json property value was incorrect.");
      }
    });

    aDebuggee.performRequests();
  });
}
