/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html;charset=utf8,test Scratchpad." +
                     "openErrorConsole()";
}

function runTests()
{
  Services.obs.addObserver(function observer(aSubject) {
    Services.obs.removeObserver(observer, "web-console-created");
    aSubject.QueryInterface(Ci.nsISupportsString);

    let hud = HUDService.getBrowserConsole();
    ok(hud, "browser console is open");
    is(aSubject.data, hud.hudId, "notification hudId is correct");

    HUDService.toggleBrowserConsole().then(finish);
  }, "web-console-created", false);

  let hud = HUDService.getBrowserConsole();
  ok(!hud, "browser console is not open");
  info("wait for the browser console to open from Scratchpad");

  gScratchpadWindow.Scratchpad.openErrorConsole();
}
