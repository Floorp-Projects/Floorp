/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for newly modified
// property value.

const SEARCH = "100%";

let TEST_URI = [
  "<style type='text/css'>",
  "  #testid {",
  "    width: 100%;",
  "    height: 50%;",
  "  }",
  "</style>",
  "<h1 id='testid'>Styled Node</h1>"
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testModifyPropertyValueFilter(inspector, view);
});

function* testModifyPropertyValueFilter(inspector, view) {
  info("Setting filter text to \"" + SEARCH + "\"");

  let searchField = view.searchField;
  let onvRuleViewFiltered = inspector.once("ruleview-filtered");

  searchField.focus();
  synthesizeKeys(SEARCH, view.styleWindow);
  yield onvRuleViewFiltered;

  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let rule = ruleEditor.rule;
  let propEditor = rule.textProps[1].editor;
  let editor = yield focusEditableField(view, propEditor.valueSpan);

  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(rule.textProps[0].editor.container.classList.contains("ruleview-highlight"),
    "width text property is correctly highlighted.");
  ok(!propEditor.container.classList.contains("ruleview-highlight"),
    "height text property is not highlighted.");

  let onBlur = once(editor.input, "blur");
  let onModification = rule._applyingModifications;
  EventUtils.sendString("100%", view.styleWindow);
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onBlur;
  yield onModification;

  ok(propEditor.container.classList.contains("ruleview-highlight"),
    "height text property is correctly highlighted.");
}

