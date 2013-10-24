/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if requests render correct information in the details UI.
 */

function test() {
  initNetMonitor(SIMPLE_SJS).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, L10N, Editor, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu, NetworkDetails } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 1).then(() => {
      is(RequestsMenu.selectedItem, null,
        "There shouldn't be any selected item in the requests menu.");
      is(RequestsMenu.itemCount, 1,
        "The requests menu should not be empty after the first request.");
      is(NetMonitorView.detailsPaneHidden, true,
        "The details pane should still be hidden after the first request.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.getElementById("details-pane-toggle"));

      isnot(RequestsMenu.selectedItem, null,
        "There should be a selected item in the requests menu.");
      is(RequestsMenu.selectedIndex, 0,
        "The first item should be selected in the requests menu.");
      is(NetMonitorView.detailsPaneHidden, false,
        "The details pane should not be hidden after toggle button was pressed.");

      testHeadersTab();
      testCookiesTab();
      testParamsTab();
      testResponseTab()
        .then(() => {
          testTimingsTab();
          return teardown(aMonitor);
        })
        .then(finish);
    });

    function testHeadersTab() {
      let tab = document.querySelectorAll("#details-pane tab")[0];
      let tabpanel = document.querySelectorAll("#details-pane tabpanel")[0];

      is(tab.getAttribute("selected"), "true",
        "The headers tab in the network details pane should be selected.");

      is(tabpanel.querySelector("#headers-summary-url-value").getAttribute("value"),
        SIMPLE_SJS, "The url summary value is incorrect.");
      is(tabpanel.querySelector("#headers-summary-url-value").getAttribute("tooltiptext"),
        SIMPLE_SJS, "The url summary tooltiptext is incorrect.");
      is(tabpanel.querySelector("#headers-summary-method-value").getAttribute("value"),
        "GET", "The method summary value is incorrect.");
      is(tabpanel.querySelector("#headers-summary-status-circle").getAttribute("code"),
        "200", "The status summary code is incorrect.");
      is(tabpanel.querySelector("#headers-summary-status-value").getAttribute("value"),
        "200 Och Aye", "The status summary value is incorrect.");

      is(tabpanel.querySelectorAll(".variables-view-scope").length, 2,
        "There should be 2 header scopes displayed in this tabpanel.");
      ok(tabpanel.querySelectorAll(".variable-or-property").length >= 12,
        "There should be at least 12 header values displayed in this tabpanel.");
      // Can't test for an exact total number of headers, because it seems to
      // vary across pgo/non-pgo builds.

      is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
        "The empty notice should not be displayed in this tabpanel.");

      let responseScope = tabpanel.querySelectorAll(".variables-view-scope")[0];
      let requestScope = tabpanel.querySelectorAll(".variables-view-scope")[1];

      is(responseScope.querySelector(".name").getAttribute("value"),
        L10N.getStr("responseHeaders") + " (" +
        L10N.getFormatStr("networkMenu.sizeKB", L10N.numberWithDecimals(173/1024, 3)) + ")",
        "The response headers scope doesn't have the correct title.");

      ok(requestScope.querySelector(".name").getAttribute("value").contains(
        L10N.getStr("requestHeaders") + " (0"),
        "The request headers scope doesn't have the correct title.");
      // Can't test for full request headers title because the size may
      // vary across platforms ("User-Agent" header differs). We're pretty
      // sure it's smaller than 1 MB though, so it starts with a 0.

      is(responseScope.querySelectorAll(".variables-view-variable .name")[0].getAttribute("value"),
        "Connection", "The first response header name was incorrect.");
      is(responseScope.querySelectorAll(".variables-view-variable .value")[0].getAttribute("value"),
        "\"close\"", "The first response header value was incorrect.");
      is(responseScope.querySelectorAll(".variables-view-variable .name")[1].getAttribute("value"),
        "Content-Length", "The second response header name was incorrect.");
      is(responseScope.querySelectorAll(".variables-view-variable .value")[1].getAttribute("value"),
        "\"12\"", "The second response header value was incorrect.");
      is(responseScope.querySelectorAll(".variables-view-variable .name")[2].getAttribute("value"),
        "Content-Type", "The third response header name was incorrect.");
      is(responseScope.querySelectorAll(".variables-view-variable .value")[2].getAttribute("value"),
        "\"text/plain; charset=utf-8\"", "The third response header value was incorrect.");
      is(responseScope.querySelectorAll(".variables-view-variable .name")[5].getAttribute("value"),
        "foo-bar", "The last response header name was incorrect.");
      is(responseScope.querySelectorAll(".variables-view-variable .value")[5].getAttribute("value"),
        "\"baz\"", "The last response header value was incorrect.");

      is(requestScope.querySelectorAll(".variables-view-variable .name")[0].getAttribute("value"),
        "Host", "The first request header name was incorrect.");
      is(requestScope.querySelectorAll(".variables-view-variable .value")[0].getAttribute("value"),
        "\"example.com\"", "The first request header value was incorrect.");
      is(requestScope.querySelectorAll(".variables-view-variable .name")[5].getAttribute("value"),
        "Connection", "The penultimate request header name was incorrect.");
      is(requestScope.querySelectorAll(".variables-view-variable .value")[5].getAttribute("value"),
        "\"keep-alive\"", "The penultimate request header value was incorrect.");

      let lastReqHeaderName = requestScope.querySelectorAll(".variables-view-variable .name")[6];
      let lastReqHeaderValue = requestScope.querySelectorAll(".variables-view-variable .value")[6];
      if (lastReqHeaderName && lastReqHeaderValue) {
        is(lastReqHeaderName.getAttribute("value"),
          "Cache-Control", "The last request header name was incorrect.");
        is(lastReqHeaderValue.getAttribute("value"),
          "\"max-age=0\"", "The last request header value was incorrect.");
      } else {
        info("The number of request headers was 6 instead of 7. Technically, " +
             "not a failure in this particular test, but needs investigation.");
      }
    }

    function testCookiesTab() {
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[1]);

      let tab = document.querySelectorAll("#details-pane tab")[1];
      let tabpanel = document.querySelectorAll("#details-pane tabpanel")[1];

      is(tab.getAttribute("selected"), "true",
        "The cookies tab in the network details pane should be selected.");

      is(tabpanel.querySelectorAll(".variables-view-scope").length, 0,
        "There should be no cookie scopes displayed in this tabpanel.");
      is(tabpanel.querySelectorAll(".variable-or-property").length, 0,
        "There should be no cookie values displayed in this tabpanel.");
      is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 1,
        "The empty notice should be displayed in this tabpanel.");
    }

    function testParamsTab() {
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[2]);

      let tab = document.querySelectorAll("#details-pane tab")[2];
      let tabpanel = document.querySelectorAll("#details-pane tabpanel")[2];

      is(tab.getAttribute("selected"), "true",
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

    function testResponseTab() {
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[3]);

      let tab = document.querySelectorAll("#details-pane tab")[3];
      let tabpanel = document.querySelectorAll("#details-pane tabpanel")[3];

      is(tab.getAttribute("selected"), "true",
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

      return NetMonitorView.editor("#response-content-textarea").then((aEditor) => {
        is(aEditor.getText(), "Hello world!",
          "The text shown in the source editor is incorrect.");
        is(aEditor.getMode(), Editor.modes.text,
          "The mode active in the source editor is incorrect.");
      });
    }

    function testTimingsTab() {
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[4]);

      let tab = document.querySelectorAll("#details-pane tab")[4];
      let tabpanel = document.querySelectorAll("#details-pane tabpanel")[4];

      is(tab.getAttribute("selected"), "true",
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

    aDebuggee.location.reload();
  });
}
