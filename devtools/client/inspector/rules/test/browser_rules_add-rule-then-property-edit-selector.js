/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the behaviour of adding a new rule to the rule view, adding a new
// property and editing the selector.

const TEST_URI = `
  <style type="text/css">
    #testid {
      text-align: center;
    }
  </style>
  <div id="testid">Styled Node</div>
  <span>This is a span</span>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);

  await addNewRuleAndDismissEditor(inspector, view, "#testid", 1);

  info("Adding a new property to the new rule");
  await testAddingProperty(view, 1);

  info("Editing existing selector field");
  await testEditSelector(view, "span");

  info("Selecting the modified element");
  await selectNode("span", inspector);

  info("Check new rule and property exist in the modified element");
  await checkModifiedElement(view, "span", 1);
});

function testAddingProperty(view, index) {
  const ruleEditor = getRuleViewRuleEditor(view, index);
  ruleEditor.addProperty("font-weight", "bold", "", true);
  const textProps = ruleEditor.rule.textProps;
  const lastRule = textProps[textProps.length - 1];
  is(lastRule.name, "font-weight", "Last rule name is font-weight");
  is(lastRule.value, "bold", "Last rule value is bold");
}

async function testEditSelector(view, name) {
  const idRuleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  const editor = await focusEditableField(view, idRuleEditor.selectorText);

  is(inplaceEditor(idRuleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Entering a new selector name: " + name);
  editor.input.value = name;

  info("Waiting for rule view to update");
  const onRuleViewChanged = once(view, "ruleview-changed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  is(view._elementStyle.rules.length, 3, "Should have 3 rules.");
}

function checkModifiedElement(view, name, index) {
  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");

  const idRuleEditor = getRuleViewRuleEditor(view, index);
  const textProps = idRuleEditor.rule.textProps;
  const lastRule = textProps[textProps.length - 1];
  is(lastRule.name, "font-weight", "Last rule name is font-weight");
  is(lastRule.value, "bold", "Last rule value is bold");
}
