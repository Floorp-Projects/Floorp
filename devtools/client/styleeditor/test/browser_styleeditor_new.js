/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that new sheets can be added and edited.

const TESTCASE_URI = TEST_BASE_HTTP + "simple.html";

const TESTCASE_CSS_SOURCE = "body{background-color:red;";

add_task(function* () {
  let { panel, ui } = yield openStyleEditorForURL(TESTCASE_URI);

  let editor = yield createNew(ui, panel.panelWindow);
  yield testInitialState(editor);

  let originalHref = editor.styleSheet.href;
  let waitForPropertyChange = onPropertyChange(editor);

  yield typeInEditor(editor, panel.panelWindow);

  yield waitForPropertyChange;

  testUpdated(editor, originalHref);
});

function createNew(ui, panelWindow) {
  info("Creating a new stylesheet now");
  let deferred = defer();

  ui.once("editor-added", (ev, editor) => {
    editor.getSourceEditor().then(deferred.resolve);
  });

  waitForFocus(function () {
    // create a new style sheet
    let newButton = panelWindow.document
      .querySelector(".style-editor-newButton");
    ok(newButton, "'new' button exists");

    EventUtils.synthesizeMouseAtCenter(newButton, {}, panelWindow);
  }, panelWindow);

  return deferred.promise;
}

function onPropertyChange(editor) {
  let deferred = defer();

  editor.styleSheet.on("property-change", function onProp(property) {
    // wait for text to be entered fully
    let text = editor.sourceEditor.getText();
    if (property == "ruleCount" && text == TESTCASE_CSS_SOURCE + "}") {
      editor.styleSheet.off("property-change", onProp);
      deferred.resolve();
    }
  });

  return deferred.promise;
}

function* testInitialState(editor) {
  info("Testing the initial state of the new editor");

  let summary = editor.summary;

  ok(editor.sourceLoaded, "new editor is loaded when attached");
  ok(editor.isNew, "new editor has isNew flag");

  ok(editor.sourceEditor.hasFocus(), "new editor has focus");

  summary = editor.summary;
  let ruleCount = summary.querySelector(".stylesheet-rule-count").textContent;
  is(parseInt(ruleCount, 10), 0, "new editor initially shows 0 rules");

  let color = yield getComputedStyleProperty({
    selector: "body",
    name: "background-color"
  });
  is(color, "rgb(255, 255, 255)",
     "content's background color is initially white");
}

function typeInEditor(editor, panelWindow) {
  let deferred = defer();

  waitForFocus(function () {
    for (let c of TESTCASE_CSS_SOURCE) {
      EventUtils.synthesizeKey(c, {}, panelWindow);
    }
    ok(editor.unsaved, "new editor has unsaved flag");

    deferred.resolve();
  }, panelWindow);

  return deferred.promise;
}

function testUpdated(editor, originalHref) {
  info("Testing the state of the new editor after editing it");

  is(editor.sourceEditor.getText(), TESTCASE_CSS_SOURCE + "}",
     "rule bracket has been auto-closed");

  let ruleCount = editor.summary.querySelector(".stylesheet-rule-count")
    .textContent;
  is(parseInt(ruleCount, 10), 1,
     "new editor shows 1 rule after modification");

  is(editor.styleSheet.href, originalHref,
     "style sheet href did not change");
}
