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

    gScratchpadWindow = Scratchpad.openScratchpad();
    gScratchpadWindow.addEventListener("load", runTests, false);
  }, true);

  content.location = "data:text/html,<p>test run() and display() in Scratchpad";
}

function runTests()
{
  gScratchpadWindow.removeEventListener("load", arguments.callee, false);

  let sp = gScratchpadWindow.Scratchpad;

  content.wrappedJSObject.foobarBug636725 = 1;

  sp.setText("++window.foobarBug636725");

  let exec = sp.run();
  is(exec[0], sp.getText(), "run()[0] is correct");
  ok(!exec[1], "run()[1] is correct");
  is(exec[2], content.wrappedJSObject.foobarBug636725,
     "run()[2] is correct");

  is(sp.getText(), "++window.foobarBug636725",
     "run() does not change the editor content");

  is(content.wrappedJSObject.foobarBug636725, 2,
     "run() updated window.foobarBug636725");

  sp.display();

  is(content.wrappedJSObject.foobarBug636725, 3,
     "display() updated window.foobarBug636725");

  is(sp.getText(), "++window.foobarBug636725/*\n3\n*/",
     "display() shows evaluation result in the textbox");

  is(sp.selectedText, "/*\n3\n*/", "selectedText is correct");
  let selection = sp.getSelectionRange();
  is(selection.start, 24, "selection.start is correct");
  is(selection.end, 31, "selection.end is correct");

  // Test selection run() and display().

  sp.setText("window.foobarBug636725 = 'a';\n" +
             "window.foobarBug636725 = 'b';");

  sp.selectRange(1, 2);

  selection = sp.getSelectionRange();

  is(selection.start, 1, "selection.start is 1");
  is(selection.end, 2, "selection.end is 2");

  sp.selectRange(0, 29);

  selection = sp.getSelectionRange();

  is(selection.start, 0, "selection.start is 0");
  is(selection.end, 29, "selection.end is 29");

  exec = sp.run();

  is(exec[0], "window.foobarBug636725 = 'a';",
     "run()[0] is correct");
  ok(!exec[1], 
     "run()[1] is correct");
  is(exec[2], "a",
     "run()[2] is correct");

  is(sp.getText(), "window.foobarBug636725 = 'a';\n" +
                   "window.foobarBug636725 = 'b';",
     "run() does not change the textbox value");

  is(content.wrappedJSObject.foobarBug636725, "a",
     "run() worked for the selected range");

  sp.setText("window.foobarBug636725 = 'c';\n" +
             "window.foobarBug636725 = 'b';");

  sp.selectRange(0, 22);

  sp.display();

  is(content.wrappedJSObject.foobarBug636725, "a",
     "display() worked for the selected range");

  is(sp.getText(), "window.foobarBug636725" +
                   "/*\na\n*/" +
                   " = 'c';\n" +
                   "window.foobarBug636725 = 'b';",
     "display() shows evaluation result in the textbox");

  is(sp.selectedText, "/*\na\n*/", "selectedText is correct");

  selection = sp.getSelectionRange();
  is(selection.start, 22, "selection.start is correct");
  is(selection.end, 29, "selection.end is correct");

  sp.deselect();

  ok(!sp.selectedText, "selectedText is empty");

  selection = sp.getSelectionRange();
  is(selection.start, selection.end, "deselect() works");

  // Test undo/redo.

  sp.setText("foo1");
  sp.setText("foo2");
  is(sp.getText(), "foo2", "editor content updated");
  sp.undo();
  is(sp.getText(), "foo1", "undo() works");
  sp.redo();
  is(sp.getText(), "foo2", "redo() works");

  gScratchpadWindow.close();
  gScratchpadWindow = null;
  gBrowser.removeCurrentTab();
  finish();
}
