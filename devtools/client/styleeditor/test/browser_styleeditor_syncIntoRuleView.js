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

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  info("Open the inspector and select the node we want to add style to");
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  info("Open the StyleEditor");
  const { panel, ui } = await openStyleEditor();

  const editor = await ui.editors[0].getSourceEditor();
  await new Promise(res => waitForFocus(res, panel.panelWindow));

  info("Type new rule in stylesheet");
  editor.focus();
  EventUtils.sendString(TESTCASE_CSS_SOURCE, panel.panelWindow);
  ok(editor.unsaved, "new editor has unsaved flag");

  info("Wait for ruleview to update");
  await inspector.toolbox.selectTool("inspector");
  await waitFor(() => getRuleViewRule(view, "#testid"));

  info("Check that edits were synced to rule view");
  const value = getRuleViewPropertyValue(view, "#testid", "color");
  is(value, "chartreuse", "Got the expected color property");
});
