/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Reference to the Workspace chrome window object.
let gWorkspaceWindow;

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

    ok(Workspace, "Workspace variable exists");

    gWorkspaceWindow = Workspace.openWorkspace();
    gWorkspaceWindow.addEventListener("load", runTests, false);
  }, true);

  content.location = "data:text/html,Workspace test for bug 646070 - chrome context preference";
}

function runTests()
{
  gWorkspaceWindow.removeEventListener("load", arguments.callee, false);

  let ws = gWorkspaceWindow.Workspace;
  ok(ws, "Workspace object exists in new window");

  let contextMenu = gWorkspaceWindow.document.getElementById("ws-context-menu");
  ok(contextMenu, "Context menu element exists");
  ok(!contextMenu.hasAttribute("hidden"), "Context menu is visible");

  let errorConsoleCommand = gWorkspaceWindow.document.
                            getElementById("ws-cmd-errorConsole");
  ok(errorConsoleCommand, "Error console command element exists");
  ok(!errorConsoleCommand.hasAttribute("disabled"),
     "Error console command is enabled");

  let errorConsoleMenu = gWorkspaceWindow.document.
                         getElementById("ws-menu-errorConsole");
  ok(errorConsoleMenu, "Error console menu element exists");
  ok(!errorConsoleMenu.hasAttribute("hidden"),
     "Error console menuitem is visible");

  let chromeContextCommand = gWorkspaceWindow.document.
                            getElementById("ws-cmd-chromeContext");
  ok(chromeContextCommand, "Chrome context command element exists");
  ok(!chromeContextCommand.hasAttribute("disabled"),
     "Chrome context command is disabled");

  Services.prefs.setBoolPref(DEVTOOLS_CHROME_ENABLED, gOldPref);

  gWorkspaceWindow.close();
  gWorkspaceWindow = null;
  gBrowser.removeCurrentTab();
  finish();
}
