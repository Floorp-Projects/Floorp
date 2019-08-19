/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  BrowserConsoleManager,
} = require("devtools/client/webconsole/browser-console-manager");

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    openScratchpad(runTests);
  });

  BrowserTestUtils.loadURI(
    gBrowser,
    "data:text/html;charset=utf8,test Scratchpad." + "openErrorConsole()"
  );
}

function runTests() {
  Services.obs.addObserver(function observer(aSubject) {
    Services.obs.removeObserver(observer, "web-console-created");
    aSubject.QueryInterface(Ci.nsISupportsString);

    const hud = BrowserConsoleManager.getBrowserConsole();
    ok(hud, "browser console is open");
    is(aSubject.data, hud.hudId, "notification hudId is correct");

    BrowserConsoleManager.toggleBrowserConsole().then(finish);
  }, "web-console-created");

  const hud = BrowserConsoleManager.getBrowserConsole();
  ok(!hud, "browser console is not open");
  info("wait for the browser console to open from Scratchpad");

  gScratchpadWindow.Scratchpad.openErrorConsole();
}
