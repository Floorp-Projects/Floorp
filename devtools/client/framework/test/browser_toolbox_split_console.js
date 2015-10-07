/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that these toolbox split console APIs work:
//  * toolbox.useKeyWithSplitConsole()
//  * toolbox.isSplitConsoleFocused

let gToolbox = null;
let panelWin = null;

const URL = "data:text/html;charset=utf8,test split console key delegation";
const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

add_task(function*() {
  let tab = yield addTab(URL);
  let target = TargetFactory.forTab(tab);
  gToolbox = yield gDevTools.showToolbox(target, "inspector");
  panelWin = gToolbox.getPanel("inspector").panelWin;

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

  let keyElm = panelWin.document.createElementNS(XULNS, "key");
  keyElm.setAttribute("keycode", "VK_F3");
  keyElm.addEventListener("command", () => {commandCalled = true}, false);
  panelWin.document.getElementsByTagName('keyset')[0].appendChild(keyElm);

  info("useKeyWithSplitConsole on inspector while inspector is focused");
  gToolbox.useKeyWithSplitConsole(keyElm, "inspector");

  info("synthesizeKey with the console focused");
  let consoleInput = gToolbox.getPanel("webconsole").hud.jsterm.inputNode;
  consoleInput.focus();
  synthesizeKeyElement(keyElm);

  ok(commandCalled, "Shortcut key should trigger the command");
}

// A key bound to a *different* tool should not trigger it's command
function* testUseKeyWithSplitConsoleWrongTool() {
  let commandCalled = false;

  let keyElm = panelWin.document.createElementNS(XULNS, "key");
  keyElm.setAttribute("keycode", "VK_F4");
  keyElm.addEventListener("command", () => {commandCalled = true}, false);
  panelWin.document.getElementsByTagName('keyset')[0].appendChild(keyElm);

  info("useKeyWithSplitConsole on jsdebugger while inspector is focused");
  gToolbox.useKeyWithSplitConsole(keyElm, "jsdebugger");

  info("synthesizeKey with the console focused");
  let consoleInput = gToolbox.getPanel("webconsole").hud.jsterm.inputNode;
  consoleInput.focus();
  synthesizeKeyElement(keyElm);

  ok(!commandCalled, "Shortcut key shouldn't trigger the command");
}

function* cleanup() {
  // We don't want the open split console to confuse other tests..
  Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
  yield gToolbox.destroy();
  gBrowser.removeCurrentTab();
  gToolbox = panelWin = null;
}
