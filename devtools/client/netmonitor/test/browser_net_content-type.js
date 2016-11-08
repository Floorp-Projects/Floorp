/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if different response content types are handled correctly.
 */

function* content_type_test(isHTTPS) {
  let { L10N } = require("devtools/client/netmonitor/l10n");

  let pageURL = isHTTPS ? HTTPS_CONTENT_TYPE_WITHOUT_CACHE_URL
                        : CONTENT_TYPE_WITHOUT_CACHE_URL;
  let imageURL = isHTTPS ? HTTPS_TEST_IMAGE
                         : TEST_IMAGE;
  let sjsURL = isHTTPS ? HTTPS_CONTENT_TYPE_SJS
                       : CONTENT_TYPE_SJS;
  let { tab, monitor } = yield initNetMonitor(pageURL);
  info("Starting test... ");

  let { document, Editor, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 8);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  let okStatus = isHTTPS ? "Connected" : "OK";

  verifyRequestItemTarget(RequestsMenu.getItemAtIndex(0),
    "GET", sjsURL + "?fmt=xml", {
      status: 200,
      statusText: okStatus,
      type: "xml",
      fullMimeType: "text/xml; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 42),
      time: true
    });
  verifyRequestItemTarget(RequestsMenu.getItemAtIndex(1),
    "GET", sjsURL + "?fmt=css", {
      status: 200,
      statusText: okStatus,
      type: "css",
      fullMimeType: "text/css; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 34),
      time: true
    });
  verifyRequestItemTarget(RequestsMenu.getItemAtIndex(2),
    "GET", sjsURL + "?fmt=js", {
      status: 200,
      statusText: okStatus,
      type: "js",
      fullMimeType: "application/javascript; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 34),
      time: true
    });
  verifyRequestItemTarget(RequestsMenu.getItemAtIndex(3),
    "GET", sjsURL + "?fmt=json", {
      status: 200,
      statusText: okStatus,
      type: "json",
      fullMimeType: "application/json; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 29),
      time: true
    });
  if (!isHTTPS) {
    // 404 doesn't work on HTTPS test harness.
    verifyRequestItemTarget(RequestsMenu.getItemAtIndex(4),
      "GET", sjsURL + "?fmt=bogus", {
        status: 404,
        statusText: "Not Found",
        type: "html",
        fullMimeType: "text/html; charset=utf-8",
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 24),
        time: true
      });
  }
  verifyRequestItemTarget(RequestsMenu.getItemAtIndex(5),
    "GET", imageURL, {
      fuzzyUrl: true,
      status: 200,
      statusText: okStatus,
      type: "png",
      fullMimeType: "image/png",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 580),
      time: true
    });
  verifyRequestItemTarget(RequestsMenu.getItemAtIndex(6),
    "GET", sjsURL + "?fmt=gzip", {
      status: 200,
      statusText: okStatus,
      type: "plain",
      fullMimeType: "text/plain",
      transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 73),
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 10.73),
      time: true
    });
  if (isHTTPS) {
    // Brotli is enabled only on HTTPS.
    verifyRequestItemTarget(RequestsMenu.getItemAtIndex(6),
      "GET", sjsURL + "?fmt=gzip", {
        status: 200,
        statusText: okStatus,
        type: "plain",
        fullMimeType: "text/plain",
        transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 73),
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 10.73),
        time: true
      });
  }

  let onEvent = waitForResponseBodyDisplayed();
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll("#details-pane tab")[3]);
  yield onEvent;
  yield testResponseTab("xml");

  yield selectIndexAndWaitForTabUpdated(1);
  yield testResponseTab("css");

  yield selectIndexAndWaitForTabUpdated(2);
  yield testResponseTab("js");

  yield selectIndexAndWaitForTabUpdated(3);
  yield testResponseTab("json");

  yield selectIndexAndWaitForTabUpdated(4);
  yield testResponseTab("html");

  yield selectIndexAndWaitForTabUpdated(5);
  yield testResponseTab("png");

  yield selectIndexAndWaitForTabUpdated(6);
  yield testResponseTab("gzip");

  if (isHTTPS) {
    yield selectIndexAndWaitForTabUpdated(7);
    yield testResponseTab("br");
  }

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
      case "xml": {
        checkVisibility("textarea");

        let editor = yield NetMonitorView.editor("#response-content-textarea");
        is(editor.getText(), "<label value='greeting'>Hello XML!</label>",
          "The text shown in the source editor is incorrect for the xml request.");
        is(editor.getMode(), Editor.modes.html,
          "The mode active in the source editor is incorrect for the xml request.");
        break;
      }
      case "css": {
        checkVisibility("textarea");

        let editor = yield NetMonitorView.editor("#response-content-textarea");
        is(editor.getText(), "body:pre { content: 'Hello CSS!' }",
          "The text shown in the source editor is incorrect for the xml request.");
        is(editor.getMode(), Editor.modes.css,
          "The mode active in the source editor is incorrect for the xml request.");
        break;
      }
      case "js": {
        checkVisibility("textarea");

        let editor = yield NetMonitorView.editor("#response-content-textarea");
        is(editor.getText(), "function() { return 'Hello JS!'; }",
          "The text shown in the source editor is incorrect for the xml request.");
        is(editor.getMode(), Editor.modes.js,
          "The mode active in the source editor is incorrect for the xml request.");
        break;
      }
      case "json": {
        checkVisibility("json");

        is(tabpanel.querySelectorAll(".variables-view-scope").length, 1,
          "There should be 1 json scope displayed in this tabpanel.");
        is(tabpanel.querySelectorAll(".variables-view-property").length, 2,
          "There should be 2 json properties displayed in this tabpanel.");
        is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
          "The empty notice should not be displayed in this tabpanel.");

        let jsonScope = tabpanel.querySelectorAll(".variables-view-scope")[0];

        is(jsonScope.querySelector(".name").getAttribute("value"),
          L10N.getStr("jsonScopeName"),
          "The json scope doesn't have the correct title.");

        is(jsonScope.querySelectorAll(".variables-view-property .name")[0]
          .getAttribute("value"),
          "greeting", "The first json property name was incorrect.");
        is(jsonScope.querySelectorAll(".variables-view-property .value")[0]
          .getAttribute("value"),
          "\"Hello JSON!\"", "The first json property value was incorrect.");

        is(jsonScope.querySelectorAll(".variables-view-property .name")[1]
          .getAttribute("value"),
          "__proto__", "The second json property name was incorrect.");
        is(jsonScope.querySelectorAll(".variables-view-property .value")[1]
          .getAttribute("value"),
          "Object", "The second json property value was incorrect.");
        break;
      }
      case "html": {
        checkVisibility("textarea");

        let editor = yield NetMonitorView.editor("#response-content-textarea");
        is(editor.getText(), "<blink>Not Found</blink>",
          "The text shown in the source editor is incorrect for the xml request.");
        is(editor.getMode(), Editor.modes.html,
          "The mode active in the source editor is incorrect for the xml request.");
        break;
      }
      case "png": {
        checkVisibility("image");

        let imageNode = tabpanel.querySelector("#response-content-image");
        yield once(imageNode, "load");

        is(tabpanel.querySelector("#response-content-image-name-value")
          .getAttribute("value"), "test-image.png",
          "The image name info isn't correct.");
        is(tabpanel.querySelector("#response-content-image-mime-value")
          .getAttribute("value"), "image/png",
          "The image mime info isn't correct.");
        is(tabpanel.querySelector("#response-content-image-dimensions-value")
          .getAttribute("value"), "16" + " \u00D7 " + "16",
          "The image dimensions info isn't correct.");
        break;
      }
      case "gzip": {
        checkVisibility("textarea");

        let expected = new Array(1000).join("Hello gzip!");
        let editor = yield NetMonitorView.editor("#response-content-textarea");
        is(editor.getText(), expected,
          "The text shown in the source editor is incorrect for the gzip request.");
        is(editor.getMode(), Editor.modes.text,
          "The mode active in the source editor is incorrect for the gzip request.");
        break;
      }
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

  function selectIndexAndWaitForTabUpdated(index) {
    let onTabUpdated = monitor.panelWin.once(monitor.panelWin.EVENTS.TAB_UPDATED);
    RequestsMenu.selectedIndex = index;
    return onTabUpdated;
  }

  function waitForResponseBodyDisplayed() {
    return monitor.panelWin.once(monitor.panelWin.EVENTS.RESPONSE_BODY_DISPLAYED);
  }
}

add_task(function* () {
    yield* content_type_test(false);
});

add_task(function* () {
    yield* content_type_test(true);
});
