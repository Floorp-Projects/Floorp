/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that these toolbox split console APIs work:
//  * toolbox.useKeyWithSplitConsole()
//  * toolbox.isSplitConsoleFocused

let gToolbox = null;
let panelWin = null;

const URL = "data:text/html;charset=utf8,test split console key delegation";

// Force the old debugger UI since it's directly used (see Bug 1301705)
Services.prefs.setBoolPref("devtools.debugger.new-debugger-frontend", false);
registerCleanupFunction(function* () {
  Services.prefs.clearUserPref("devtools.debugger.new-debugger-frontend");
});

add_task(function* () {
  let tab = yield addTab(URL);
  let target = TargetFactory.forTab(tab);
  gToolbox = yield gDevTools.showToolbox(target, "jsdebugger");
  panelWin = gToolbox.getPanel("jsdebugger").panelWin;

  yield gToolbox.openSplitConsole();
  yield testIsSplitConsoleFocused();
  yield testUseKeyWithSplitConsole();
  yield testUseKeyWithSplitConsoleWrongTool();

  yield cleanup();
});

function* testIsSplitConsoleFocused() {
  yield gToolbox.openSplitConsole();
  // The newly opened split console should have focus
  ok(gToolbox.isSplitConsoleFocused(), "Split console is focused");
  panelWin.focus();
  ok(!gToolbox.isSplitConsoleFocused(), "Split console is no longer focused");
}

// A key bound to the selected tool should trigger it's command
function* testUseKeyWithSplitConsole() {
  let commandCalled = false;

  info("useKeyWithSplitConsole on debugger while debugger is focused");
  gToolbox.useKeyWithSplitConsole("F3", () => {
    commandCalled = true;
  }, "jsdebugger");

  info("synthesizeKey with the console focused");
  let consoleInput = gToolbox.getPanel("webconsole").hud.jsterm.inputNode;
  consoleInput.focus();
  synthesizeKeyShortcut("F3", panelWin);

  ok(commandCalled, "Shortcut key should trigger the command");
}

// A key bound to a *different* tool should not trigger it's command
function* testUseKeyWithSplitConsoleWrongTool() {
  let commandCalled = false;

  info("useKeyWithSplitConsole on inspector while debugger is focused");
  gToolbox.useKeyWithSplitConsole("F4", () => {
    commandCalled = true;
  }, "inspector");

  info("synthesizeKey with the console focused");
  let consoleInput = gToolbox.getPanel("webconsole").hud.jsterm.inputNode;
  consoleInput.focus();
  synthesizeKeyShortcut("F4", panelWin);

  ok(!commandCalled, "Shortcut key shouldn't trigger the command");
}

function* cleanup() {
  // We don't want the open split console to confuse other tests..
  Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
  yield gToolbox.destroy();
  gBrowser.removeCurrentTab();
  gToolbox = panelWin = null;
}
