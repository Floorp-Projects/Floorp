/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the flexbox highlighter is re-displayed after reloading a page.

const TEST_URI = `
  <style type='text/css'>
    #flex {
      display: flex;
    }
  </style>
  <div id="flex"></div>
`;

const OTHER_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid"></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  info("Check that the flexbox highlighter can be displayed.");
  const {inspector, view} = await openRuleView();
  const {highlighters} = view;

  await selectNode("#flex", inspector);
  const container = getRuleViewProperty(view, "#flex", "display").valueSpan;
  const flexboxToggle = container.querySelector(".ruleview-flex");

  info("Toggling ON the flexbox highlighter from the rule-view.");
  const onHighlighterShown = highlighters.once("flexbox-highlighter-shown");
  flexboxToggle.click();
  await onHighlighterShown;

  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");

  info("Reload the page, expect the highlighter to be displayed once again");
  let onStateRestored = highlighters.once("flexbox-state-restored");
  await refreshTab();
  let { restored } = await onStateRestored;
  ok(restored, "The highlighter state was restored");

  info("Check that the flexbox highlighter can be displayed after reloading the page");
  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");

  info("Navigate to another URL, and check that the highlighter is hidden");
  const otherUri = "data:text/html;charset=utf-8," + encodeURIComponent(OTHER_URI);
  onStateRestored = highlighters.once("flexbox-state-restored");
  await navigateTo(inspector, otherUri);
  ({ restored } = await onStateRestored);
  ok(!restored, "The highlighter state was not restored");
  ok(!highlighters.flexboxHighlighterShown, "Flexbox highlighter is hidden.");
});
