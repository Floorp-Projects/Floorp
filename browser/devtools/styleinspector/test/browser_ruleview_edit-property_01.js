/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing adding new properties via the inplace-editors in the rule
// view.
// FIXME: some of the inplace-editor focus/blur/commit/revert stuff
// should be factored out in head.js

let BACKGROUND_IMAGE_URL = 'url("' + TEST_URL_ROOT + 'doc_test_image.png")';
let TEST_URI = [
  '<style type="text/css">',
  '#testid {',
  '  color: red;',
  '  background-color: blue;',
  '}',
  '.testclass, .unmatched {',
  '  background-color: green;',
  '}',
  '</style>',
  '<div id="testid" class="testclass">Styled Node</div>',
  '<div id="testid2">Styled Node</div>'
].join("\n");

let TEST_DATA = [
  { name: "border-color", value: "red", isValid: true },
  { name: "background-image", value: BACKGROUND_IMAGE_URL, isValid: true },
  { name: "border", value: "solid 1px foo", isValid: false },
];

add_task(function*() {
  let tab = yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode("#testid", inspector);

  let ruleEditor = getRuleViewRuleEditor(view, 1);
  for (let {name, value, isValid} of TEST_DATA) {
    yield testEditProperty(ruleEditor, name, value, isValid);
  }
});

function* testEditProperty(ruleEditor, name, value, isValid) {
  info("Test editing existing property name/value fields");

  let doc = ruleEditor.doc;
  let propEditor = ruleEditor.rule.textProps[0].editor;

  info("Focusing an existing property name in the rule-view");
  let editor = yield focusEditableField(propEditor.nameSpan, 32, 1);

  is(inplaceEditor(propEditor.nameSpan), editor, "The property name editor got focused");
  let input = editor.input;

  info("Entering a new property name, including : to commit and focus the value");
  let onValueFocus = once(ruleEditor.element, "focus", true);
  let onModifications = ruleEditor.rule._applyingModifications;
  for (let ch of name + ":") {
    EventUtils.sendChar(ch, doc.defaultView);
  }
  yield onValueFocus;
  yield onModifications;

  // Getting the value editor after focus
  editor = inplaceEditor(doc.activeElement);
  input = editor.input;
  is(inplaceEditor(propEditor.valueSpan), editor, "Focus moved to the value.");

  info("Entering a new value, including ; to commit and blur the value");
  let onBlur = once(input, "blur");
  onModifications = ruleEditor.rule._applyingModifications;
  for (let ch of value + ";") {
    EventUtils.sendChar(ch, doc.defaultView);
  }
  yield onBlur;
  yield onModifications;

  is(propEditor.isValid(), isValid, value + " is " + isValid ? "valid" : "invalid");

  info("Checking that the style property was changed on the content page");
  let propValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name
  });

  if (isValid) {
    is(propValue, value, name + " should have been set.");
  } else {
    isnot(propValue, value, name + " shouldn't have been set.");
  }
}