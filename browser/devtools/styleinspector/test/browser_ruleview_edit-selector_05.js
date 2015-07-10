/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that adding a new property of an unmatched rule works properly.

let TEST_URI = [
  '<style type="text/css">',
  '  #testid {',
  '  }',
  '  .testclass {',
  '    background-color: white;',
  '  }',
  '</style>',
  '<div id="testid">Styled Node</div>',
  '<span class="testclass">This is a span</span>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode("#testid", inspector);
  yield testEditSelector(view, "span");
  yield testAddProperty(view);

  info("Selecting the modified element with the new rule");
  yield selectNode("span", inspector);
  yield checkModifiedElement(view, "span");
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

  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
  ok(getRuleViewRuleEditor(view, 1).element.getAttribute("unmatched"),
    "Rule with " + name + " does not match the current element.");
}

function* checkModifiedElement(view, name) {
  is(view._elementStyle.rules.length, 3, "Should have 3 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
}

function* testAddProperty(view) {
  info("Test creating a new property");

  let ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing a new property name in the rule-view");
  let editor = yield focusEditableField(view, ruleEditor.closeBrace);

  is(inplaceEditor(ruleEditor.newPropSpan), editor,
    "The new property editor got focused");
  let input = editor.input;

  info("Entering text-align in the property name editor");
  input.value = "text-align";

  info("Pressing return to commit and focus the new value field");
  let onValueFocus = once(ruleEditor.element, "focus", true);
  let onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);
  yield onValueFocus;
  yield onRuleViewChanged;

  // Getting the new value editor after focus
  editor = inplaceEditor(view.doc.activeElement);
  let textProp = ruleEditor.rule.textProps[0];

  is(ruleEditor.rule.textProps.length,  1, "Created a new text property.");
  is(ruleEditor.propertyList.children.length, 1, "Created a property editor.");
  is(editor, inplaceEditor(textProp.editor.valueSpan),
    "Editing the value span now.");

  info("Entering a value and bluring the field to expect a rule change");
  editor.input.value = "center";
  let onBlur = once(editor.input, "blur");
  onRuleViewChanged = view.once("ruleview-changed");
  editor.input.blur();
  yield onBlur;
  yield onRuleViewChanged;

  is(textProp.value, "center", "Text prop should have been changed.");
  is(textProp.overridden, false, "Property should not be overridden");
}
