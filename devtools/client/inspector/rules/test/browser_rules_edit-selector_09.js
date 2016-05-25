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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  yield selectNode("#testid", inspector);
  yield testEditSelector(view, "span");
  yield testAddImportantProperty(view);
  yield testAddMatchedRule(view, "span, div");
});

function* testEditSelector(view, name) {
  info("Test editing existing selector fields");

  let ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  let editor = yield focusEditableField(view, ruleEditor.selectorText);

  is(inplaceEditor(ruleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Entering a new selector name and committing");
  editor.input.value = name;

  info("Waiting for rule view to update");
  let onRuleViewChanged = once(view, "ruleview-changed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onRuleViewChanged;

  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
  ok(getRuleViewRuleEditor(view, 1).element.getAttribute("unmatched"),
    "Rule with " + name + " does not match the current element.");

  // Escape the new property editor after editing the selector
  let onBlur = once(view.styleDocument.activeElement, "blur");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  yield onBlur;
}

function* testAddImportantProperty(view) {
  info("Test creating a new property with !important");
  let textProp = yield addProperty(view, 1, "color", "red !important");

  is(textProp.value, "red", "Text prop should have been changed.");
  is(textProp.priority, "important",
    "Text prop has an \"important\" priority.");
  ok(!textProp.overridden, "Property should not be overridden");

  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let prop = ruleEditor.rule.textProps[0];
  ok(!prop.overridden,
    "Existing property on matched rule should not be overridden");
}

function* testAddMatchedRule(view, name) {
  info("Test adding a matching selector");

  let ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  let editor = yield focusEditableField(view, ruleEditor.selectorText);

  is(inplaceEditor(ruleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Entering a new selector name and committing");
  editor.input.value = name;

  info("Waiting for rule view to update");
  let onRuleViewChanged = once(view, "ruleview-changed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onRuleViewChanged;

  is(getRuleViewRuleEditor(view, 1).element.getAttribute("unmatched"), "false",
    "Rule with " + name + " does match the current element.");

  // Escape the new property editor after editing the selector
  let onBlur = once(view.styleDocument.activeElement, "blur");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  yield onBlur;
}
