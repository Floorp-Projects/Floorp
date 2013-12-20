/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "simple.html";

let TRANSITION_CLASS = "moz-styleeditor-transitioning";
let TESTCASE_CSS_SOURCE = "body{background-color:red;";

let gUI;

function test()
{
  waitForExplicitFinish();

  addTabAndOpenStyleEditor(function(panel) {
    gUI = panel.UI;
    gUI.on("editor-added", testEditorAdded);
  });

  content.location = TESTCASE_URI;
}

let gAddedCount = 0;  // to add new stylesheet after the 2 initial stylesheets
let gNewEditor;       // to make sure only one new stylesheet got created
let gOriginalHref;

let checksCompleted = 0;

function testEditorAdded(aEvent, aEditor)
{
  gAddedCount++;
  if (gAddedCount == 2) {
    waitForFocus(function () {// create a new style sheet
      let newButton = gPanelWindow.document.querySelector(".style-editor-newButton");
      ok(newButton, "'new' button exists");

      EventUtils.synthesizeMouseAtCenter(newButton, {}, gPanelWindow);
    }, gPanelWindow);
  }
  if (gAddedCount < 3) {
    return;
  }

  ok(!gNewEditor, "creating a new stylesheet triggers one EditorAdded event");
  gNewEditor = aEditor; // above test will fail if we get a duplicate event

  is(gUI.editors.length, 3,
     "creating a new stylesheet added a new StyleEditor instance");

  aEditor.styleSheet.once("style-applied", function() {
    // when changes have been completely applied to live stylesheet after transisiton
    ok(!content.document.documentElement.classList.contains(TRANSITION_CLASS),
       "StyleEditor's transition class has been removed from content");

    if (++checksCompleted == 3) {
      cleanup();
    }
  });

  aEditor.styleSheet.on("property-change", function(property) {
    if (property == "ruleCount") {
      let ruleCount = aEditor.summary.querySelector(".stylesheet-rule-count").textContent;
      is(parseInt(ruleCount), 1,
         "new editor shows 1 rule after modification");

      if (++checksCompleted == 3) {
        cleanup();
      }
    }
  });

  aEditor.getSourceEditor().then(testEditor);
}

function testEditor(aEditor) {
  waitForFocus(function () {
  gOriginalHref = aEditor.styleSheet.href;

  let summary = aEditor.summary;

  ok(aEditor.sourceLoaded,
     "new editor is loaded when attached");
  ok(aEditor.isNew,
     "new editor has isNew flag");

  ok(aEditor.sourceEditor.hasFocus(),
     "new editor has focus");

  let summary = aEditor.summary;
  let ruleCount = summary.querySelector(".stylesheet-rule-count").textContent;
  is(parseInt(ruleCount), 0,
     "new editor initially shows 0 rules");

  let computedStyle = content.getComputedStyle(content.document.body, null);
  is(computedStyle.backgroundColor, "rgb(255, 255, 255)",
     "content's background color is initially white");

  for each (let c in TESTCASE_CSS_SOURCE) {
    EventUtils.synthesizeKey(c, {}, gPanelWindow);
  }

  ok(aEditor.unsaved,
     "new editor has unsaved flag");

  // we know that the testcase above will start a CSS transition
  content.addEventListener("transitionend", onTransitionEnd, false);
}, gPanelWindow) ;
}

function onTransitionEnd() {
  content.removeEventListener("transitionend", onTransitionEnd, false);

  is(gNewEditor.sourceEditor.getText(), TESTCASE_CSS_SOURCE + "}",
     "rule bracket has been auto-closed");

  let computedStyle = content.getComputedStyle(content.document.body, null);
  is(computedStyle.backgroundColor, "rgb(255, 0, 0)",
     "content's background color has been updated to red");

  if (gNewEditor) {
    is(gNewEditor.styleSheet.href, gOriginalHref,
       "style sheet href did not change");
  }

  if (++checksCompleted == 3) {
    cleanup();
  }
}

function cleanup() {
  gNewEditor = null;
  gUI = null;
  finish();
}
