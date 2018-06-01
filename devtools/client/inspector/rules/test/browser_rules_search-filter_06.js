/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter does not highlight the source with
// input that could be parsed as a property line.

const SEARCH = "doc_urls_clickable.css: url";
const TEST_URI = URL_ROOT + "doc_urls_clickable.html";

add_task(async function() {
  await addTab(TEST_URI);
  const {inspector, view} = await openRuleView();
  await selectNode(".relative1", inspector);
  await testAddTextInFilter(inspector, view);
});

async function testAddTextInFilter(inspector, view) {
  await setSearchFilter(view, SEARCH);

  info("Check that the correct rules are visible");
  is(view.element.children.length, 1, "Should have 1 rules.");
  is(getRuleViewRuleEditor(view, 0).rule.selectorText, "element",
    "First rule is inline element.");
}
