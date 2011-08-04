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

  ok(sp.textbox, "textbox exists");
  sp.textbox.value = "++window.foobarBug636725";

  let exec = sp.run();
  is(exec[0], sp.textbox.value, "execute()[0] is correct");
  is(exec[1], content.wrappedJSObject.foobarBug636725,
     "execute()[1] is correct");

  is(sp.textbox.value, "++window.foobarBug636725",
     "execute() does not change the textbox value");

  is(content.wrappedJSObject.foobarBug636725, 2,
     "execute() updated window.foobarBug636725");

  sp.display();

  is(content.wrappedJSObject.foobarBug636725, 3,
     "print() updated window.foobarBug636725");

  is(sp.textbox.value, "++window.foobarBug636725/*\n3\n*/",
     "print() shows evaluation result in the textbox");

  is(sp.selectedText, "/*\n3\n*/", "selectedText is correct");
  is(sp.textbox.selectionStart, 24, "selectionStart is correct");
  is(sp.textbox.selectionEnd, 31, "selectionEnd is correct");

  // Test selection execute() and print().

  sp.textbox.value = "window.foobarBug636725 = 'a';\n" +
                     "window.foobarBug636725 = 'b';";

  sp.selectRange(1, 2);

  is(sp.textbox.selectionStart, 1, "selectionStart is 1");
  is(sp.textbox.selectionEnd, 2, "selectionEnd is 2");

  sp.selectRange(0, 29);

  is(sp.textbox.selectionStart, 0, "selectionStart is 0");
  is(sp.textbox.selectionEnd, 29, "selectionEnd is 29");

  exec = sp.run();

  is(exec[0], "window.foobarBug636725 = 'a';",
     "execute()[0] is correct");
  is(exec[1], "a",
     "execute()[1] is correct");

  is(sp.textbox.value, "window.foobarBug636725 = 'a';\n" +
                       "window.foobarBug636725 = 'b';",
     "execute() does not change the textbox value");

  is(content.wrappedJSObject.foobarBug636725, "a",
     "execute() worked for the selected range");

  sp.textbox.value = "window.foobarBug636725 = 'c';\n" +
                     "window.foobarBug636725 = 'b';";

  sp.selectRange(0, 22);

  sp.display();

  is(content.wrappedJSObject.foobarBug636725, "a",
     "print() worked for the selected range");

  is(sp.textbox.value, "window.foobarBug636725" +
                       "/*\na\n*/" +
                       " = 'c';\n" +
                       "window.foobarBug636725 = 'b';",
     "print() shows evaluation result in the textbox");

  is(sp.selectedText, "/*\na\n*/", "selectedText is correct");
  is(sp.textbox.selectionStart, 22, "selectionStart is correct");
  is(sp.textbox.selectionEnd, 29, "selectionEnd is correct");

  sp.deselect();

  ok(!sp.selectedText, "selectedText is empty");
  is(sp.textbox.selectionStart, sp.textbox.selectionEnd, "deselect() works");

  gScratchpadWindow.close();
  gScratchpadWindow = null;
  gBrowser.removeCurrentTab();
  finish();
}
