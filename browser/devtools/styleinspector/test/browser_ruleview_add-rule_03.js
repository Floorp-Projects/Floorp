/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the behaviour of adding a new rule to the rule view, adding a new
// property and editing the selector

let PAGE_CONTENT = [
  '<style type="text/css">',
  '  #testid {',
  '    text-align: center;',
  '  }',
  '</style>',
  '<div id="testid">Styled Node</div>',
  '<span>This is a span</span>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(PAGE_CONTENT));

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode("#testid", inspector);

  info("Waiting for rule view to change");
  let onRuleViewChanged = once(view, "ruleview-changed");

  info("Adding the new rule");
  view.addRuleButton.click();

  yield onRuleViewChanged;

  info("Adding new properties to the new rule");
  yield testNewRule(view, "#testid", 1);

  info("Editing existing selector field");
  yield testEditSelector(view, "span");

  info("Selecting the modified element");
  yield selectNode("span", inspector);

  info("Check new rule and property exist in the modified element");
  yield checkModifiedElement(view, "span", 1);
});

function* testNewRule(view, expected, index) {
  let idRuleEditor = getRuleViewRuleEditor(view, index);
  let editor = idRuleEditor.selectorText.ownerDocument.activeElement;
  is(editor.value, expected,
      "Selector editor value is as expected: " + expected);

  info("Entering the escape key");
  EventUtils.synthesizeKey("VK_ESCAPE", {});

  is(idRuleEditor.selectorText.textContent, expected,
      "Selector text value is as expected: " + expected);

  info("Adding new properties to new rule: " + expected)
  idRuleEditor.addProperty("font-weight", "bold", "");
  let textProps = idRuleEditor.rule.textProps;
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
