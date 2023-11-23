/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that emptying out an existing value removes the property and
// doesn't cause any other issues.  See also Bug 1150780.

const TEST_URI = `
  <style type="text/css">
  #testid {
    color: red;
    background-color: blue;
    font-size: 12px;
  }
  .testclass, .unmatched {
    background-color: green;
  }
  </style>
  <div id="testid" class="testclass">Styled Node</div>
  <div id="testid2">Styled Node</div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const propEditor = getTextProperty(view, 1, {
    "background-color": "blue",
  }).editor;

  await focusEditableField(view, propEditor.valueSpan);

  info("Deleting all the text out of a value field");
  let onRuleViewChanged = view.once("ruleview-changed");
  await sendKeysAndWaitForFocus(view, ruleEditor.element, ["DELETE", "TAB"]);
  await onRuleViewChanged;

  info("Pressing enter a couple times to cycle through editors");
  await sendKeysAndWaitForFocus(view, ruleEditor.element, ["TAB"]);
  onRuleViewChanged = view.once("ruleview-changed");
  await sendKeysAndWaitForFocus(view, ruleEditor.element, ["TAB"]);
  await onRuleViewChanged;

  isnot(propEditor.nameSpan.style.display, "none", "The name span is visible");
  is(ruleEditor.rule.textProps.length, 2, "Correct number of props");
});
