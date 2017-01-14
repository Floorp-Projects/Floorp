/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if very large response contents are just displayed as plain text.
 */

const HTML_LONG_URL = CONTENT_TYPE_SJS + "?fmt=html-long";

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  info("Starting test... ");

  // This test could potentially be slow because over 100 KB of stuff
  // is going to be requested and displayed in the source editor.
  requestLongerTimeout(2);

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, HTML_LONG_URL, function* (url) {
    content.wrappedJSObject.performRequests(1, url);
  });
  yield wait;

  verifyRequestItemTarget(RequestsMenu, RequestsMenu.getItemAtIndex(0),
    "GET", CONTENT_TYPE_SJS + "?fmt=html-long", {
      status: 200,
      statusText: "OK"
    });

  let waitDOM = waitForDOM(document, "#panel-3 .editor-mount iframe");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  document.querySelector("#tab-3 a").click();
  let [editor] = yield waitDOM;
  yield once(editor, "DOMContentLoaded");
  yield waitForDOM(editor.contentDocument, ".CodeMirror-code");

  let text = editor.contentDocument
        .querySelector(".CodeMirror-line").textContent;

  ok(text.match(/^<p>/), "The text shown in the source editor is incorrect.");

  yield teardown(monitor);

  // This test uses a lot of memory, so force a GC to help fragmentation.
  info("Forcing GC after netmonitor test.");
  Cu.forceGC();
});
