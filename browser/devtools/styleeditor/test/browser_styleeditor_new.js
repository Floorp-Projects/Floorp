/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "simple.html";


function test()
{
  waitForExplicitFinish();

  addTabAndLaunchStyleEditorChromeWhenLoaded(function (aChrome) {
    aChrome.addChromeListener({
      onContentAttach: run,
      onEditorAdded: testEditorAdded
    });
    if (aChrome.isContentAttached) {
      run(aChrome);
    }
  });

  content.location = TESTCASE_URI;
}

function run(aChrome)
{
  is(aChrome.editors.length, 2,
     "there is 2 stylesheets initially");
}

let gAddedCount = 0;  // to add new stylesheet after the 2 initial stylesheets
let gNewEditor;       // to make sure only one new stylesheet got created
let gCommitCount = 0; // to make sure only one Commit event is triggered

function testEditorAdded(aChrome, aEditor)
{
  gAddedCount++;
  if (gAddedCount == 2) {
    waitForFocus(function () { // create a new style sheet
      let newButton = gChromeWindow.document.querySelector(".style-editor-newButton");
      EventUtils.synthesizeMouseAtCenter(newButton, {}, gChromeWindow);
    }, gChromeWindow);
  }
  if (gAddedCount != 3) {
    return;
  }

  ok(!gNewEditor, "creating a new stylesheet triggers one EditorAdded event");
  gNewEditor = aEditor; // above test will fail if we get a duplicate event

  is(aChrome.editors.length, 3,
     "creating a new stylesheet added a new StyleEditor instance");

  let listener = {
    onAttach: function (aEditor) {
      waitForFocus(function () {
        ok(aEditor.isLoaded,
           "new editor is loaded when attached");
        ok(aEditor.hasFlag("new"),
           "new editor has NEW flag");
        ok(!aEditor.hasFlag("unsaved"),
           "new editor does not have UNSAVED flag");

        ok(aEditor.inputElement,
           "new editor has an input element attached");

        ok(aEditor.sourceEditor.hasFocus(),
           "new editor has focus");

        let summary = aChrome.getSummaryElementForEditor(aEditor);
        let ruleCount = summary.querySelector(".stylesheet-rule-count").textContent;
        is(parseInt(ruleCount), 0,
           "new editor initially shows 0 rules");

        let computedStyle = content.getComputedStyle(content.document.body, null);
        is(computedStyle.backgroundColor, "rgb(255, 255, 255)",
           "content's background color is initially white");

        for each (let c in "body{background-color:red;}") {
          EventUtils.synthesizeKey(c, {}, gChromeWindow);
        }
      }, gChromeWindow) ;
    },

    onCommit: function (aEditor) {
      gCommitCount++;

      ok(aEditor.hasFlag("new"),
         "new editor still has NEW flag");
      ok(aEditor.hasFlag("unsaved"),
         "new editor has UNSAVED flag after modification");

      let summary = aChrome.getSummaryElementForEditor(aEditor);
      let ruleCount = summary.querySelector(".stylesheet-rule-count").textContent;
      is(parseInt(ruleCount), 1,
         "new editor shows 1 rule after modification");

      let computedStyle = content.getComputedStyle(content.document.body, null);
      is(computedStyle.backgroundColor, "rgb(255, 0, 0)",
         "content's background color has been updated to red");

      executeSoon(function () {
        is(gCommitCount, 1, "received only one Commit event (throttle)");

        aEditor.removeActionListener(listener);

        gNewEditor = null;
        finish();
      });
    }
  };

  aEditor.addActionListener(listener);
  if (aEditor.sourceEditor) {
    listener.onAttach(aEditor);
  }
}
