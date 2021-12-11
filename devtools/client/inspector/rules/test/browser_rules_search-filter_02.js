/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for keyframe rule
// selectors.

const SEARCH = "20%";
const TEST_URI = URL_ROOT + "doc_keyframeanimation.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("#boxy", inspector);
  await testAddTextInFilter(inspector, view);
});

async function testAddTextInFilter(inspector, view) {
  await setSearchFilter(view, SEARCH);

  info("Check that the correct rules are visible");
  is(
    getRuleViewRuleEditor(view, 0).rule.selectorText,
    "element",
    "First rule is inline element."
  );

  const ruleEditor = getRuleViewRuleEditor(view, 2, 0);

  is(ruleEditor.rule.domRule.keyText, "20%", "Second rule is 20%.");
  ok(
    ruleEditor.selectorText.classList.contains("ruleview-highlight"),
    "20% selector is highlighted."
  );
}
