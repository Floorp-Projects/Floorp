/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changes are previewed when editing a property value.

const TEST_URI = `
  <style type="text/css">
    #testid {
      display:block;
    }
  </style>
  <div id="testid">Styled Node</div><span>inline element</span>
`;

// Format
// {
//   value : what to type in the field
//   expected : expected computed style on the targeted element
// }
const TEST_DATA = [
  {value: "inline", expected: "inline"},
  {value: "inline-block", expected: "inline-block"},

  // Invalid property values should not apply, and should fall back to default
  {value: "red", expected: "block"},
  {value: "something", expected: "block"},

  {escape: true, value: "inline", expected: "block"}
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);

  for (const data of TEST_DATA) {
    await testLivePreviewData(data, view, "#testid");
  }
});

async function testLivePreviewData(data, ruleView, selector) {
  const rule = getRuleViewRuleEditor(ruleView, 1).rule;
  const propEditor = rule.textProps[0].editor;

  info("Focusing the property value inplace-editor");
  const editor = await focusEditableField(ruleView, propEditor.valueSpan);
  is(inplaceEditor(propEditor.valueSpan), editor,
    "The focused editor is the value");

  info("Entering value in the editor: " + data.value);
  const onPreviewDone = ruleView.once("ruleview-changed");
  EventUtils.sendString(data.value, ruleView.styleWindow);
  ruleView.debounce.flush();
  await onPreviewDone;

  const onValueDone = ruleView.once("ruleview-changed");
  if (data.escape) {
    EventUtils.synthesizeKey("KEY_Escape");
  } else {
    EventUtils.synthesizeKey("KEY_Enter");
  }
  await onValueDone;

  // While the editor is still focused in, the display should have
  // changed already
  is((await getComputedStyleProperty(selector, null, "display")),
    data.expected,
    "Element should be previewed as " + data.expected);
}
