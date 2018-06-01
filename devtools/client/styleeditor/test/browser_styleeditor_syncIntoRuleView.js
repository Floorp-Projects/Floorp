/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that changes in the style editor are synchronized into the
// style inspector.

const TEST_URI = `
  <style type='text/css'>
  </style>
  <div id='testid' class='testclass'>Styled Node</div>
`;

const TESTCASE_CSS_SOURCE = "#testid { color: chartreuse; }";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);

  const { panel, ui } = await openStyleEditor();

  const editor = await ui.editors[0].getSourceEditor();

  const waitForRuleView = view.once("ruleview-refreshed");
  await typeInEditor(editor, panel.panelWindow);
  await waitForRuleView;

  const value = getRuleViewPropertyValue(view, "#testid", "color");
  is(value, "chartreuse", "check that edits were synced to rule view");
});

function typeInEditor(editor, panelWindow) {
  return new Promise(resolve => {
    waitForFocus(function() {
      for (const c of TESTCASE_CSS_SOURCE) {
        EventUtils.synthesizeKey(c, {}, panelWindow);
      }
      ok(editor.unsaved, "new editor has unsaved flag");

      resolve();
    }, panelWindow);
  });
}
