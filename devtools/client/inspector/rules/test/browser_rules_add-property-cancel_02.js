/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests adding a new property and escapes the new empty property value editor.

const TEST_URI = `
  <style type='text/css'>
    #testid {
      background-color: blue;
    }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  info("Test creating a new property and escaping");

  let elementRuleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing a new property name in the rule-view");
  let editor = yield focusEditableField(view, elementRuleEditor.closeBrace);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor,
    "The new property editor got focused.");

  info("Entering a value in the property name editor");
  editor.input.value = "color";

  info("Pressing return to commit and focus the new value field");
  let onValueFocus = once(elementRuleEditor.element, "focus", true);
  let onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onValueFocus;
  yield onRuleViewChanged;

  // Getting the new value editor after focus
  editor = inplaceEditor(view.styleDocument.activeElement);
  let textProp = elementRuleEditor.rule.textProps[1];

  is(elementRuleEditor.rule.textProps.length, 2,
    "Created a new text property.");
  is(elementRuleEditor.propertyList.children.length, 2,
    "Created a property editor.");
  is(editor, inplaceEditor(textProp.editor.valueSpan),
    "Editing the value span now.");

  info("Entering a property value");
  editor.input.value = "red";

  info("Escaping out of the field");
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  yield onRuleViewChanged;

  is(view.styleDocument.body, view.styleDocument.activeElement,
    "Correct element has focus");

  is(elementRuleEditor.rule.textProps.length, 1,
    "Removed the new text property.");
  is(elementRuleEditor.propertyList.children.length, 1,
    "Removed the property editor.");
});
