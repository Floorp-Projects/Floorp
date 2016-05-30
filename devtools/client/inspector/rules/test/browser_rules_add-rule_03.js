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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  yield addNewRuleAndDismissEditor(inspector, view, "#testid", 1);

  info("Adding a new property to the new rule");
  yield testAddingProperty(view, 1);

  info("Editing existing selector field");
  yield testEditSelector(view, "span");

  info("Selecting the modified element");
  yield selectNode("span", inspector);

  info("Check new rule and property exist in the modified element");
  yield checkModifiedElement(view, "span", 1);
});

function* testAddingProperty(view, index) {
  let ruleEditor = getRuleViewRuleEditor(view, index);
  ruleEditor.addProperty("font-weight", "bold", "");
  let textProps = ruleEditor.rule.textProps;
  let lastRule = textProps[textProps.length - 1];
  is(lastRule.name, "font-weight", "Last rule name is font-weight");
  is(lastRule.value, "bold", "Last rule value is bold");
}

function* testEditSelector(view, name) {
  let idRuleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  let editor = yield focusEditableField(view, idRuleEditor.selectorText);

  is(inplaceEditor(idRuleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Entering a new selector name: " + name);
  editor.input.value = name;

  info("Waiting for rule view to update");
  let onRuleViewChanged = once(view, "ruleview-changed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onRuleViewChanged;

  is(view._elementStyle.rules.length, 3, "Should have 3 rules.");
}

function* checkModifiedElement(view, name, index) {
  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");

  let idRuleEditor = getRuleViewRuleEditor(view, index);
  let textProps = idRuleEditor.rule.textProps;
  let lastRule = textProps[textProps.length - 1];
  is(lastRule.name, "font-weight", "Last rule name is font-weight");
  is(lastRule.value, "bold", "Last rule value is bold");
}
