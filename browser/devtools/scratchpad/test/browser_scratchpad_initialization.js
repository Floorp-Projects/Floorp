/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Reference to the Scratchpad chrome window object.
let gScratchpadWindow;

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    ok(Scratchpad, "Scratchpad variable exists");

    gScratchpadWindow = Scratchpad.openScratchpad();
    gScratchpadWindow.addEventListener("load", runTests, false);
  }, true);

  content.location = "data:text/html,initialization test for Scratchpad";
}

function runTests()
{
  gScratchpadWindow.removeEventListener("load", arguments.callee, false);

  let sp = gScratchpadWindow.Scratchpad;
  ok(sp, "Scratchpad object exists in new window");
  is(typeof sp.run, "function", "Scratchpad.run() exists");
  is(typeof sp.inspect, "function", "Scratchpad.inspect() exists");
  is(typeof sp.display, "function", "Scratchpad.display() exists");

  let environmentMenu = gScratchpadWindow.document.
                          getElementById("sp-environment-menu");
  ok(environmentMenu, "Environment menu element exists");
  ok(environmentMenu.hasAttribute("hidden"),
     "Environment menu is not visible");

  let errorConsoleCommand = gScratchpadWindow.document.
                            getElementById("sp-cmd-errorConsole");
  ok(errorConsoleCommand, "Error console command element exists");
  is(errorConsoleCommand.getAttribute("disabled"), "true",
     "Error console command is disabled");

  let chromeContextCommand = gScratchpadWindow.document.
                            getElementById("sp-cmd-browserContext");
  ok(chromeContextCommand, "Chrome context command element exists");
  is(chromeContextCommand.getAttribute("disabled"), "true",
     "Chrome context command is disabled");

  gScratchpadWindow.close();
  gScratchpadWindow = null;
  gBrowser.removeCurrentTab();
  finish();
}
