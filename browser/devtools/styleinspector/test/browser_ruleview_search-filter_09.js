/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for inline styles.

const SEARCH = "color";

let TEST_URI = [
  '<style type="text/css">',
  '  #testid {',
  '    width: 100%;',
  '  }',
  '</style>',
  '<div id="testid" style="background-color:aliceblue">Styled Node</div>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testAddTextInFilter(inspector, view);
});

function* testAddTextInFilter(inspector, ruleView) {
  info("Setting filter text to \"" + SEARCH + "\"");

  let win = ruleView.doc.defaultView;
  let searchField = ruleView.searchField;
  let onRuleViewFiltered = inspector.once("ruleview-filtered");

  searchField.focus();
  synthesizeKeys(SEARCH, win);
  yield onRuleViewFiltered;

  info("Check that the correct rules are visible");
  is(ruleView.element.children.length, 1, "Should have 1 rule.");

  let rule = getRuleViewRuleEditor(ruleView, 0).rule;

  is(rule.selectorText, "element", "First rule is inline element.");
  ok(rule.textProps[0].editor.container.classList.contains("ruleview-highlight"),
    "background-color text property is correctly highlighted.");
}
