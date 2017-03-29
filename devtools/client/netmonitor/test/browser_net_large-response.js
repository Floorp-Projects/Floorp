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

  let { document, gStore, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  gStore.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, HTML_LONG_URL, function* (url) {
    content.wrappedJSObject.performRequests(1, url);
  });
  yield wait;

  verifyRequestItemTarget(
    document,
    getDisplayedRequests(gStore.getState()),
    getSortedRequests(gStore.getState()).get(0),
    "GET",
    CONTENT_TYPE_SJS + "?fmt=html-long",
    {
      status: 200,
      statusText: "OK"
    });

  let waitDOM = waitForDOM(document, "#response-panel .editor-mount iframe");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".network-details-panel-toggle"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#response-tab"));
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
