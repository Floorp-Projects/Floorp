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

  content.location = "data:text/html,<title>foobarBug636725</title>" +
    "<p>test inspect() in Workspace";
}

function runTests()
{
  gWorkspaceWindow.removeEventListener("load", arguments.callee, false);

  let ws = gWorkspaceWindow.Workspace;
  let doc = gWorkspaceWindow.document;

  let methodsAndItems = {
    "ws-menu-newworkspace": "openWorkspace",
    "ws-menu-open": "openFile",
    "ws-menu-save": "saveFile",
    "ws-menu-saveas": "saveFileAs",
    "ws-text-execute": "execute",
    "ws-text-inspect": "inspect",
    "ws-menu-content": "setContentContext",
    "ws-menu-chrome": "setChromeContext",
    "ws-menu-resetContext": "resetContext",
    "ws-menu-errorConsole": "openErrorConsole",
    "ws-menu-webConsole": "openWebConsole",
  };

  let lastMethodCalled = null;
  ws.__noSuchMethod__ = function(aMethodName) {
    lastMethodCalled = aMethodName;
  };

  for (let id in methodsAndItems) {
    lastMethodCalled = null;

    let methodName = methodsAndItems[id];
    let oldMethod = ws[methodName];
    ok(oldMethod, "found method " + methodName + " in Workspace object");

    delete ws[methodName];

    let menu = doc.getElementById(id);
    ok(menu, "found menuitem #" + id);

    try {
      menu.doCommand();
    }
    catch (ex) {
      ok(false, "exception thrown while executing the command of menuitem #" + id);
    }

    ok(lastMethodCalled == methodName,
       "method " + methodName + " invoked by the associated menuitem");

    ws[methodName] = oldMethod;
  }

  delete ws.__noSuchMethod__;

  gWorkspaceWindow.close();
  gWorkspaceWindow = null;
  gBrowser.removeCurrentTab();
  finish();
}
