/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests editing a property name or value and escaping will revert the
// changes and restore the original value.

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: #00F;
  }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;

  yield focusEditableField(view, propEditor.nameSpan);
  yield sendKeysAndWaitForFocus(view, ruleEditor.element,
    ["DELETE", "ESCAPE"]);

  is(propEditor.nameSpan.textContent, "background-color",
    "'background-color' property name is correctly set.");
  is((yield getComputedStyleProperty("#testid", null, "background-color")),
    "rgb(0, 0, 255)", "#00F background color is set.");

  yield focusEditableField(view, propEditor.valueSpan);
  let onValueDeleted = view.once("ruleview-changed");
  yield sendKeysAndWaitForFocus(view, ruleEditor.element,
    ["DELETE", "ESCAPE"]);
  yield onValueDeleted;

  is(propEditor.valueSpan.textContent, "#00F",
    "'#00F' property value is correctly set.");
  is((yield getComputedStyleProperty("#testid", null, "background-color")),
    "rgb(0, 0, 255)", "#00F background color is set.");
});
