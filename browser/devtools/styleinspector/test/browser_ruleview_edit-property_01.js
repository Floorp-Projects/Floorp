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

  yield testEditProperty(view, "border-color", "red");
  yield testEditProperty(view, "background-image", TEST_URL);
});

function* testEditProperty(view, name, value) {
  info("Test editing existing property name/value fields");

  let idRuleEditor = getRuleViewRuleEditor(view, 1);
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
