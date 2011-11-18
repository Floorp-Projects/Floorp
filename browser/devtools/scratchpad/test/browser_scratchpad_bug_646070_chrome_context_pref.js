/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let gOldPref;
let DEVTOOLS_CHROME_ENABLED = "devtools.chrome.enabled";

function test()
{
  waitForExplicitFinish();

  gOldPref = Services.prefs.getBoolPref(DEVTOOLS_CHROME_ENABLED);
  Services.prefs.setBoolPref(DEVTOOLS_CHROME_ENABLED, true);

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    ok(Scratchpad, "Scratchpad variable exists");

    gScratchpadWindow = Scratchpad.openScratchpad();
    gScratchpadWindow.addEventListener("load", runTests, false);
  }, true);

  content.location = "data:text/html,Scratchpad test for bug 646070 - chrome context preference";
}

function runTests()
{
  gScratchpadWindow.removeEventListener("load", arguments.callee, false);

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

  Services.prefs.setBoolPref(DEVTOOLS_CHROME_ENABLED, gOldPref);

  finish();
}
