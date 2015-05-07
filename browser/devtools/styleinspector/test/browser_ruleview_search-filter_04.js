/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for keyframe rule selectors.

const SEARCH = "20%"
const TEST_URI = TEST_URL_ROOT + "doc_keyframeanimation.html";

add_task(function*() {
  yield addTab(TEST_URI);
  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("#boxy", inspector);
  yield testAddTextInFilter(inspector, view);
});

function* testAddTextInFilter(inspector, ruleView) {
  info("Setting filter text to \"" + SEARCH + "\"");

  let win = ruleView.doc.defaultView;
  let searchField = ruleView.searchField;
  let onRuleViewFilter = inspector.once("ruleview-filtered");

  searchField.focus();
  synthesizeKeys(SEARCH, win);
  yield onRuleViewFilter;

  info("Check that the correct rules are visible");
  is(getRuleViewRuleEditor(ruleView, 0).rule.selectorText, "element",
    "First rule is inline element.");

  let ruleEditor = getRuleViewRuleEditor(ruleView, 2, 0);

  is(ruleEditor.rule.domRule.keyText, "20%", "Second rule is 20%.");
  ok(ruleEditor.selectorText.classList.contains("ruleview-highlight"),
    "20% selector is highlighted.")
}
