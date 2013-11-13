/* Any copyright is dedicated to the Public Domain
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that the JS input field is focused when the user switches back to the
// web console from other tools, see bug 891581.

const TEST_URI = "data:text/html;charset=utf8,<p>hello";

function test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(hud)
{
  is(hud.jsterm.inputNode.hasAttribute("focused"), true,
     "inputNode should be focused");

  hud.ui.filterBox.focus();

  is(hud.ui.filterBox.hasAttribute("focused"), true,
     "filterBox should be focused");

  is(hud.jsterm.inputNode.hasAttribute("focused"), false,
     "inputNode shouldn't be focused");

  openDebugger().then(debuggerOpened);
}

function debuggerOpened()
{
  openConsole(null, consoleReopened);
}

function consoleReopened(hud)
{
  is(hud.jsterm.inputNode.hasAttribute("focused"), true,
     "inputNode should be focused");

  finishTest();
}
