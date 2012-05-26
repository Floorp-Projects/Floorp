/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Julian Viereck <jviereck@mozilla.com>
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-own-console.html";

function test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testOpenWebConsole);
  }, true);
}

function testOpenWebConsole(aHud)
{
  hud = aHud;
  ok(hud, "WebConsole was opened");

  testOwnConsole();
}

function testConsoleOnPage(console) {
  isnot(console, undefined, "Console object defined on page");
  is(console.foo, "bar", "Custom console is not overwritten");
}

function testOwnConsole()
{
  let console = browser.contentWindow.wrappedJSObject.console;
  // Test console on the page. There is already one so it shouldn't be
  // overwritten by the WebConsole's console.
  testConsoleOnPage(console);

  // Check that the console object is set on the HUD object although there
  // is no console object added to the page.
  ok(hud.console, "HUD console is defined");
  finishTest();
}
