/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-webconsole-error-observer.html";

function test()
{
  waitForExplicitFinish();

  expectUncaughtException();

  addTab(TEST_URI);
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    testOpenUI(true);
  }, true);
}

function testOpenUI(aTestReopen)
{
  // test to see if the messages are
  // displayed when the console UI is opened

  openConsole(null, function(hud) {
    testLogEntry(hud.outputNode, "log Bazzle",
                 "Find a console log entry from before console UI is opened",
                 false, null);

    testLogEntry(hud.outputNode, "error Bazzle",
                 "Find a console error entry from before console UI is opened",
                 false, null);

    testLogEntry(hud.outputNode, "bazBug611032", "Found the JavaScript error");
    testLogEntry(hud.outputNode, "cssColorBug611032", "Found the CSS error");

    HUDService.deactivateHUDForContext(gBrowser.selectedTab);

    if (aTestReopen) {
      HUDService.deactivateHUDForContext(gBrowser.selectedTab);
      executeSoon(testOpenUI);
    }
    else {
      executeSoon(finish);
    }
  });
}
