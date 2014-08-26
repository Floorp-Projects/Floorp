/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "simple.html";

let TESTCASE_CSS_SOURCE = "body{background-color:red;";

let gOriginalHref;
let gUI;

waitForExplicitFinish();

let test = asyncTest(function*() {
  let panel = yield addTabAndOpenStyleEditors(2, null, TESTCASE_URI);
  gUI = panel.UI;

  let editor = yield createNew();
  testInitialState(editor);

  let waitForPropertyChange = onPropertyChange(editor);

  yield typeInEditor(editor);

  yield waitForPropertyChange;

  testUpdated(editor);

  gUI = null;
});

function createNew() {
  info("Creating a new stylesheet now");
  let deferred = promise.defer();

  gUI.once("editor-added", (ev, editor) => {
    editor.getSourceEditor().then(deferred.resolve);
  });

  waitForFocus(function () {// create a new style sheet
    let newButton = gPanelWindow.document.querySelector(".style-editor-newButton");
    ok(newButton, "'new' button exists");

    EventUtils.synthesizeMouseAtCenter(newButton, {}, gPanelWindow);
  }, gPanelWindow);

  return deferred.promise;
}

function onPropertyChange(aEditor) {
  let deferred = promise.defer();

  aEditor.styleSheet.on("property-change", function onProp(property, value) {
    // wait for text to be entered fully
    let text = aEditor.sourceEditor.getText();
    if (property == "ruleCount" && text == TESTCASE_CSS_SOURCE + "}") {
      aEditor.styleSheet.off("property-change", onProp);
      deferred.resolve();
    }
  });

  return deferred.promise;
}

function testInitialState(aEditor) {
  info("Testing the initial state of the new editor");
  gOriginalHref = aEditor.styleSheet.href;

  let summary = aEditor.summary;

  ok(aEditor.sourceLoaded, "new editor is loaded when attached");
  ok(aEditor.isNew, "new editor has isNew flag");

  ok(aEditor.sourceEditor.hasFocus(), "new editor has focus");

  let summary = aEditor.summary;
  let ruleCount = summary.querySelector(".stylesheet-rule-count").textContent;
  is(parseInt(ruleCount), 0,
     "new editor initially shows 0 rules");

  let computedStyle = content.getComputedStyle(content.document.body, null);
  is(computedStyle.backgroundColor, "rgb(255, 255, 255)",
     "content's background color is initially white");
}

function typeInEditor(aEditor) {
  let deferred = promise.defer();

  waitForFocus(function () {
    for each (let c in TESTCASE_CSS_SOURCE) {
      EventUtils.synthesizeKey(c, {}, gPanelWindow);
    }
    ok(aEditor.unsaved, "new editor has unsaved flag");

    deferred.resolve();
  }, gPanelWindow);

  return deferred.promise;
}

function testUpdated(aEditor) {
  info("Testing the state of the new editor after editing it");

  is(aEditor.sourceEditor.getText(), TESTCASE_CSS_SOURCE + "}",
     "rule bracket has been auto-closed");

  let ruleCount = aEditor.summary.querySelector(".stylesheet-rule-count").textContent;
  is(parseInt(ruleCount), 1,
     "new editor shows 1 rule after modification");

  is(aEditor.styleSheet.href, gOriginalHref,
     "style sheet href did not change");
}
