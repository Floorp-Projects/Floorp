/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the POST requests display the correct information in the UI,
 * for raw payloads with content-type headers attached to the upload stream.
 */

function test() {
  initNetMonitor(POST_RAW_WITH_HEADERS_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, L10N, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu, NetworkDetails } = NetMonitorView;
    let TAB_UPDATED = aMonitor.panelWin.EVENTS.TAB_UPDATED;
    RequestsMenu.lazyUpdate = false;

    Task.spawn(function*() {
      yield waitForNetworkEvents(aMonitor, 0, 1);

      NetMonitorView.toggleDetailsPane({ visible: true });
      RequestsMenu.selectedIndex = 0;

      yield waitFor(aMonitor.panelWin, TAB_UPDATED);

      let tab = document.querySelectorAll("#details-pane tab")[0];
      let tabpanel = document.querySelectorAll("#details-pane tabpanel")[0];
      let requestFromUploadScope = tabpanel.querySelectorAll(".variables-view-scope")[2];

      is(tab.getAttribute("selected"), "true",
        "The headers tab in the network details pane should be selected.");
      is(tabpanel.querySelectorAll(".variables-view-scope").length, 3,
        "There should be 3 header scopes displayed in this tabpanel.");

      is(requestFromUploadScope.querySelector(".name").getAttribute("value"),
        L10N.getStr("requestHeadersFromUpload") + " (" +
        L10N.getFormatStr("networkMenu.sizeKB", L10N.numberWithDecimals(74/1024, 3)) + ")",
        "The request headers from upload scope doesn't have the correct title.");

      is(requestFromUploadScope.querySelectorAll(".variables-view-variable").length, 2,
        "There should be 2 header values displayed in the request headers from upload scope.");

      is(requestFromUploadScope.querySelectorAll(".variables-view-variable .name")[0].getAttribute("value"),
        "content-type", "The first request header name was incorrect.");
      is(requestFromUploadScope.querySelectorAll(".variables-view-variable .value")[0].getAttribute("value"),
        "\"application/x-www-form-urlencoded\"", "The first request header value was incorrect.");
      is(requestFromUploadScope.querySelectorAll(".variables-view-variable .name")[1].getAttribute("value"),
        "custom-header", "The second request header name was incorrect.");
      is(requestFromUploadScope.querySelectorAll(".variables-view-variable .value")[1].getAttribute("value"),
        "\"hello world!\"", "The second request header value was incorrect.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[2]);

      yield waitFor(aMonitor.panelWin, TAB_UPDATED);

      let tab = document.querySelectorAll("#details-pane tab")[2];
      let tabpanel = document.querySelectorAll("#details-pane tabpanel")[2];
      let formDataScope = tabpanel.querySelectorAll(".variables-view-scope")[0];

      is(tab.getAttribute("selected"), "true",
        "The response tab in the network details pane should be selected.");
      is(tabpanel.querySelectorAll(".variables-view-scope").length, 1,
        "There should be 1 header scope displayed in this tabpanel.");

      is(formDataScope.querySelector(".name").getAttribute("value"),
        L10N.getStr("paramsFormData"),
        "The form data scope doesn't have the correct title.");

      is(formDataScope.querySelectorAll(".variables-view-variable").length, 2,
        "There should be 2 payload values displayed in the form data scope.");

      is(formDataScope.querySelectorAll(".variables-view-variable .name")[0].getAttribute("value"),
        "foo", "The first payload param name was incorrect.");
      is(formDataScope.querySelectorAll(".variables-view-variable .value")[0].getAttribute("value"),
        "\"bar\"", "The first payload param value was incorrect.");
      is(formDataScope.querySelectorAll(".variables-view-variable .name")[1].getAttribute("value"),
        "baz", "The second payload param name was incorrect.");
      is(formDataScope.querySelectorAll(".variables-view-variable .value")[1].getAttribute("value"),
        "\"123\"", "The second payload param value was incorrect.");

      teardown(aMonitor).then(finish);
    });

    aDebuggee.performRequests();
  });
}
