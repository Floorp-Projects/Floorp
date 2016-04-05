/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

//

const TEST_URI = `
  <style type="text/css">
    #testid {
      background-color: blue;
    }
    .testclass, .unmatched {
      background-color: green;
    };
  </style>
  <div id='testid' class='testclass'>Styled Node</div>
  <div id='testid2'>Styled Node</div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  info("Focus the new property name field");
  let elementRuleEditor = getRuleViewRuleEditor(view, 0);
  let editor = yield focusNewRuleViewProperty(elementRuleEditor);
  let input = editor.input;

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor,
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

  is(elementRuleEditor.rule.textProps.length, 1,
    "Should have created a new text property.");
  is(elementRuleEditor.propertyList.children.length, 1,
    "Should have created a property editor.");
  let textProp = elementRuleEditor.rule.textProps[0];
  is(editor, inplaceEditor(textProp.editor.valueSpan),
    "Should be editing the value span now.");

  info("Entering the property value");
  // We're editing inline style, so expect a style attribute mutation.
  let onMutated = inspector.once("markupmutation");
  let onValueAdded = view.once("ruleview-changed");
  editor.input.value = "purple";
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onValueAdded;
  yield onMutated;

  is(textProp.value, "purple", "Text prop should have been changed.");
});
