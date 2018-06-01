/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that editing a selector to an unmatched rule does set up the correct
// property on the rule, and that settings property in said rule does not
// lead to overriding properties from matched rules.
// Test that having a rule with both matched and unmatched selectors does work
// correctly.

const TEST_URI = `
  <style type="text/css">
    #testid {
      color: black;
    }
    .testclass {
      background-color: white;
    }
  </style>
  <div id="testid">Styled Node</div>
  <span class="testclass">This is a span</span>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();

  await selectNode("#testid", inspector);
  await testEditSelector(view, "span");
  await testAddImportantProperty(view);
  await testAddMatchedRule(view, "span, div");
});

async function testEditSelector(view, name) {
  info("Test editing existing selector fields");

  const ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  const editor = await focusEditableField(view, ruleEditor.selectorText);

  is(inplaceEditor(ruleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Entering a new selector name and committing");
  editor.input.value = name;

  info("Waiting for rule view to update");
  const onRuleViewChanged = once(view, "ruleview-changed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
  ok(getRuleViewRuleEditor(view, 1).element.getAttribute("unmatched"),
    "Rule with " + name + " does not match the current element.");

  // Escape the new property editor after editing the selector
  const onBlur = once(view.styleDocument.activeElement, "blur");
  EventUtils.synthesizeKey("KEY_Escape", {}, view.styleWindow);
  await onBlur;
}

async function testAddImportantProperty(view) {
  info("Test creating a new property with !important");
  const textProp = await addProperty(view, 1, "color", "red !important");

  is(textProp.value, "red", "Text prop should have been changed.");
  is(textProp.priority, "important",
    "Text prop has an \"important\" priority.");
  ok(!textProp.overridden, "Property should not be overridden");

  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const prop = ruleEditor.rule.textProps[0];
  ok(!prop.overridden,
    "Existing property on matched rule should not be overridden");
}

async function testAddMatchedRule(view, name) {
  info("Test adding a matching selector");

  const ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  const editor = await focusEditableField(view, ruleEditor.selectorText);

  is(inplaceEditor(ruleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Entering a new selector name and committing");
  editor.input.value = name;

  info("Waiting for rule view to update");
  const onRuleViewChanged = once(view, "ruleview-changed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  is(getRuleViewRuleEditor(view, 1).element.getAttribute("unmatched"), "false",
    "Rule with " + name + " does match the current element.");

  // Escape the new property editor after editing the selector
  const onBlur = once(view.styleDocument.activeElement, "blur");
  EventUtils.synthesizeKey("KEY_Escape", {}, view.styleWindow);
  await onBlur;
}
