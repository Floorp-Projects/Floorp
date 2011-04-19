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

    gWorkspaceWindow = Workspace.openWorkspace();
    gWorkspaceWindow.addEventListener("load", runTests, false);
  }, true);

  content.location = "data:text/html,test context switch in Workspace";
}

function runTests()
{
  gWorkspaceWindow.removeEventListener("load", arguments.callee, false);

  let ws = gWorkspaceWindow.Workspace;

  let contentMenu = gWorkspaceWindow.document.getElementById("ws-menu-content");
  let chromeMenu = gWorkspaceWindow.document.getElementById("ws-menu-chrome");
  let statusbar = ws.statusbarStatus;

  ok(contentMenu, "found #ws-menu-content");
  ok(chromeMenu, "found #ws-menu-chrome");
  ok(statusbar, "found Workspace.statusbarStatus");

  ws.setContentContext();

  is(ws.executionContext, gWorkspaceWindow.WORKSPACE_CONTEXT_CONTENT,
     "executionContext is content");

  is(contentMenu.getAttribute("checked"), "true",
     "content menuitem is checked");

  ok(!chromeMenu.hasAttribute("checked"),
     "chrome menuitem is not checked");

  is(statusbar.getAttribute("label"), contentMenu.getAttribute("label"),
     "statusbar label is correct");

  ok(ws.textbox, "textbox exists");
  ws.textbox.value = "window.foobarBug636725 = 'aloha';";

  ok(!content.wrappedJSObject.foobarBug636725,
     "no content.foobarBug636725");

  ws.execute();

  is(content.wrappedJSObject.foobarBug636725, "aloha",
     "content.foobarBug636725 has been set");

  ws.setChromeContext();

  is(ws.executionContext, gWorkspaceWindow.WORKSPACE_CONTEXT_CHROME,
     "executionContext is chrome");

  is(chromeMenu.getAttribute("checked"), "true",
     "chrome menuitem is checked");

  ok(!contentMenu.hasAttribute("checked"),
     "content menuitem is not checked");

  is(statusbar.getAttribute("label"), chromeMenu.getAttribute("label"),
     "statusbar label is correct");

  ws.textbox.value = "window.foobarBug636725 = 'aloha2';";

  ok(!window.foobarBug636725, "no window.foobarBug636725");

  ws.execute();

  is(window.foobarBug636725, "aloha2", "window.foobarBug636725 has been set");

  ws.textbox.value = "window.gBrowser";

  is(typeof ws.execute()[1].addTab, "function",
     "chrome context has access to chrome objects");

  gWorkspaceWindow.close();
  gWorkspaceWindow = null;
  gBrowser.removeCurrentTab();
  finish();
}
