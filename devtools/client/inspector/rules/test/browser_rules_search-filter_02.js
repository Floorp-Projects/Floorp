/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for keyframe rule
// selectors.

const SEARCH = "20%";
const TEST_URI = URL_ROOT + "doc_keyframeanimation.html";

add_task(function*() {
  yield addTab(TEST_URI);
  let {inspector, view} = yield openRuleView();
  yield selectNode("#boxy", inspector);
  yield testAddTextInFilter(inspector, view);
});

function* testAddTextInFilter(inspector, view) {
  yield setSearchFilter(view, SEARCH);

  info("Check that the correct rules are visible");
  is(getRuleViewRuleEditor(view, 0).rule.selectorText, "element",
    "First rule is inline element.");

  let ruleEditor = getRuleViewRuleEditor(view, 2, 0);

  is(ruleEditor.rule.domRule.keyText, "20%", "Second rule is 20%.");
  ok(ruleEditor.selectorText.classList.contains("ruleview-highlight"),
    "20% selector is highlighted.");
}
