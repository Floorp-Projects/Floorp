/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing various inplace-editor behaviors in the rule-view
// FIXME: To be split in several test files, and some of the inplace-editor
// focus/blur/commit/revert stuff should be factored out in head.js

let TEST_URL = 'url("' + TEST_URL_ROOT + 'doc_test_image.png")';
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
  yield addTab("data:text/html;charset=utf-8,test rule view user changes");

  info("Creating the test document");
  content.document.body.innerHTML = PAGE_CONTENT;

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode("#testid", inspector);

  yield testCreateNew(view);
});

function* testCreateNew(view) {
  info("Test creating a new property");

  let elementRuleEditor = getRuleViewRuleEditor(view, 0);

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
  is(textProp.overridden, false, "Property should not be overridden");
  is(textProp.editor.isValid(), false, "#XYZ should not be a valid entry");
}
