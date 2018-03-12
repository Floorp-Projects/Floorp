/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that adding a new rule is synced to the style editor.

const TESTCASE_URI = TEST_BASE_HTTP + "sync.html";

const expectedText = `
#testid {
}`;

add_task(function* () {
  yield addTab(TESTCASE_URI);
  let { inspector, view } = yield openRuleView();
  yield selectNode("#testid", inspector);

  let onRuleViewChanged = once(view, "ruleview-changed");
  view.addRuleButton.click();
  yield onRuleViewChanged;

  let { ui } = yield openStyleEditor();

  info("Selecting the second editor");
  yield ui.selectStyleSheet(ui.editors[1].styleSheet);

  let editor = ui.editors[1];
  let text = editor.sourceEditor.getText();
  is(text, expectedText, "selector edits are synced");
});
