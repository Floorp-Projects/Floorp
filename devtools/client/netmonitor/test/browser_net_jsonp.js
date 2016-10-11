/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if JSONP responses are handled correctly.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/l10n");

  let { tab, monitor } = yield initNetMonitor(JSONP_URL);
  info("Starting test... ");

  let { document, EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;
  NetworkDetails._json.lazyEmpty = false;

  let wait = waitForNetworkEvents(monitor, 2);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  verifyRequestItemTarget(RequestsMenu, RequestsMenu.getItemAtIndex(0),
    "GET", CONTENT_TYPE_SJS + "?fmt=jsonp&jsonp=$_0123Fun", {
      status: 200,
      statusText: "OK",
      type: "json",
      fullMimeType: "text/json; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 41),
      time: true
    });
  verifyRequestItemTarget(RequestsMenu, RequestsMenu.getItemAtIndex(1),
    "GET", CONTENT_TYPE_SJS + "?fmt=jsonp2&jsonp=$_4567Sad", {
      status: 200,
      statusText: "OK",
      type: "json",
      fullMimeType: "text/json; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 54),
      time: true
    });

  let onEvent = monitor.panelWin.once(EVENTS.RESPONSE_BODY_DISPLAYED);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll("#details-pane tab")[3]);
  yield onEvent;

  testResponseTab("$_0123Fun", "\"Hello JSONP!\"");

  onEvent = monitor.panelWin.once(EVENTS.RESPONSE_BODY_DISPLAYED);
  RequestsMenu.selectedIndex = 1;
  yield onEvent;

  testResponseTab("$_4567Sad", "\"Hello weird JSONP!\"");

  yield teardown(monitor);

  function testResponseTab(func, greeting) {
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
    is(tabpanel.querySelectorAll(".variables-view-property").length, 2,
      "There should be 2 json properties displayed in this tabpanel.");
    is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
      "The empty notice should not be displayed in this tabpanel.");

    let jsonScope = tabpanel.querySelectorAll(".variables-view-scope")[0];

    is(jsonScope.querySelector(".name").getAttribute("value"),
      L10N.getFormatStr("jsonpScopeName", func),
      "The json scope doesn't have the correct title.");

    is(jsonScope.querySelectorAll(".variables-view-property .name")[0]
      .getAttribute("value"),
      "greeting", "The first json property name was incorrect.");
    is(jsonScope.querySelectorAll(".variables-view-property .value")[0]
      .getAttribute("value"),
      greeting, "The first json property value was incorrect.");

    is(jsonScope.querySelectorAll(".variables-view-property .name")[1]
      .getAttribute("value"),
      "__proto__", "The second json property name was incorrect.");
    is(jsonScope.querySelectorAll(".variables-view-property .value")[1]
      .getAttribute("value"),
      "Object", "The second json property value was incorrect.");
  }
});
