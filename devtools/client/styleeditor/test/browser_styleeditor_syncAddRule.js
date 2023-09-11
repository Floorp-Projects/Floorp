/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that adding a new rule is synced to the style editor.

const TESTCASE_URI = TEST_BASE_HTTP + "sync.html";

const expectedText = `
#testid {
}`;

add_task(async function () {
  await addTab(TESTCASE_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  const onNewRuleAdded = once(view, "new-rule-added");
  view.addRuleButton.click();
  await onNewRuleAdded;

  const { ui } = await openStyleEditor();

  info("Selecting the second editor");
  await ui.selectStyleSheet(ui.editors[1].styleSheet);

  const editor = ui.editors[1];
  const text = editor.sourceEditor.getText();
  is(text, expectedText, "selector edits are synced");
});
