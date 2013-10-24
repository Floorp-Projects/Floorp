/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if malformed JSON responses are handled correctly.
 */

function test() {
  initNetMonitor(JSON_MALFORMED_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, Editor, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 1).then(() => {
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(0),
        "GET", CONTENT_TYPE_SJS + "?fmt=json-malformed", {
          status: 200,
          statusText: "OK",
          type: "json",
          fullMimeType: "text/json; charset=utf-8"
        });

      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.getElementById("details-pane-toggle"));
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[3]);

      let tab = document.querySelectorAll("#details-pane tab")[3];
      let tabpanel = document.querySelectorAll("#details-pane tabpanel")[3];

      is(tab.getAttribute("selected"), "true",
        "The response tab in the network details pane should be selected.");

      is(tabpanel.querySelector("#response-content-info-header")
        .hasAttribute("hidden"), false,
        "The response info header doesn't have the intended visibility.");
      is(tabpanel.querySelector("#response-content-info-header")
        .getAttribute("value"),
        "SyntaxError: JSON.parse: unexpected non-whitespace character after JSON data",
        "The response info header doesn't have the intended value attribute.");
      is(tabpanel.querySelector("#response-content-info-header")
        .getAttribute("tooltiptext"),
        "SyntaxError: JSON.parse: unexpected non-whitespace character after JSON data",
        "The response info header doesn't have the intended tooltiptext attribute.");

      is(tabpanel.querySelector("#response-content-json-box")
        .hasAttribute("hidden"), true,
        "The response content json box doesn't have the intended visibility.");
      is(tabpanel.querySelector("#response-content-textarea-box")
        .hasAttribute("hidden"), false,
        "The response content textarea box doesn't have the intended visibility.");
      is(tabpanel.querySelector("#response-content-image-box")
        .hasAttribute("hidden"), true,
        "The response content image box doesn't have the intended visibility.");

      NetMonitorView.editor("#response-content-textarea").then((aEditor) => {
        is(aEditor.getText(), "{ \"greeting\": \"Hello malformed JSON!\" },",
          "The text shown in the source editor is incorrect.");
        is(aEditor.getMode(), Editor.modes.js,
          "The mode active in the source editor is incorrect.");

        teardown(aMonitor).then(finish);
      });
    });

    aDebuggee.performRequests();
  });
}
