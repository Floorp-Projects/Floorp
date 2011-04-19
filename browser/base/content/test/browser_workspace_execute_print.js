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

  content.location = "data:text/html,<p>test execute() and print() in Workspace";
}

function runTests()
{
  gWorkspaceWindow.removeEventListener("load", arguments.callee, false);

  let ws = gWorkspaceWindow.Workspace;

  content.wrappedJSObject.foobarBug636725 = 1;

  ok(ws.textbox, "textbox exists");
  ws.textbox.value = "++window.foobarBug636725";

  let exec = ws.execute();
  is(exec[0], ws.textbox.value, "execute()[0] is correct");
  is(exec[1], content.wrappedJSObject.foobarBug636725,
     "execute()[1] is correct");

  is(ws.textbox.value, "++window.foobarBug636725",
     "execute() does not change the textbox value");

  is(content.wrappedJSObject.foobarBug636725, 2,
     "execute() updated window.foobarBug636725");

  ws.print();

  is(content.wrappedJSObject.foobarBug636725, 3,
     "print() updated window.foobarBug636725");

  is(ws.textbox.value, "++window.foobarBug636725/*\n3\n*/",
     "print() shows evaluation result in the textbox");

  is(ws.selectedText, "/*\n3\n*/", "selectedText is correct");
  is(ws.textbox.selectionStart, 24, "selectionStart is correct");
  is(ws.textbox.selectionEnd, 31, "selectionEnd is correct");

  // Test selection execute() and print().

  ws.textbox.value = "window.foobarBug636725 = 'a';\n" +
                     "window.foobarBug636725 = 'b';";

  ws.selectRange(1, 2);

  is(ws.textbox.selectionStart, 1, "selectionStart is 1");
  is(ws.textbox.selectionEnd, 2, "selectionEnd is 2");

  ws.selectRange(0, 29);

  is(ws.textbox.selectionStart, 0, "selectionStart is 0");
  is(ws.textbox.selectionEnd, 29, "selectionEnd is 29");

  exec = ws.execute();

  is(exec[0], "window.foobarBug636725 = 'a';",
     "execute()[0] is correct");
  is(exec[1], "a",
     "execute()[1] is correct");

  is(ws.textbox.value, "window.foobarBug636725 = 'a';\n" +
                       "window.foobarBug636725 = 'b';",
     "execute() does not change the textbox value");

  is(content.wrappedJSObject.foobarBug636725, "a",
     "execute() worked for the selected range");

  ws.textbox.value = "window.foobarBug636725 = 'c';\n" +
                     "window.foobarBug636725 = 'b';";

  ws.selectRange(0, 22);

  ws.print();

  is(content.wrappedJSObject.foobarBug636725, "a",
     "print() worked for the selected range");

  is(ws.textbox.value, "window.foobarBug636725" +
                       "/*\na\n*/" +
                       " = 'c';\n" +
                       "window.foobarBug636725 = 'b';",
     "print() shows evaluation result in the textbox");

  is(ws.selectedText, "/*\na\n*/", "selectedText is correct");
  is(ws.textbox.selectionStart, 22, "selectionStart is correct");
  is(ws.textbox.selectionEnd, 29, "selectionEnd is correct");

  ws.deselect();

  ok(!ws.selectedText, "selectedText is empty");
  is(ws.textbox.selectionStart, ws.textbox.selectionEnd, "deselect() works");

  gWorkspaceWindow.close();
  gWorkspaceWindow = null;
  gBrowser.removeCurrentTab();
  finish();
}
