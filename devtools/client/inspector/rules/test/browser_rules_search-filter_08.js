/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for newly modified
// property value.

const SEARCH = "100%";

const TEST_URI = `
  <style type='text/css'>
    #testid {
      width: 100%;
      height: 50%;
    }
  </style>
  <h1 id='testid'>Styled Node</h1>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);

  info("Enter the test value in the search filter");
  await setSearchFilter(view, SEARCH);

  info("Focus the height property value");
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const rule = ruleEditor.rule;
  const propEditor = rule.textProps[1].editor;
  await focusEditableField(view, propEditor.valueSpan);

  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(rule.textProps[0].editor.container.classList
    .contains("ruleview-highlight"),
    "width text property is correctly highlighted.");
  ok(!propEditor.container.classList.contains("ruleview-highlight"),
    "height text property is not highlighted.");

  info("Change the height property value to 100%");
  const onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.sendString("100%", view.styleWindow);
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  ok(propEditor.container.classList.contains("ruleview-highlight"),
    "height text property is correctly highlighted.");
});
