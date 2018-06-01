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

  #testid, span {
    font-size: 4em;
  }
  `;

add_task(async function() {
  await addTab(TESTCASE_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);
  const ruleEditor = getRuleViewRuleEditor(view, 1);

  let editor = await focusEditableField(view, ruleEditor.selectorText);
  editor.input.value = "#testid, span";
  const onRuleViewChanged = once(view, "ruleview-changed");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  const { ui } = await openStyleEditor();

  editor = await ui.editors[0].getSourceEditor();
  const text = editor.sourceEditor.getText();
  is(text, expectedText, "selector edits are synced");
});
