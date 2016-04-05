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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("div", inspector);

  // Add a property to the element's style declaration, add some text,
  // then press escape.

  let elementRuleEditor = getRuleViewRuleEditor(view, 1);
  let editor = yield focusNewRuleViewProperty(elementRuleEditor);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor,
    "Next focused editor should be the new property editor.");

  EventUtils.sendString("background", view.styleWindow);

  let onBlur = once(editor.input, "blur");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield onBlur;

  is(elementRuleEditor.rule.textProps.length, 1,
    "Should have canceled creating a new text property.");
  is(view.styleDocument.documentElement, view.styleDocument.activeElement,
    "Correct element has focus");
});
