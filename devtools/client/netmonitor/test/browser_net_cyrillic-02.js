/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if cyrillic text is rendered correctly in the source editor
 * when loaded directly from an HTML page.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CYRILLIC_URL);
  info("Starting test... ");

  let { document, gStore, windowRequire } = monitor.panelWin;
  let {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  let wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  yield wait;

  verifyRequestItemTarget(
    document,
    getDisplayedRequests(gStore.getState()),
    getSortedRequests(gStore.getState()).get(0),
    "GET",
    CYRILLIC_URL,
    {
      status: 200,
      statusText: "OK"
    });

  wait = waitForDOM(document, "#headers-panel");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  yield wait;
  wait = waitForDOM(document, "#response-panel .editor-mount iframe");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#response-tab"));
  let [editor] = yield wait;
  yield once(editor, "DOMContentLoaded");
  yield waitForDOM(editor.contentDocument, ".CodeMirror-code");
  let text = editor.contentDocument
          .querySelector(".CodeMirror-code").textContent;

  ok(text.includes("\u0411\u0440\u0430\u0442\u0430\u043d"),
    "The text shown in the source editor is correct.");

  return teardown(monitor);
});
