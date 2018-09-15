/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that clicking on a function displays its source in the debugger. See Bug 1050691.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/" +
                 "test-click-function-to-source.html";

// Force the old debugger UI since it's directly used (see Bug 1301705)
pushPref("devtools.debugger.new-debugger-frontend", false);

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Open the Debugger panel.");
  const {panel} = await openDebugger();
  const panelWin = panel.panelWin;

  info("And right after come back to the Console panel.");
  await openConsole();

  info("Log a function");
  const onLoggedFunction = waitForMessage(hud, "function foo");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.wrappedJSObject.foo();
  });
  const {node} = await onLoggedFunction;
  const jumpIcon = node.querySelector(".jump-definition");
  ok(jumpIcon, "A jump to definition button is rendered, as expected");

  info("Click on the jump to definition button.");
  const onEditorLocationSet = panelWin.once(panelWin.EVENTS.EDITOR_LOCATION_SET);
  jumpIcon.click();
  await onEditorLocationSet;

  const {editor} = panelWin.DebuggerView;
  const {line, ch} = editor.getCursor();
  // Source editor starts counting line and column numbers from 0.
  is(line, 8, "Debugger is open at the expected line");
  is(ch, 0, "Debugger is open at the expected character");
});
