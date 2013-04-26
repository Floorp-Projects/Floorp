/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "minified.html";

let gUI;

function test()
{
  waitForExplicitFinish();

  addTabAndOpenStyleEditor(function(panel) {
    gUI = panel.UI;
    gUI.on("editor-added", function(event, editor) {
      editor.getSourceEditor().then(function() {
        testEditor(editor);
      });
    });
  });

  content.location = TESTCASE_URI;
}

let editorTestedCount = 0;
function testEditor(aEditor)
{
  if (aEditor.styleSheet.styleSheetIndex == 0) {
    let prettifiedSource = "body\{\r?\n\tbackground\:white;\r?\n\}\r?\n\r?\ndiv\{\r?\n\tfont\-size\:4em;\r?\n\tcolor\:red\r?\n\}\r?\n";
    let prettifiedSourceRE = new RegExp(prettifiedSource);

    ok(prettifiedSourceRE.test(aEditor.sourceEditor.getText()),
       "minified source has been prettified automatically");
    editorTestedCount++;
    let summary = gUI.editors[1].summary;
    EventUtils.synthesizeMouseAtCenter(summary, {}, gPanelWindow);
  }

  if (aEditor.styleSheet.styleSheetIndex == 1) {
    let originalSource = "body \{ background\: red; \}\r?\ndiv \{\r?\nfont\-size\: 5em;\r?\ncolor\: red\r?\n\}";
    let originalSourceRE = new RegExp(originalSource);

    ok(originalSourceRE.test(aEditor.sourceEditor.getText()),
       "non-minified source has been left untouched");
    editorTestedCount++;
  }

  if (editorTestedCount == 2) {
    gUI = null;
    finish();
  }
}
