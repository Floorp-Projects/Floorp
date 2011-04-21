/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Reference to the Workspace chrome window object.
let gWorkspaceWindow;

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    ok(Workspace, "Workspace variable exists");

    gWorkspaceWindow = Workspace.openWorkspace();
    gWorkspaceWindow.addEventListener("load", runTests, false);
  }, true);

  content.location = "data:text/html,initialization test for Workspace";
}

function runTests()
{
  gWorkspaceWindow.removeEventListener("load", arguments.callee, false);

  let ws = gWorkspaceWindow.Workspace;
  ok(ws, "Workspace object exists in new window");
  is(typeof ws.execute, "function", "Workspace.execute() exists");
  is(typeof ws.inspect, "function", "Workspace.inspect() exists");
  is(typeof ws.print, "function", "Workspace.print() exists");

  let chromeContextMenu = gWorkspaceWindow.document.
                          getElementById("ws-menu-chrome");
  ok(chromeContextMenu, "Chrome context menuitem element exists");
  is(chromeContextMenu.getAttribute("hidden"), "true",
     "Chrome context menuitem is hidden");

  let errorConsoleCommand = gWorkspaceWindow.document.
                            getElementById("ws-cmd-errorConsole");
  ok(errorConsoleCommand, "Error console command element exists");
  is(errorConsoleCommand.getAttribute("disabled"), "true",
     "Error console command is disabled");

  let errorConsoleMenu = gWorkspaceWindow.document.
                         getElementById("ws-menu-errorConsole");
  ok(errorConsoleMenu, "Error console menu element exists");
  is(errorConsoleMenu.getAttribute("hidden"), "true",
     "Error console menu item is hidden");

  let chromeContextCommand = gWorkspaceWindow.document.
                            getElementById("ws-cmd-chromeContext");
  ok(chromeContextCommand, "Chrome context command element exists");
  is(chromeContextCommand.getAttribute("disabled"), "true",
     "Chrome context command is disabled");

  gWorkspaceWindow.close();
  gWorkspaceWindow = null;
  gBrowser.removeCurrentTab();
  finish();
}
