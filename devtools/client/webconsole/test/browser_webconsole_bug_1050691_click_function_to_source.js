/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that clicking on a function displays its source in the debugger.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug_1050691_click_function_to_source.html";

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  // Open the Debugger panel.
  let debuggerPanel = yield openDebugger();
  // And right after come back to the Console panel.
  yield openConsole();
  yield testWithDebuggerOpen(hud, debuggerPanel);
});

function* testWithDebuggerOpen(hud, debuggerPanel) {
  let clickable = yield printFunction(hud);
  let panelWin = debuggerPanel.panelWin;
  let onEditorLocationSet = panelWin.once(panelWin.EVENTS.EDITOR_LOCATION_SET);
  synthesizeClick(clickable, hud);
  yield onEditorLocationSet;
  ok(isDebuggerCaretPos(debuggerPanel, 7),
    "Clicking on a function should go to its source in the debugger view");
}

function synthesizeClick(clickable, hud) {
  EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow);
}

var printFunction = Task.async(function* (hud) {
  hud.jsterm.clearOutput();
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.wrappedJSObject.foo();
  });
  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });
  let msg = [...result.matched][0];
  let clickable = msg.querySelector("a");
  ok(clickable, "clickable item for object should exist");
  return clickable;
});
