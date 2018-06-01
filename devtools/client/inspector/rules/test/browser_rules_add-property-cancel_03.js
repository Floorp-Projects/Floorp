/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests adding a new property and escapes the property name editor with a
// value.

const TEST_URI = `
  <style type='text/css'>
    div {
      background-color: blue;
    }
  </style>
  <div>Test node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("div", inspector);

  // Add a property to the element's style declaration, add some text,
  // then press escape.

  const elementRuleEditor = getRuleViewRuleEditor(view, 1);
  const editor = await focusNewRuleViewProperty(elementRuleEditor);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor,
    "Next focused editor should be the new property editor.");

  EventUtils.sendString("background", view.styleWindow);

  const onBlur = once(editor.input, "blur");
  EventUtils.synthesizeKey("KEY_Escape");
  await onBlur;

  is(elementRuleEditor.rule.textProps.length, 1,
    "Should have canceled creating a new text property.");
  is(view.styleDocument.activeElement, view.styleDocument.body,
    "Correct element has focus");
});
