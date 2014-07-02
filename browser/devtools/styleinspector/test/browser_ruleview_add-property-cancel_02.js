/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing various inplace-editor behaviors in the rule-view

let TEST_URL = 'url("' + TEST_URL_ROOT + 'doc_test_image.png")';
let PAGE_CONTENT = [
  '<style type="text/css">',
  '  #testid {',
  '    background-color: blue;',
  '  }',
  '</style>',
  '<div id="testid">Styled Node</div>'
].join("\n");

let test = asyncTest(function*() {
  yield addTab("data:text/html,test rule view user changes");

  info("Creating the test document");
  content.document.body.innerHTML = PAGE_CONTENT;

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode("#testid", inspector);

  info("Test creating a new property and escaping");

  let elementRuleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing a new property name in the rule-view");
  let editor = yield focusEditableField(elementRuleEditor.closeBrace);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor, "The new property editor got focused.");
  let input = editor.input;

  info("Entering a value in the property name editor");
  let onModifications = elementRuleEditor.rule._applyingModifications;
  input.value = "color";
  yield onModifications;

  info("Pressing return to commit and focus the new value field");
  let onValueFocus = once(elementRuleEditor.element, "focus", true);
  let onModifications = elementRuleEditor.rule._applyingModifications;
  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);
  yield onValueFocus;
  yield onModifications;

  // Getting the new value editor after focus
  editor = inplaceEditor(view.doc.activeElement);
  let textProp = elementRuleEditor.rule.textProps[1];

  is(elementRuleEditor.rule.textProps.length,  2, "Created a new text property.");
  is(elementRuleEditor.propertyList.children.length, 2, "Created a property editor.");
  is(editor, inplaceEditor(textProp.editor.valueSpan), "Editing the value span now.");

  info("Entering a property value");
  editor.input.value = "red";

  info("Escaping out of the field");
  let onModifications = elementRuleEditor.rule._applyingModifications;
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  yield onModifications;

  info("Checking that the previous field is focused");
  let focusedElement = inplaceEditor(elementRuleEditor.rule.textProps[0].editor.valueSpan).input;
  is(focusedElement, focusedElement.ownerDocument.activeElement, "Correct element has focus");

  let onModifications = elementRuleEditor.rule._applyingModifications;
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  yield onModifications;

  is(elementRuleEditor.rule.textProps.length,  1, "Removed the new text property.");
  is(elementRuleEditor.propertyList.children.length, 1, "Removed the property editor.");
});
