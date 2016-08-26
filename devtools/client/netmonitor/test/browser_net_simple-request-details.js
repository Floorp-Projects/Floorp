/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if requests render correct information in the details UI.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SIMPLE_SJS);
  info("Starting test... ");

  let { document, EVENTS, L10N, Editor, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  yield wait;

  is(RequestsMenu.selectedItem, null,
    "There shouldn't be any selected item in the requests menu.");
  is(RequestsMenu.itemCount, 1,
    "The requests menu should not be empty after the first request.");
  is(NetMonitorView.detailsPaneHidden, true,
    "The details pane should still be hidden after the first request.");

  let onTabUpdated = monitor.panelWin.once(EVENTS.TAB_UPDATED);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  yield onTabUpdated;

  isnot(RequestsMenu.selectedItem, null,
    "There should be a selected item in the requests menu.");
  is(RequestsMenu.selectedIndex, 0,
    "The first item should be selected in the requests menu.");
  is(NetMonitorView.detailsPaneHidden, false,
    "The details pane should not be hidden after toggle button was pressed.");

  testHeadersTab();
  yield testCookiesTab();
  testParamsTab();
  yield testResponseTab();
  testTimingsTab();
  return teardown(monitor);

  function testHeadersTab() {
    let tabEl = document.querySelectorAll("#details-pane tab")[0];
    let tabpanel = document.querySelectorAll("#details-pane tabpanel")[0];

    is(tabEl.getAttribute("selected"), "true",
      "The headers tab in the network details pane should be selected.");

    is(tabpanel.querySelector("#headers-summary-url-value").getAttribute("value"),
      SIMPLE_SJS, "The url summary value is incorrect.");
    is(tabpanel.querySelector("#headers-summary-url-value").getAttribute("tooltiptext"),
      SIMPLE_SJS, "The url summary tooltiptext is incorrect.");
    is(tabpanel.querySelector("#headers-summary-method-value").getAttribute("value"),
      "GET", "The method summary value is incorrect.");
    is(tabpanel.querySelector("#headers-summary-address-value").getAttribute("value"),
      "127.0.0.1:8888", "The remote address summary value is incorrect.");
    is(tabpanel.querySelector("#headers-summary-status-circle").getAttribute("code"),
      "200", "The status summary code is incorrect.");
    is(tabpanel.querySelector("#headers-summary-status-value").getAttribute("value"),
      "200 Och Aye", "The status summary value is incorrect.");

    is(tabpanel.querySelectorAll(".variables-view-scope").length, 2,
      "There should be 2 header scopes displayed in this tabpanel.");
    is(tabpanel.querySelectorAll(".variable-or-property").length, 19,
      "There should be 19 header values displayed in this tabpanel.");

    is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
      "The empty notice should not be displayed in this tabpanel.");

    let responseScope = tabpanel.querySelectorAll(".variables-view-scope")[0];
    let requestScope = tabpanel.querySelectorAll(".variables-view-scope")[1];

    is(responseScope.querySelector(".name").getAttribute("value"),
      L10N.getStr("responseHeaders") + " (" +
      L10N.getFormatStr("networkMenu.sizeKB",
        L10N.numberWithDecimals(330 / 1024, 3)) + ")",
      "The response headers scope doesn't have the correct title.");

    ok(requestScope.querySelector(".name").getAttribute("value").includes(
      L10N.getStr("requestHeaders") + " (0"),
      "The request headers scope doesn't have the correct title.");
    // Can't test for full request headers title because the size may
    // vary across platforms ("User-Agent" header differs). We're pretty
    // sure it's smaller than 1 MB though, so it starts with a 0.

    is(responseScope.querySelectorAll(".variables-view-variable .name")[0]
      .getAttribute("value"),
      "Cache-Control", "The first response header name was incorrect.");
    is(responseScope.querySelectorAll(".variables-view-variable .value")[0]
      .getAttribute("value"),
      "\"no-cache, no-store, must-revalidate\"",
      "The first response header value was incorrect.");
    is(responseScope.querySelectorAll(".variables-view-variable .name")[1]
      .getAttribute("value"),
      "Connection", "The second response header name was incorrect.");
    is(responseScope.querySelectorAll(".variables-view-variable .value")[1]
      .getAttribute("value"),
      "\"close\"", "The second response header value was incorrect.");
    is(responseScope.querySelectorAll(".variables-view-variable .name")[2]
      .getAttribute("value"),
      "Content-Length", "The third response header name was incorrect.");
    is(responseScope.querySelectorAll(".variables-view-variable .value")[2]
      .getAttribute("value"),
      "\"12\"", "The third response header value was incorrect.");
    is(responseScope.querySelectorAll(".variables-view-variable .name")[3]
      .getAttribute("value"),
      "Content-Type", "The fourth response header name was incorrect.");
    is(responseScope.querySelectorAll(".variables-view-variable .value")[3]
      .getAttribute("value"),
      "\"text/plain; charset=utf-8\"", "The fourth response header value was incorrect.");
    is(responseScope.querySelectorAll(".variables-view-variable .name")[9]
      .getAttribute("value"),
      "foo-bar", "The last response header name was incorrect.");
    is(responseScope.querySelectorAll(".variables-view-variable .value")[9]
      .getAttribute("value"),
      "\"baz\"", "The last response header value was incorrect.");

    is(requestScope.querySelectorAll(".variables-view-variable .name")[0]
      .getAttribute("value"),
      "Host", "The first request header name was incorrect.");
    is(requestScope.querySelectorAll(".variables-view-variable .value")[0]
      .getAttribute("value"),
      "\"example.com\"", "The first request header value was incorrect.");
    is(requestScope.querySelectorAll(".variables-view-variable .name")[6]
      .getAttribute("value"),
      "Connection", "The ante-penultimate request header name was incorrect.");
    is(requestScope.querySelectorAll(".variables-view-variable .value")[6]
      .getAttribute("value"),
      "\"keep-alive\"", "The ante-penultimate request header value was incorrect.");
    is(requestScope.querySelectorAll(".variables-view-variable .name")[7]
      .getAttribute("value"),
      "Pragma", "The penultimate request header name was incorrect.");
    is(requestScope.querySelectorAll(".variables-view-variable .value")[7]
      .getAttribute("value"),
      "\"no-cache\"", "The penultimate request header value was incorrect.");
    is(requestScope.querySelectorAll(".variables-view-variable .name")[8]
      .getAttribute("value"),
      "Cache-Control", "The last request header name was incorrect.");
    is(requestScope.querySelectorAll(".variables-view-variable .value")[8]
      .getAttribute("value"),
      "\"no-cache\"", "The last request header value was incorrect.");
  }

  function* testCookiesTab() {
    let onEvent = monitor.panelWin.once(EVENTS.TAB_UPDATED);
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll("#details-pane tab")[1]);
    yield onEvent;

    let tabEl = document.querySelectorAll("#details-pane tab")[1];
    let tabpanel = document.querySelectorAll("#details-pane tabpanel")[1];

    is(tabEl.getAttribute("selected"), "true",
      "The cookies tab in the network details pane should be selected.");

    is(tabpanel.querySelectorAll(".variables-view-scope").length, 2,
      "There should be 2 cookie scopes displayed in this tabpanel.");
    is(tabpanel.querySelectorAll(".variable-or-property").length, 6,
      "There should be 6 cookie values displayed in this tabpanel.");
  }

  function testParamsTab() {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll("#details-pane tab")[2]);

    let tabEl = document.querySelectorAll("#details-pane tab")[2];
    let tabpanel = document.querySelectorAll("#details-pane tabpanel")[2];

    is(tabEl.getAttribute("selected"), "true",
      "The params tab in the network details pane should be selected.");

    is(tabpanel.querySelectorAll(".variables-view-scope").length, 0,
      "There should be no param scopes displayed in this tabpanel.");
    is(tabpanel.querySelectorAll(".variable-or-property").length, 0,
      "There should be no param values displayed in this tabpanel.");
    is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 1,
      "The empty notice should be displayed in this tabpanel.");

    is(tabpanel.querySelector("#request-params-box")
      .hasAttribute("hidden"), false,
      "The request params box should not be hidden.");
    is(tabpanel.querySelector("#request-post-data-textarea-box")
      .hasAttribute("hidden"), true,
      "The request post data textarea box should be hidden.");
  }

  function* testResponseTab() {
    let onEvent = monitor.panelWin.once(EVENTS.TAB_UPDATED);
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll("#details-pane tab")[3]);
    yield onEvent;

    let tabEl = document.querySelectorAll("#details-pane tab")[3];
    let tabpanel = document.querySelectorAll("#details-pane tabpanel")[3];

    is(tabEl.getAttribute("selected"), "true",
      "The response tab in the network details pane should be selected.");

    is(tabpanel.querySelector("#response-content-info-header")
      .hasAttribute("hidden"), true,
      "The response info header should be hidden.");
    is(tabpanel.querySelector("#response-content-json-box")
      .hasAttribute("hidden"), true,
      "The response content json box should be hidden.");
    is(tabpanel.querySelector("#response-content-textarea-box")
      .hasAttribute("hidden"), false,
      "The response content textarea box should not be hidden.");
    is(tabpanel.querySelector("#response-content-image-box")
      .hasAttribute("hidden"), true,
      "The response content image box should be hidden.");

    let editor = yield NetMonitorView.editor("#response-content-textarea");
    is(editor.getText(), "Hello world!",
      "The text shown in the source editor is incorrect.");
    is(editor.getMode(), Editor.modes.text,
      "The mode active in the source editor is incorrect.");
  }

  function testTimingsTab() {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll("#details-pane tab")[4]);

    let tabEl = document.querySelectorAll("#details-pane tab")[4];
    let tabpanel = document.querySelectorAll("#details-pane tabpanel")[4];

    is(tabEl.getAttribute("selected"), "true",
      "The timings tab in the network details pane should be selected.");

    ok(tabpanel.querySelector("#timings-summary-blocked .requests-menu-timings-total")
      .getAttribute("value").match(/[0-9]+/),
      "The blocked timing info does not appear to be correct.");

    ok(tabpanel.querySelector("#timings-summary-dns .requests-menu-timings-total")
      .getAttribute("value").match(/[0-9]+/),
      "The dns timing info does not appear to be correct.");

    ok(tabpanel.querySelector("#timings-summary-connect .requests-menu-timings-total")
      .getAttribute("value").match(/[0-9]+/),
      "The connect timing info does not appear to be correct.");

    ok(tabpanel.querySelector("#timings-summary-send .requests-menu-timings-total")
      .getAttribute("value").match(/[0-9]+/),
      "The send timing info does not appear to be correct.");

    ok(tabpanel.querySelector("#timings-summary-wait .requests-menu-timings-total")
      .getAttribute("value").match(/[0-9]+/),
      "The wait timing info does not appear to be correct.");

    ok(tabpanel.querySelector("#timings-summary-receive .requests-menu-timings-total")
      .getAttribute("value").match(/[0-9]+/),
      "The receive timing info does not appear to be correct.");
  }
});
