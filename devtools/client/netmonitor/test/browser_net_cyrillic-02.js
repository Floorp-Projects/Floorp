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

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  yield wait;

  verifyRequestItemTarget(RequestsMenu, RequestsMenu.getItemAtIndex(0),
    "GET", CYRILLIC_URL, {
      status: 200,
      statusText: "OK"
    });

  wait = waitForDOM(document, "#response-tabpanel .editor-mount iframe");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll("#details-pane tab")[3]);
  let [editor] = yield wait;
  yield once(editor, "DOMContentLoaded");
  yield waitForDOM(editor.contentDocument, ".CodeMirror-code");
  let text = editor.contentDocument
          .querySelector(".CodeMirror-code").textContent;

  ok(text.includes("\u0411\u0440\u0430\u0442\u0430\u043d"),
    "The text shown in the source editor is correct.");

  return teardown(monitor);
});
