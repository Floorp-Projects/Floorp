/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests adding a new property and escapes the new empty property name editor.

const TEST_URI = `
  <style type="text/css">
    #testid {
      background-color: blue;
    }
    .testclass {
      background-color: green;
    }
  </style>
  <div id='testid' class='testclass'>Styled Node</div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  let elementRuleEditor = getRuleViewRuleEditor(view, 0);
  let editor = yield focusNewRuleViewProperty(elementRuleEditor);
  is(inplaceEditor(elementRuleEditor.newPropSpan), editor,
    "The new property editor got focused");

  info("Escape the new property editor");
  let onBlur = once(editor.input, "blur");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  yield onBlur;

  info("Checking the state of cancelling a new property name editor");
  is(elementRuleEditor.rule.textProps.length, 0,
    "Should have cancelled creating a new text property.");
  ok(!elementRuleEditor.propertyList.hasChildNodes(),
    "Should not have any properties.");

  is(view.styleDocument.activeElement, view.styleDocument.body,
    "Correct element has focus");
});
