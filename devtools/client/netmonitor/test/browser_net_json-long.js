/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if very long JSON responses are handled correctly.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/l10n");

  let { tab, monitor } = yield initNetMonitor(JSON_LONG_URL);
  info("Starting test... ");

  // This is receiving over 80 KB of json and will populate over 6000 items
  // in a variables view instance. Debug builds are slow.
  requestLongerTimeout(4);

  let { document, EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  verifyRequestItemTarget(RequestsMenu, RequestsMenu.getItemAtIndex(0),
    "GET", CONTENT_TYPE_SJS + "?fmt=json-long", {
      status: 200,
      statusText: "OK",
      type: "json",
      fullMimeType: "text/json; charset=utf-8",
      size: L10N.getFormatStr("networkMenu.sizeKB",
        L10N.numberWithDecimals(85975 / 1024, 2)),
      time: true
    });

  let onEvent = monitor.panelWin.once(EVENTS.RESPONSE_BODY_DISPLAYED);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll("#details-pane tab")[3]);
  yield onEvent;

  testResponseTab();

  yield teardown(monitor);

  function testResponseTab() {
    let tabEl = document.querySelectorAll("#details-pane tab")[3];
    let tabpanel = document.querySelectorAll("#details-pane tabpanel")[3];

    is(tabEl.getAttribute("selected"), "true",
      "The response tab in the network details pane should be selected.");

    is(tabpanel.querySelector("#response-content-info-header")
      .hasAttribute("hidden"), true,
      "The response info header doesn't have the intended visibility.");
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
    is(tabpanel.querySelectorAll(".variables-view-property").length, 6143,
      "There should be 6143 json properties displayed in this tabpanel.");
    is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
      "The empty notice should not be displayed in this tabpanel.");

    let jsonScope = tabpanel.querySelectorAll(".variables-view-scope")[0];
    let names = ".variables-view-property > .title > .name";
    let values = ".variables-view-property > .title > .value";

    is(jsonScope.querySelector(".name").getAttribute("value"),
      L10N.getStr("jsonScopeName"),
      "The json scope doesn't have the correct title.");

    is(jsonScope.querySelectorAll(names)[0].getAttribute("value"),
      "0", "The first json property name was incorrect.");
    is(jsonScope.querySelectorAll(values)[0].getAttribute("value"),
      "Object", "The first json property value was incorrect.");

    is(jsonScope.querySelectorAll(names)[1].getAttribute("value"),
      "greeting", "The second json property name was incorrect.");
    is(jsonScope.querySelectorAll(values)[1].getAttribute("value"),
      "\"Hello long string JSON!\"", "The second json property value was incorrect.");
  }
});
