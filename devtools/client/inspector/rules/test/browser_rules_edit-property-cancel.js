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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);

  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const propEditor = ruleEditor.rule.textProps[0].editor;

  await focusEditableField(view, propEditor.nameSpan);
  await sendKeysAndWaitForFocus(view, ruleEditor.element,
    ["DELETE", "ESCAPE"]);

  is(propEditor.nameSpan.textContent, "background-color",
    "'background-color' property name is correctly set.");
  is((await getComputedStyleProperty("#testid", null, "background-color")),
    "rgb(0, 0, 255)", "#00F background color is set.");

  await focusEditableField(view, propEditor.valueSpan);
  const onValueDeleted = view.once("ruleview-changed");
  await sendKeysAndWaitForFocus(view, ruleEditor.element,
    ["DELETE", "ESCAPE"]);
  await onValueDeleted;

  is(propEditor.valueSpan.textContent, "#00F",
    "'#00F' property value is correctly set.");
  is((await getComputedStyleProperty("#testid", null, "background-color")),
    "rgb(0, 0, 255)", "#00F background color is set.");
});
