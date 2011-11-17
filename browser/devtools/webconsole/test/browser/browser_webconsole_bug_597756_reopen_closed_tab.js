/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test//browser/test-bug-597756-reopen-closed-tab.html";

let newTabIsOpen = false;

function tabLoaded(aEvent) {
  gBrowser.selectedBrowser.removeEventListener(aEvent.type, arguments.callee, true);

  HUDService.activateHUDForContext(gBrowser.selectedTab);

  gBrowser.selectedBrowser.addEventListener("load", tabReloaded, true);
  expectUncaughtException();
  content.location.reload();
}

function tabReloaded(aEvent) {
  gBrowser.selectedBrowser.removeEventListener(aEvent.type, arguments.callee, true);

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];
  ok(HUD, "Web Console is open");

  isnot(HUD.outputNode.textContent.indexOf("fooBug597756_error"), -1,
    "error message must be in console output");

  executeSoon(function() {
    if (newTabIsOpen) {
      testEnd();
      return;
    }

    let newTab = gBrowser.addTab();
    gBrowser.removeCurrentTab();
    gBrowser.selectedTab = newTab;

    newTabIsOpen = true;
    gBrowser.selectedBrowser.addEventListener("load", tabLoaded, true);
    expectUncaughtException();
    content.location = TEST_URI;
  });
}

function testEnd() {
  gBrowser.removeCurrentTab();
  executeSoon(finishTest);
}

function test() {
  expectUncaughtException();
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

