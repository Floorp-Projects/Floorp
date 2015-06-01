/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests editing SVG styles using the rules view

let TEST_URL = "chrome://global/skin/icons/warning.svg";
let TEST_SELECTOR = "path";

add_task(function*() {
  yield addTab(TEST_URL);

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode(TEST_SELECTOR, inspector);

  yield testCreateNew(view);
});

function* testCreateNew(ruleView) {
  info("Test creating a new property");

  let elementRuleEditor = getRuleViewRuleEditor(ruleView, 0);

  info("Focusing a new property name in the rule-view");
  let editor = yield focusEditableField(ruleView, elementRuleEditor.closeBrace);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor,
    "Next focused editor should be the new property editor.");

  let input = editor.input;

  info("Entering the property name");
  input.value = "fill";

  info("Pressing RETURN and waiting for the value field focus");
  let onFocus = once(elementRuleEditor.element, "focus", true);
  EventUtils.sendKey("return", ruleView.doc.defaultView);
  yield onFocus;
  yield elementRuleEditor.rule._applyingModifications;

  editor = inplaceEditor(ruleView.doc.activeElement);

  is(elementRuleEditor.rule.textProps.length,  1, "Should have created a new text property.");
  is(elementRuleEditor.propertyList.children.length, 1, "Should have created a property editor.");
  let textProp = elementRuleEditor.rule.textProps[0];
  is(editor, inplaceEditor(textProp.editor.valueSpan), "Should be editing the value span now.");

  editor.input.value = "red";
  let onBlur = once(editor.input, "blur");
  EventUtils.sendKey("return", ruleView.doc.defaultView);
  yield onBlur;
  yield elementRuleEditor.rule._applyingModifications;

  is(textProp.value, "red", "Text prop should have been changed.");

  is((yield getComputedStyleProperty(TEST_SELECTOR, null, "fill")), "rgb(255, 0, 0)", "The fill was changed to red");
}
