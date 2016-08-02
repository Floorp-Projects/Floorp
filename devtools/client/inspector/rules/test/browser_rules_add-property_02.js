/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test adding a valid property to a CSS rule, and navigating through the fields
// by pressing ENTER.

const TEST_URI = `
  <style type="text/css">
    #testid {
      color: blue;
    }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  info("Focus the new property name field");
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let editor = yield focusNewRuleViewProperty(ruleEditor);
  let input = editor.input;

  is(inplaceEditor(ruleEditor.newPropSpan), editor,
    "Next focused editor should be the new property editor.");
  ok(input.selectionStart === 0 && input.selectionEnd === input.value.length,
    "Editor contents are selected.");

  // Try clicking on the editor's input again, shouldn't cause trouble
  // (see bug 761665).
  EventUtils.synthesizeMouse(input, 1, 1, {}, view.styleWindow);
  input.select();

  info("Entering the property name");
  editor.input.value = "background-color";

  info("Pressing RETURN and waiting for the value field focus");
  let onNameAdded = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);

  yield onNameAdded;

  editor = inplaceEditor(view.styleDocument.activeElement);

  is(ruleEditor.rule.textProps.length, 2,
    "Should have created a new text property.");
  is(ruleEditor.propertyList.children.length, 2,
    "Should have created a property editor.");
  let textProp = ruleEditor.rule.textProps[1];
  is(editor, inplaceEditor(textProp.editor.valueSpan),
    "Should be editing the value span now.");

  info("Entering the property value");
  let onValueAdded = view.once("ruleview-changed");
  editor.input.value = "purple";
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onValueAdded;

  is(textProp.value, "purple", "Text prop should have been changed.");
});
