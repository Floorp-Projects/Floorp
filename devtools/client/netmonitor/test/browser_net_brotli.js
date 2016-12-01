/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BROTLI_URL = HTTPS_EXAMPLE_URL + "html_brotli-test-page.html";
const BROTLI_REQUESTS = 1;

/**
 * Test brotli encoded response is handled correctly on HTTPS.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/l10n");

  let { tab, monitor } = yield initNetMonitor(BROTLI_URL);
  info("Starting test... ");

  let { document, Editor, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, BROTLI_REQUESTS);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  verifyRequestItemTarget(RequestsMenu, RequestsMenu.getItemAtIndex(0),
    "GET", HTTPS_CONTENT_TYPE_SJS + "?fmt=br", {
      status: 200,
      statusText: "Connected",
      type: "plain",
      fullMimeType: "text/plain",
      transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 10),
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 64),
      time: true
    });

  let onEvent = waitForResponseBodyDisplayed();
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll("#details-pane tab")[3]);
  yield onEvent;
  yield testResponseTab("br");

  yield teardown(monitor);

  function* testResponseTab(type) {
    let tabEl = document.querySelectorAll("#details-pane tab")[3];
    let tabpanel = document.querySelectorAll("#details-pane tabpanel")[3];

    is(tabEl.getAttribute("selected"), "true",
      "The response tab in the network details pane should be selected.");

    function checkVisibility(box) {
      is(tabpanel.querySelector("#response-content-info-header")
        .hasAttribute("hidden"), true,
        "The response info header doesn't have the intended visibility.");
      is(tabpanel.querySelector("#response-content-json-box")
        .hasAttribute("hidden"), box != "json",
        "The response content json box doesn't have the intended visibility.");
      is(tabpanel.querySelector("#response-content-textarea-box")
        .hasAttribute("hidden"), box != "textarea",
        "The response content textarea box doesn't have the intended visibility.");
      is(tabpanel.querySelector("#response-content-image-box")
        .hasAttribute("hidden"), box != "image",
        "The response content image box doesn't have the intended visibility.");
    }

    switch (type) {
      case "br": {
        checkVisibility("textarea");

        let expected = "X".repeat(64);
        let editor = yield NetMonitorView.editor("#response-content-textarea");
        is(editor.getText(), expected,
          "The text shown in the source editor is incorrect for the brotli request.");
        is(editor.getMode(), Editor.modes.text,
          "The mode active in the source editor is incorrect for the brotli request.");
        break;
      }
    }
  }

  function waitForResponseBodyDisplayed() {
    return monitor.panelWin.once(monitor.panelWin.EVENTS.RESPONSE_BODY_DISPLAYED);
  }
});
