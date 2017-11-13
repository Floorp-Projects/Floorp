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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  info("Check that the flexbox highlighter can be displayed.");
  let {inspector, view} = yield openRuleView();
  let {highlighters} = view;

  yield selectNode("#flex", inspector);
  let container = getRuleViewProperty(view, "#flex", "display").valueSpan;
  let flexboxToggle = container.querySelector(".ruleview-flex");

  info("Toggling ON the flexbox highlighter from the rule-view.");
  let onHighlighterShown = highlighters.once("flexbox-highlighter-shown");
  flexboxToggle.click();
  yield onHighlighterShown;

  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");

  info("Reload the page, expect the highlighter to be displayed once again");
  let onStateRestored = highlighters.once("flexbox-state-restored");
  yield refreshTab(gBrowser.selectedTab);
  let { restored } = yield onStateRestored;
  ok(restored, "The highlighter state was restored");

  info("Check that the flexbox highlighter can be displayed after reloading the page");
  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");

  info("Navigate to another URL, and check that the highlighter is hidden");
  let otherUri = "data:text/html;charset=utf-8," + encodeURIComponent(OTHER_URI);
  onStateRestored = highlighters.once("flexbox-state-restored");
  yield navigateTo(inspector, otherUri);
  ({ restored } = yield onStateRestored);
  ok(!restored, "The highlighter state was not restored");
  ok(!highlighters.flexboxHighlighterShown, "Flexbox highlighter is hidden.");
});
