/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Sucan <mihai.sucan@gmail.com>
 */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test//test-bug-621644-jsterm-dollar.html";

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  waitForFocus(function () {
    openConsole();

    let hudId = HUDService.getHudIdByWindow(content);
    let HUD = HUDService.hudReferences[hudId];

    HUD.jsterm.clearOutput();

    HUD.jsterm.setInputValue("$(document.body)");
    HUD.jsterm.execute();

    let outputItem = HUD.outputNode.
                     querySelector(".webconsole-msg-output:last-child");
    ok(outputItem.textContent.indexOf("<p>") > -1,
       "jsterm output is correct for $()");

    HUD.jsterm.clearOutput();

    HUD.jsterm.setInputValue("$$(document)");
    HUD.jsterm.execute();

    outputItem = HUD.outputNode.
                     querySelector(".webconsole-msg-output:last-child");
    ok(outputItem.textContent.indexOf("621644") > -1,
       "jsterm output is correct for $$()");

    executeSoon(finishTest);
  }, content);
}

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", false);
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoad, true);
}
