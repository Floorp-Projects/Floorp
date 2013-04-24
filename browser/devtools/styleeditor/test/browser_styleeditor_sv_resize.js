/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "simple.html";

let gOriginalWidth; // these are set by runTests()
let gOriginalHeight;

function test()
{
  waitForExplicitFinish();

  addTabAndOpenStyleEditor(function(panel) {
    let UI = panel.UI;
    UI.on("editor-added", function(event, editor) {
      if (editor == UI.editors[1]) {
        // wait until both editors are added
        runTests(UI);
      }
    });
  });

  content.location = TESTCASE_URI;
}

function runTests(aUI)
{
  is(aUI.editors.length, 2,
     "there is 2 stylesheets initially");

  aUI.editors[0].getSourceEditor().then(function onEditorAttached(aEditor) {
    executeSoon(function () {
      waitForFocus(function () {
        // queue a resize to inverse aspect ratio
        // this will trigger a detach and reattach (to workaround bug 254144)
        let originalSourceEditor = aEditor.sourceEditor;
        aEditor.sourceEditor.setCaretOffset(4); // to check the caret is preserved

        gOriginalWidth = gPanelWindow.outerWidth;
        gOriginalHeight = gPanelWindow.outerHeight;
        gPanelWindow.resizeTo(120, 480);

        executeSoon(function () {
          is(aEditor.sourceEditor, originalSourceEditor,
             "the editor still references the same SourceEditor instance");
          is(aEditor.sourceEditor.getCaretOffset(), 4,
             "the caret position has been preserved");

          // queue a resize to original aspect ratio
          waitForFocus(function () {
            gPanelWindow.resizeTo(gOriginalWidth, gOriginalHeight);
            executeSoon(function () {
              finish();
            });
          }, gPanelWindow);
        });
      }, gPanelWindow);
    });
  });
}
