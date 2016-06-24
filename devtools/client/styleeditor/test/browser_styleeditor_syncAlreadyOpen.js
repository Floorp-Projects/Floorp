/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that changes in the style inspector are synchronized into the
// style editor.

const TESTCASE_URI = TEST_BASE_HTTP + "sync.html";

const expectedText = `
  body {
    border-width: 15px;
    color: red;
  }

  #testid {
    /*! font-size: 4em; */
  }
  `;

add_task(function* () {
  yield addTab(TESTCASE_URI);

  let { inspector, view, toolbox } = yield openRuleView();

  // In this test, make sure the style editor is open before making
  // changes in the inspector.
  let { ui } = yield openStyleEditor();
  let editor = yield ui.editors[0].getSourceEditor();

  let onEditorChange = defer();
  editor.sourceEditor.on("change", onEditorChange.resolve);

  yield toolbox.getPanel("inspector");
  yield selectNode("#testid", inspector);
  let ruleEditor = getRuleViewRuleEditor(view, 1);

  // Disable the "font-size" property.
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let onModification = view.once("ruleview-changed");
  propEditor.enable.click();
  yield onModification;

  yield openStyleEditor();
  yield onEditorChange.promise;

  let text = editor.sourceEditor.getText();
  is(text, expectedText, "style inspector changes are synced");
});
