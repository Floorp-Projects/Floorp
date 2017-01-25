/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if cyrillic text is rendered correctly in the source editor.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CYRILLIC_URL);
  info("Starting test... ");

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  verifyRequestItemTarget(RequestsMenu, RequestsMenu.getItemAtIndex(0),
    "GET", CONTENT_TYPE_SJS + "?fmt=txt", {
      status: 200,
      statusText: "DA DA DA"
    });

  wait = waitForDOM(document, "#panel-3 .editor-mount iframe");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  document.querySelector("#tab-3 a").click();
  let [editor] = yield wait;
  yield once(editor, "DOMContentLoaded");
  yield waitForDOM(editor.contentDocument, ".CodeMirror-code");
  let text = editor.contentDocument
          .querySelector(".CodeMirror-line").textContent;

  ok(text.includes("\u0411\u0440\u0430\u0442\u0430\u043d"),
    "The text shown in the source editor is correct.");

  yield teardown(monitor);
});
