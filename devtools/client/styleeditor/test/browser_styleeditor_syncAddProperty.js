/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that adding a new rule is synced to the style editor.

const TESTCASE_URI = TEST_BASE_HTTP + "sync.html";

const expectedText = `
  body {
    border-width: 15px;
    color: red;
  }

  #testid {
    font-size: 4em;
    /*! background-color: yellow; */
  }
  `;

add_task(function* () {
  yield addTab(TESTCASE_URI);
  let { inspector, view } = yield openRuleView();
  yield selectNode("#testid", inspector);

  info("Focusing a new property name in the rule-view");
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let editor = yield focusEditableField(view, ruleEditor.closeBrace);
  is(inplaceEditor(ruleEditor.newPropSpan), editor,
    "The new property editor has focus");

  let input = editor.input;
  input.value = "/* background-color: yellow; */";

  info("Pressing return to commit and focus the new value field");
  let onModifications = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onModifications;

  let { ui } = yield openStyleEditor();
  let sourceEditor = yield ui.editors[0].getSourceEditor();
  let text = sourceEditor.sourceEditor.getText();
  is(text, expectedText, "selector edits are synced");
});
