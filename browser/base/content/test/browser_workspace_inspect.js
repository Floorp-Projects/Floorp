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

  ok(ws.textbox, "textbox exists");
  ws.textbox.value = "document";

  ws.inspect();

  let propPanel = document.querySelector(".workspace_propertyPanel");
  ok(propPanel, "property panel is open");

  propPanel.addEventListener("popupshown", function() {
    propPanel.removeEventListener("popupshown", arguments.callee, false);

    let tree = propPanel.querySelector("tree");
    ok(tree, "property panel tree found");

    let column = tree.columns[0];
    let found = false;

    for (let i = 0; i < tree.view.rowCount; i++) {
      let cell = tree.view.getCellText(i, column);
      if (cell == 'title: "foobarBug636725"') {
        found = true;
        break;
      }
    }
    ok(found, "found the document.title property");

    executeSoon(function() {
      propPanel.hidePopup();

      gWorkspaceWindow.close();
      gWorkspaceWindow = null;
      gBrowser.removeCurrentTab();
      finish();
    });
  }, false);
}
