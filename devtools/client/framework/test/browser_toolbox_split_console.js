/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that these toolbox split console APIs work:
//  * toolbox.useKeyWithSplitConsole()
//  * toolbox.isSplitConsoleFocused

let gToolbox = null;
let panelWin = null;

const URL = "data:text/html;charset=utf8,test split console key delegation";

add_task(async function() {
  const tab = await addTab(URL);
  gToolbox = await gDevTools.showToolboxForTab(tab, { toolId: "jsdebugger" });
  panelWin = gToolbox.getPanel("jsdebugger").panelWin;

  await gToolbox.openSplitConsole();
  await testIsSplitConsoleFocused();
  await testUseKeyWithSplitConsole();
  await testUseKeyWithSplitConsoleWrongTool();

  await cleanup();
});

async function testIsSplitConsoleFocused() {
  await gToolbox.openSplitConsole();
  // The newly opened split console should have focus
  ok(gToolbox.isSplitConsoleFocused(), "Split console is focused");
  panelWin.focus();
  ok(!gToolbox.isSplitConsoleFocused(), "Split console is no longer focused");
}

// A key bound to the selected tool should trigger it's command
function testUseKeyWithSplitConsole() {
  let commandCalled = false;

  info("useKeyWithSplitConsole on debugger while debugger is focused");
  gToolbox.useKeyWithSplitConsole(
    "F3",
    () => {
      commandCalled = true;
    },
    "jsdebugger"
  );

  info("synthesizeKey with the console focused");
  focusConsoleInput();
  synthesizeKeyShortcut("F3", panelWin);

  ok(commandCalled, "Shortcut key should trigger the command");
}

// A key bound to a *different* tool should not trigger it's command
function testUseKeyWithSplitConsoleWrongTool() {
  let commandCalled = false;

  info("useKeyWithSplitConsole on inspector while debugger is focused");
  gToolbox.useKeyWithSplitConsole(
    "F4",
    () => {
      commandCalled = true;
    },
    "inspector"
  );

  info("synthesizeKey with the console focused");
  focusConsoleInput();
  synthesizeKeyShortcut("F4", panelWin);

  ok(!commandCalled, "Shortcut key shouldn't trigger the command");
}

async function cleanup() {
  await gToolbox.destroy();
  gBrowser.removeCurrentTab();
  gToolbox = panelWin = null;
}

function focusConsoleInput() {
  gToolbox.getPanel("webconsole").hud.jsterm.focus();
}
