/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedBrowser.addEventListener("load", function () {
    openScratchpad(runTests);
  }, {capture: true, once: true});

  content.location = "data:text/html,<p>test run() and display() in Scratchpad";
}

function runTests() {
  let sp = gScratchpadWindow.Scratchpad;
  let tests = [{
    method: "run",
    prepare: function* () {
      yield inContent(function* () {
        content.wrappedJSObject.foobarBug636725 = 1;
      });
      sp.editor.setText("++window.foobarBug636725");
    },
    then: function* ([code, , result]) {
      is(code, sp.getText(), "code is correct");

      let pageResult = yield inContent(function* () {
        return content.wrappedJSObject.foobarBug636725;
      });
      is(result, pageResult,
         "result is correct");

      is(sp.getText(), "++window.foobarBug636725",
         "run() does not change the editor content");

      is(pageResult, 2, "run() updated window.foobarBug636725");
    }
  }, {
    method: "display",
    prepare: function* () {},
    then: function* () {
      let pageResult = yield inContent(function* () {
        return content.wrappedJSObject.foobarBug636725;
      });
      is(pageResult, 3, "display() updated window.foobarBug636725");

      is(sp.getText(), "++window.foobarBug636725\n/*\n3\n*/",
         "display() shows evaluation result in the textbox");

      is(sp.editor.getSelection(), "\n/*\n3\n*/", "getSelection is correct");
    }
  }, {
    method: "run",
    prepare: function* () {
      sp.editor.setText("window.foobarBug636725 = 'a';\n" +
        "window.foobarBug636725 = 'b';");
      sp.editor.setSelection({ line: 0, ch: 0 }, { line: 0, ch: 29 });
    },
    then: function* ([code, , result]) {
      is(code, "window.foobarBug636725 = 'a';", "code is correct");
      is(result, "a", "result is correct");

      is(sp.getText(), "window.foobarBug636725 = 'a';\n" +
                       "window.foobarBug636725 = 'b';",
         "run() does not change the textbox value");

      let pageResult = yield inContent(function* () {
        return content.wrappedJSObject.foobarBug636725;
      });
      is(pageResult, "a", "run() worked for the selected range");
    }
  }, {
    method: "display",
    prepare: function* () {
      sp.editor.setText("window.foobarBug636725 = 'c';\n" +
                 "window.foobarBug636725 = 'b';");
      sp.editor.setSelection({ line: 0, ch: 0 }, { line: 0, ch: 22 });
    },
    then: function* () {
      let pageResult = yield inContent(function* () {
        return content.wrappedJSObject.foobarBug636725;
      });
      is(pageResult, "a", "display() worked for the selected range");

      is(sp.getText(), "window.foobarBug636725" +
                       "\n/*\na\n*/" +
                       " = 'c';\n" +
                       "window.foobarBug636725 = 'b';",
         "display() shows evaluation result in the textbox");

      is(sp.editor.getSelection(), "\n/*\na\n*/", "getSelection is correct");
    }
  }];

  runAsyncCallbackTests(sp, tests).then(function () {
    ok(sp.editor.somethingSelected(), "something is selected");
    sp.editor.dropSelection();
    ok(!sp.editor.somethingSelected(), "something is no longer selected");
    ok(!sp.editor.getSelection(), "getSelection is empty");

    // Test undo/redo.
    sp.editor.setText("foo1");
    sp.editor.setText("foo2");
    is(sp.getText(), "foo2", "editor content updated");
    sp.undo();
    is(sp.getText(), "foo1", "undo() works");
    sp.redo();
    is(sp.getText(), "foo2", "redo() works");

    finish();
  });
}
