/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the POST requests display the correct information in the UI.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/l10n");

  // Set a higher panel height in order to get full CodeMirror content
  Services.prefs.setIntPref("devtools.toolbox.footer.height", 400);

  let { tab, monitor } = yield initNetMonitor(POST_DATA_URL);
  info("Starting test... ");

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 0, 2);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  verifyRequestItemTarget(RequestsMenu, RequestsMenu.getItemAtIndex(0),
    "POST", SIMPLE_SJS + "?foo=bar&baz=42&type=urlencoded", {
      status: 200,
      statusText: "Och Aye",
      type: "plain",
      fullMimeType: "text/plain; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 12),
      time: true
    });
  verifyRequestItemTarget(RequestsMenu, RequestsMenu.getItemAtIndex(1),
    "POST", SIMPLE_SJS + "?foo=bar&baz=42&type=multipart", {
      status: 200,
      statusText: "Och Aye",
      type: "plain",
      fullMimeType: "text/plain; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 12),
      time: true
    });

  // Wait for all tree sections updated by react
  wait = waitForDOM(document, "#panel-2 .tree-section", 2);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  document.querySelector("#tab-2 a").click();
  yield wait;
  yield testParamsTab("urlencoded");

  // Wait for all tree sections and editor updated by react
  let waitForSections = waitForDOM(document, "#panel-2 .tree-section", 2);
  let waitForEditor = waitForDOM(document, "#panel-2 .editor-mount iframe");
  RequestsMenu.selectedIndex = 1;
  let [, editorFrames] = yield Promise.all([waitForSections, waitForEditor]);
  yield once(editorFrames[0], "DOMContentLoaded");
  yield waitForDOM(editorFrames[0].contentDocument, ".CodeMirror-code");
  yield testParamsTab("multipart");

  return teardown(monitor);

  function* testParamsTab(type) {
    let tabpanel = document.querySelector("#panel-2");

    function checkVisibility(box) {
      is(!tabpanel.querySelector(".treeTable"), !box.includes("params"),
        "The request params doesn't have the indended visibility.");
      is(tabpanel.querySelector(".editor-mount") === null,
        !box.includes("editor"),
        "The request post data doesn't have the indended visibility.");
    }

    is(tabpanel.querySelectorAll(".tree-section").length, 2,
      "There should be 2 tree sections displayed in this tabpanel.");
    is(tabpanel.querySelectorAll(".empty-notice").length, 0,
      "The empty notice should not be displayed in this tabpanel.");

    let treeSections = tabpanel.querySelectorAll(".tree-section");

    is(treeSections[0].querySelector(".treeLabel").textContent,
      L10N.getStr("paramsQueryString"),
      "The query section doesn't have the correct title.");

    is(treeSections[1].querySelector(".treeLabel").textContent,
      L10N.getStr(type == "urlencoded" ? "paramsFormData" : "paramsPostPayload"),
      "The post section doesn't have the correct title.");

    let labels = tabpanel.querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
    let values = tabpanel.querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

    is(labels[0].textContent, "foo", "The first query param name was incorrect.");
    is(values[0].textContent, "\"bar\"", "The first query param value was incorrect.");
    is(labels[1].textContent, "baz", "The second query param name was incorrect.");
    is(values[1].textContent, "\"42\"", "The second query param value was incorrect.");
    is(labels[2].textContent, "type", "The third query param name was incorrect.");
    is(values[2].textContent, "\"" + type + "\"", "The third query param value was incorrect.");

    if (type == "urlencoded") {
      checkVisibility("params");
      is(labels.length, 5, "There should be 5 param values displayed in this tabpanel.");
      is(labels[3].textContent, "foo", "The first post param name was incorrect.");
      is(values[3].textContent, "\"bar\"", "The first post param value was incorrect.");
      is(labels[4].textContent, "baz", "The second post param name was incorrect.");
      is(values[4].textContent, "\"123\"", "The second post param value was incorrect.");
    } else {
      checkVisibility("params editor");

      is(labels.length, 3, "There should be 3 param values displayed in this tabpanel.");

      let text = editorFrames[0].contentDocument.querySelector(".CodeMirror-code").textContent;

      ok(text.includes("Content-Disposition: form-data; name=\"text\""),
        "The text shown in the source editor is incorrect (1.1).");
      ok(text.includes("Content-Disposition: form-data; name=\"email\""),
        "The text shown in the source editor is incorrect (2.1).");
      ok(text.includes("Content-Disposition: form-data; name=\"range\""),
        "The text shown in the source editor is incorrect (3.1).");
      ok(text.includes("Content-Disposition: form-data; name=\"Custom field\""),
        "The text shown in the source editor is incorrect (4.1).");
      ok(text.includes("Some text..."),
        "The text shown in the source editor is incorrect (2.2).");
      ok(text.includes("42"),
        "The text shown in the source editor is incorrect (3.2).");
      ok(text.includes("Extra data"),
        "The text shown in the source editor is incorrect (4.2).");
    }
  }
});
