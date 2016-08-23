/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if cyrillic text is rendered correctly in the source editor
 * when loaded directly from an HTML page.
 */

add_task(function* () {
  let [tab, , monitor] = yield initNetMonitor(CYRILLIC_URL);
  info("Starting test... ");

  let { document, EVENTS, Editor, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  yield wait;

  verifyRequestItemTarget(RequestsMenu.getItemAtIndex(0),
    "GET", CYRILLIC_URL, {
      status: 200,
      statusText: "OK"
    });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll("#details-pane tab")[3]);

  yield monitor.panelWin.once(EVENTS.RESPONSE_BODY_DISPLAYED);
  let editor = yield NetMonitorView.editor("#response-content-textarea");
  // u044F = я
  is(editor.getText().indexOf("\u044F"), 486,
    "The text shown in the source editor is correct.");
  is(editor.getMode(), Editor.modes.html,
    "The mode active in the source editor is correct.");

  return teardown(monitor);
});
