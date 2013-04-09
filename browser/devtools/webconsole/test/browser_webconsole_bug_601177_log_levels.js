/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-601177-log-levels.html";

function performTest()
{
  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];

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

  executeSoon(finishTest);
}

function findEntry(aHUD, aClass, aString, aMessage)
{
  return testLogEntry(aHUD.outputNode, aString, aMessage, false, false,
                      aClass);
}

function test()
{
  Services.prefs.setBoolPref("javascript.options.strict", true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("javascript.options.strict");
  });

  addTab("data:text/html;charset=utf-8,Web Console test for bug 601177: log levels");

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    openConsole(null, function(hud) {
      browser.addEventListener("load", function onLoad2() {
        browser.removeEventListener("load", onLoad2, true);
        waitForSuccess({
          name: "all messages displayed",
          validatorFn: function()
          {
            return hud.outputNode.itemCount >= 7;
          },
          successFn: performTest,
          failureFn: function() {
            info("itemCount: " + hud.outputNode.itemCount);
            finishTest();
          },
        });
      }, true);

      expectUncaughtException();
      content.location = TEST_URI;
    });
  }, true);
}

