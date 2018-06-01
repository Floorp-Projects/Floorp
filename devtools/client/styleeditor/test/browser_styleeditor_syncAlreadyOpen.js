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

add_task(async function() {
  await addTab(TESTCASE_URI);

  const { inspector, view, toolbox } = await openRuleView();

  // In this test, make sure the style editor is open before making
  // changes in the inspector.
  const { ui } = await openStyleEditor();
  const editor = await ui.editors[0].getSourceEditor();

  const onEditorChange = new Promise(resolve => {
    editor.sourceEditor.on("change", resolve);
  });

  await toolbox.getPanel("inspector");
  await selectNode("#testid", inspector);
  const ruleEditor = getRuleViewRuleEditor(view, 1);

  // Disable the "font-size" property.
  const propEditor = ruleEditor.rule.textProps[0].editor;
  const onModification = view.once("ruleview-changed");
  propEditor.enable.click();
  await onModification;

  await openStyleEditor();
  await onEditorChange;

  const text = editor.sourceEditor.getText();
  is(text, expectedText, "style inspector changes are synced");
});
