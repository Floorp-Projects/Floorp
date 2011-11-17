/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test//browser/test-bug-601177-log-levels.html";

let msgs;

function onContentLoaded()
{
  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];
  msgs = HUD.outputNode.querySelectorAll(".hud-msg-node");

  findEntry(HUD, "hud-networkinfo", "test-bug-601177-log-levels.html",
            "found test-bug-601177-log-levels.html");

  findEntry(HUD, "hud-networkinfo", "test-bug-601177-log-levels.js",
            "found test-bug-601177-log-levels.js");

  findEntry(HUD, "hud-networkinfo", "test-image.png", "found test-image.png");

  findEntry(HUD, "hud-network", "foobar-known-to-fail.png",
            "found foobar-known-to-fail.png");

  findEntry(HUD, "hud-exception", "foobarBug601177exception",
            "found exception");

  findEntry(HUD, "hud-jswarn", "undefinedPropertyBug601177",
            "found strict warning");

  findEntry(HUD, "hud-jswarn", "foobarBug601177strictError",
            "found strict error");

  msgs = null;
  Services.prefs.setBoolPref("javascript.options.strict", false);
  finishTest();
}

function findEntry(aHUD, aClass, aString, aMessage)
{
  return testLogEntry(aHUD.outputNode, aString, aMessage, false, false,
                      aClass);
}

function test()
{
  addTab("data:text/html,Web Console test for bug 601177: log levels");

  Services.prefs.setBoolPref("javascript.options.strict", true);

  browser.addEventListener("load", function(aEvent) {
    browser.removeEventListener(aEvent.type, arguments.callee, true);

    openConsole();

    browser.addEventListener("load", function(aEvent) {
      browser.removeEventListener(aEvent.type, arguments.callee, true);
      executeSoon(onContentLoaded);
    }, true);
    expectUncaughtException();
    content.location = TEST_URI;
  }, true);
}

