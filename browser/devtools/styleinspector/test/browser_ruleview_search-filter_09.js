/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for newly added
// property.

const SEARCH = "100%";

const TEST_URI = `
  <style type='text/css'>
    #testid {
      width: 100%;
      height: 50%;
    }
  </style>
  <h1 id='testid'>Styled Node</h1>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testNewPropertyFilter(inspector, view);
});

function* testNewPropertyFilter(inspector, view) {
  yield setSearchFilter(view, SEARCH);

  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let rule = ruleEditor.rule;
  let editor = yield focusEditableField(view, ruleEditor.closeBrace);

  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(rule.textProps[0].editor.container.classList
    .contains("ruleview-highlight"),
    "width text property is correctly highlighted.");
  ok(!rule.textProps[1].editor.container.classList
    .contains("ruleview-highlight"),
    "height text property is not highlighted.");

  info("Test creating a new property");

  info("Entering margin-left in the property name editor");
  editor.input.value = "margin-left";

  info("Pressing return to commit and focus the new value field");
  let onValueFocus = once(ruleEditor.element, "focus", true);
  let onModifications = ruleEditor.rule._applyingModifications;
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onValueFocus;
  yield onModifications;

  // Getting the new value editor after focus
  editor = inplaceEditor(view.styleDocument.activeElement);
  let propEditor = ruleEditor.rule.textProps[2].editor;

  info("Entering a value and bluring the field to expect a rule change");
  editor.input.value = "100%";
  let onBlur = once(editor.input, "blur");
  onModifications = ruleEditor.rule._applyingModifications;
  editor.input.blur();
  yield onBlur;
  yield onModifications;

  ok(propEditor.container.classList.contains("ruleview-highlight"),
    "margin-left text property is correctly highlighted.");
}
