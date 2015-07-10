/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test original value is correctly displayed when ESCaping out of the
// inplace editor in the style inspector.

const originalValue = "#00F";

// Test data format
// {
//  value: what char sequence to type,
//  commitKey: what key to type to "commit" the change,
//  modifiers: commitKey modifiers,
//  expected: what value is expected as a result
// }
const testData = [
  {value: "red", commitKey: "VK_ESCAPE", modifiers: {}, expected: originalValue},
  {value: "red", commitKey: "VK_RETURN", modifiers: {}, expected: "red"},
  {value: "invalid", commitKey: "VK_RETURN", modifiers: {}, expected: "invalid"},
  {value: "blue", commitKey: "VK_TAB", modifiers: {shiftKey: true}, expected: "blue"}
];

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,test escaping property change reverts back to original value");

  info("Creating the test document");
  createDocument();

  info("Opening the rule view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test node");
  yield selectNode("#testid", inspector);

  info("Iterating over the test data");
  for (let data of testData) {
    yield runTestData(view, data);
  }
});

function createDocument() {
  let style = '' +
    '#testid {' +
    '  color: ' + originalValue + ';' +
    '}';

  let node = content.document.createElement('style');
  node.setAttribute("type", "text/css");
  node.textContent = style;
  content.document.getElementsByTagName("head")[0].appendChild(node);

  content.document.body.innerHTML = '<div id="testid">Styled Node</div>';
}

function* runTestData(view, {value, commitKey, modifiers, expected}) {
  let idRuleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = idRuleEditor.rule.textProps[0].editor;

  info("Focusing the inplace editor field");

  let editor = yield focusEditableField(view, propEditor.valueSpan);
  is(inplaceEditor(propEditor.valueSpan), editor, "Focused editor should be the value span.");

  info("Entering test data " + value);
  let onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.sendString(value, view.doc.defaultView);

  info("Waiting for focus on the field");
  let onBlur = once(editor.input, "blur");

  info("Entering the commit key " + commitKey + " " + modifiers);
  EventUtils.synthesizeKey(commitKey, modifiers);
  yield onBlur;
  // No matter if we escape or commit the change, the preview throttle is going
  // to update the property value
  yield onRuleViewChanged;

  if (commitKey === "VK_ESCAPE") {
    is(propEditor.valueSpan.textContent, expected, "Value is as expected: " + expected);
  } else {
    is(propEditor.valueSpan.textContent, expected, "Value is as expected: " + expected);
  }
}
