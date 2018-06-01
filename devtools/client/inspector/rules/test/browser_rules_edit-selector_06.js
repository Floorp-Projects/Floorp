/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing selector inplace-editor behaviors in the rule-view with unmatched
// selectors

const TEST_URI = `
  <style type="text/css">
    .testclass {
      text-align: center;
    }
    div {
    }
  </style>
  <div class="testclass">Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode(".testclass", inspector);
  await testEditClassSelector(view);
  await testEditDivSelector(view);
});

async function testEditClassSelector(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  const editor = await focusEditableField(view, ruleEditor.selectorText);

  editor.input.value = "body";
  const onRuleViewChanged = once(view, "ruleview-changed");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  // Get the new rule editor that replaced the original
  ruleEditor = getRuleViewRuleEditor(view, 1);
  const propEditor = ruleEditor.rule.textProps[0].editor;

  info("Check that the correct rules are visible");
  is(view._elementStyle.rules.length, 3, "Should have 3 rules.");
  ok(ruleEditor.element.getAttribute("unmatched"), "Rule editor is unmatched.");
  is(getRuleViewRule(view, ".testclass"), undefined,
    "Rule with .testclass selector should not exist.");
  ok(getRuleViewRule(view, "body"),
    "Rule with body selector exists.");
  is(inplaceEditor(propEditor.nameSpan),
     inplaceEditor(view.styleDocument.activeElement),
     "Focus should have moved to the property name.");
}

async function testEditDivSelector(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 2);
  const editor = await focusEditableField(view, ruleEditor.selectorText);

  editor.input.value = "asdf";
  const onRuleViewChanged = once(view, "ruleview-changed");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  // Get the new rule editor that replaced the original
  ruleEditor = getRuleViewRuleEditor(view, 2);

  info("Check that the correct rules are visible");
  is(view._elementStyle.rules.length, 3, "Should have 3 rules.");
  ok(ruleEditor.element.getAttribute("unmatched"), "Rule editor is unmatched.");
  is(getRuleViewRule(view, "div"), undefined,
    "Rule with div selector should not exist.");
  ok(getRuleViewRule(view, "asdf"),
    "Rule with asdf selector exists.");
  is(inplaceEditor(ruleEditor.newPropSpan),
     inplaceEditor(view.styleDocument.activeElement),
     "Focus should have moved to the property name.");
}
