/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onTabLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onTabLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html,<p>test run() and display() in Scratchpad";
}


function runTests()
{
  let sp = gScratchpadWindow.Scratchpad;
  let tests = [{
    method: "run",
    prepare: function() {
      content.wrappedJSObject.foobarBug636725 = 1;
      sp.setText("++window.foobarBug636725");
    },
    then: function([code, , result]) {
      is(code, sp.getText(), "code is correct");
      is(result, content.wrappedJSObject.foobarBug636725,
         "result is correct");

      is(sp.getText(), "++window.foobarBug636725",
         "run() does not change the editor content");

      is(content.wrappedJSObject.foobarBug636725, 2,
         "run() updated window.foobarBug636725");
    }
  },
  {
    method: "display",
    prepare: function() {},
    then: function() {
      is(content.wrappedJSObject.foobarBug636725, 3,
         "display() updated window.foobarBug636725");

      is(sp.getText(), "++window.foobarBug636725\n/*\n3\n*/",
         "display() shows evaluation result in the textbox");

      is(sp.selectedText, "\n/*\n3\n*/", "selectedText is correct");
    }
  },
  {
    method: "run",
    prepare: function() {
      let selection = sp.getSelectionRange();
      is(selection.start, 24, "selection.start is correct");
      is(selection.end, 32, "selection.end is correct");

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
    },
    then: function([code, , result]) {
      is(code, "window.foobarBug636725 = 'a';", "code is correct");
      is(result, "a", "result is correct");

      is(sp.getText(), "window.foobarBug636725 = 'a';\n" +
                       "window.foobarBug636725 = 'b';",
         "run() does not change the textbox value");

      is(content.wrappedJSObject.foobarBug636725, "a",
         "run() worked for the selected range");
    }
  },
  {
    method: "display",
    prepare: function() {
      sp.setText("window.foobarBug636725 = 'c';\n" +
                 "window.foobarBug636725 = 'b';");

      sp.selectRange(0, 22);
    },
    then: function() {
      is(content.wrappedJSObject.foobarBug636725, "a",
         "display() worked for the selected range");

      is(sp.getText(), "window.foobarBug636725" +
                       "\n/*\na\n*/" +
                       " = 'c';\n" +
                       "window.foobarBug636725 = 'b';",
         "display() shows evaluation result in the textbox");

      is(sp.selectedText, "\n/*\na\n*/", "selectedText is correct");
    }
  }]


  runAsyncCallbackTests(sp, tests).then(function() {
    let selection = sp.getSelectionRange();
    is(selection.start, 22, "selection.start is correct");
    is(selection.end, 30, "selection.end is correct");

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

    finish();
  });
}
