/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for newly added
// property.

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

  info("Start entering a new property in the rule");
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const rule = ruleEditor.rule;
  let editor = await focusNewRuleViewProperty(ruleEditor);

  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(rule.textProps[0].editor.container.classList
    .contains("ruleview-highlight"),
    "width text property is correctly highlighted.");
  ok(!rule.textProps[1].editor.container.classList
    .contains("ruleview-highlight"),
    "height text property is not highlighted.");

  info("Test creating a new property");

  info("Entering margin-left in the property name editor");
  // Changing the value doesn't cause a rule-view refresh, no need to wait for
  // ruleview-changed here.
  editor.input.value = "margin-left";

  info("Pressing return to commit and focus the new value field");
  let onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  await onRuleViewChanged;

  // Getting the new value editor after focus
  editor = inplaceEditor(view.styleDocument.activeElement);
  const propEditor = ruleEditor.rule.textProps[2].editor;

  info("Entering a value and bluring the field to expect a rule change");
  onRuleViewChanged = view.once("ruleview-changed");
  editor.input.value = "100%";
  view.debounce.flush();
  await onRuleViewChanged;

  onRuleViewChanged = view.once("ruleview-changed");
  editor.input.blur();
  await onRuleViewChanged;

  ok(propEditor.container.classList.contains("ruleview-highlight"),
    "margin-left text property is correctly highlighted.");
});
