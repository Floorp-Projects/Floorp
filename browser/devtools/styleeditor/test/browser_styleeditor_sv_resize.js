/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "simple.html";

let gOriginalWidth; // these are set by run() when gChromeWindow is ready
let gOriginalHeight;

function test()
{
  waitForExplicitFinish();

  addTabAndLaunchStyleEditorChromeWhenLoaded(function (aChrome) {
    if (aChrome.isContentAttached) {
      run(aChrome);
    } else {
      aChrome.addChromeListener({
        onContentAttach: run
      });
    }
  });

  content.location = TESTCASE_URI;
}

function run(aChrome)
{
  is(aChrome.editors.length, 2,
     "there is 2 stylesheets initially");

  aChrome.editors[0].addActionListener({
    onAttach: function onEditorAttached(aEditor) {
      let originalSourceEditor = aEditor.sourceEditor;
      aEditor.sourceEditor.setCaretOffset(4); // to check the caret is preserved

      // queue a resize to inverse aspect ratio
      // this will trigger a detach and reattach (to workaround bug 254144)
      executeSoon(function () {
        waitForFocus(function () {
          gOriginalWidth = gChromeWindow.outerWidth;
          gOriginalHeight = gChromeWindow.outerHeight;
          gChromeWindow.resizeTo(120, 480);

          executeSoon(function () {
            is(aEditor.sourceEditor, originalSourceEditor,
               "the editor still references the same SourceEditor instance");
            is(aEditor.sourceEditor.getCaretOffset(), 4,
               "the caret position has been preserved");

            // queue a resize to original aspect ratio
            waitForFocus(function () {
              gChromeWindow.resizeTo(gOriginalWidth, gOriginalHeight);
              executeSoon(function () {
                finish();
              });
            }, gChromeWindow);
          });
        }, gChromeWindow);
      });
    }
  });
}
