/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 646070 */

var DEVTOOLS_CHROME_ENABLED = "devtools.chrome.enabled";

function test()
{
  waitForExplicitFinish();

  Services.prefs.setBoolPref(DEVTOOLS_CHROME_ENABLED, true);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedBrowser.addEventListener("load", function () {
    openScratchpad(runTests);
  }, {capture: true, once: true});

  content.location = "data:text/html,Scratchpad test for bug 646070 - chrome context preference";
}

function runTests()
{
  let sp = gScratchpadWindow.Scratchpad;
  ok(sp, "Scratchpad object exists in new window");

  let environmentMenu = gScratchpadWindow.document.
                          getElementById("sp-environment-menu");
  ok(environmentMenu, "Environment menu element exists");
  ok(!environmentMenu.hasAttribute("hidden"),
     "Environment menu is visible");

  let errorConsoleCommand = gScratchpadWindow.document.
                            getElementById("sp-cmd-errorConsole");
  ok(errorConsoleCommand, "Error console command element exists");
  ok(!errorConsoleCommand.hasAttribute("disabled"),
     "Error console command is enabled");

  let chromeContextCommand = gScratchpadWindow.document.
                            getElementById("sp-cmd-browserContext");
  ok(chromeContextCommand, "Chrome context command element exists");
  ok(!chromeContextCommand.hasAttribute("disabled"),
     "Chrome context command is disabled");

  Services.prefs.clearUserPref(DEVTOOLS_CHROME_ENABLED);

  finish();
}
