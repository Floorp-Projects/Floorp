/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing various inplace-editor behaviors in the rule-view
// FIXME: To be split in several test files, and some of the inplace-editor
// focus/blur/commit/revert stuff should be factored out in head.js

let TEST_URL = 'url("' + TEST_URL_ROOT + 'test-image.png")';
let PAGE_CONTENT = [
  '<style type="text/css">',
  '  #testid {',
  '    background-color: blue;',
  '  }',
  '  .testclass {',
  '    background-color: green;',
  '  }',
  '</style>',
  '<div id="testid" class="testclass">Styled Node</div>'
].join("\n");

let test = asyncTest(function*() {
  yield addTab("data:text/html,test rule view user changes");

  info("Creating the test document");
  content.document.body.innerHTML = PAGE_CONTENT;

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode("#testid", inspector);

  yield testCancelNew(view);
  yield testCreateNew(view);
  yield testCreateNewEscape(view);
  yield testEditProperty(view, "border-color", "red");
  yield testEditProperty(view, "background-image", TEST_URL);
});

function* testCancelNew(view) {
  info("Test adding a new rule to the element's style declaration and leaving it empty.");

  let elementRuleEditor = view.element.children[0]._ruleEditor;

  info("Focusing a new property name in the rule-view");
  let editor = yield focusEditableField(elementRuleEditor.closeBrace);
  is(inplaceEditor(elementRuleEditor.newPropSpan), editor, "The new property editor got focused");

  info("Bluring the editor input");
  let onBlur = once(editor.input, "blur");
  editor.input.blur();
  yield onBlur;

  info("Checking the state of canceling a new property name editor");
  ok(!elementRuleEditor.rule._applyingModifications, "Shouldn't have an outstanding request after a cancel.");
  is(elementRuleEditor.rule.textProps.length,  0, "Should have canceled creating a new text property.");
  ok(!elementRuleEditor.propertyList.hasChildNodes(), "Should not have any properties.");
}

function* testCreateNew(view) {
  info("Test creating a new property");

  let elementRuleEditor = view.element.children[0]._ruleEditor;

  info("Focusing a new property name in the rule-view");
  let editor = yield focusEditableField(elementRuleEditor.closeBrace);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor, "The new property editor got focused");
  let input = editor.input;

  info("Entering background-color in the property name editor");
  input.value = "background-color";

  info("Pressing return to commit and focus the new value field");
  let onValueFocus = once(elementRuleEditor.element, "focus", true);
  let onModifications = elementRuleEditor.rule._applyingModifications;
  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);
  yield onValueFocus;
  yield onModifications;

  // Getting the new value editor after focus
  editor = inplaceEditor(view.doc.activeElement);
  let textProp = elementRuleEditor.rule.textProps[0];

  is(elementRuleEditor.rule.textProps.length,  1, "Created a new text property.");
  is(elementRuleEditor.propertyList.children.length, 1, "Created a property editor.");
  is(editor, inplaceEditor(textProp.editor.valueSpan), "Editing the value span now.");

  info("Entering a value and bluring the field to expect a rule change");
  editor.input.value = "#XYZ";
  let onBlur = once(editor.input, "blur");
  let onModifications = elementRuleEditor.rule._applyingModifications;
  editor.input.blur();
  yield onBlur;
  yield onModifications;

  is(textProp.value, "#XYZ", "Text prop should have been changed.");
  is(textProp.editor.isValid(), false, "#XYZ should not be a valid entry");
}

function* testCreateNewEscape(view) {
  info("Test creating a new property and escaping");

  let elementRuleEditor = view.element.children[0]._ruleEditor;

  info("Focusing a new property name in the rule-view");
  let editor = yield focusEditableField(elementRuleEditor.closeBrace);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor, "The new property editor got focused.");
  let input = editor.input;

  info("Entering a value in the property name editor");
  input.value = "color";

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
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);

  info("Checking that the previous field is focused");
  let focusedElement = inplaceEditor(elementRuleEditor.rule.textProps[0].editor.valueSpan).input;
  is(focusedElement, focusedElement.ownerDocument.activeElement, "Correct element has focus");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);

  is(elementRuleEditor.rule.textProps.length,  1, "Removed the new text property.");
  is(elementRuleEditor.propertyList.children.length, 1, "Removed the property editor.");
}

function* testEditProperty(view, name, value) {
  info("Test editing existing property name/value fields");

  let idRuleEditor = view.element.children[1]._ruleEditor;
  let propEditor = idRuleEditor.rule.textProps[0].editor;

  info("Focusing an existing property name in the rule-view");
  let editor = yield focusEditableField(propEditor.nameSpan, 32, 1);

  is(inplaceEditor(propEditor.nameSpan), editor, "The property name editor got focused");
  let input = editor.input;

  info("Entering a new property name, including : to commit and focus the value");
  let onValueFocus = once(idRuleEditor.element, "focus", true);
  let onModifications = idRuleEditor.rule._applyingModifications;
  for (let ch of name + ":") {
    EventUtils.sendChar(ch, view.doc.defaultView);
  }
  yield onValueFocus;
  yield onModifications;

  // Getting the value editor after focus
  editor = inplaceEditor(view.doc.activeElement);
  input = editor.input;
  is(inplaceEditor(propEditor.valueSpan), editor, "Focus moved to the value.");

  info("Entering a new value, including ; to commit and blur the value");
  let onBlur = once(input, "blur");
  let onModifications = idRuleEditor.rule._applyingModifications;
  for (let ch of value + ";") {
    EventUtils.sendChar(ch, view.doc.defaultView);
  }
  yield onBlur;
  yield onModifications;

  let propValue = idRuleEditor.rule.domRule._rawStyle().getPropertyValue(name);
  is(propValue, value, name + " should have been set.");
  is(propEditor.isValid(), true, value + " should be a valid entry");
}
