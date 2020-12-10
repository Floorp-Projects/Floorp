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
  const { inspector, view } = await openRuleView();
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.FLEXBOX;
  const {
    getNodeForActiveHighlighter,
    waitForHighlighterTypeShown,
  } = getHighlighterTestHelpers(inspector);

  await selectNode("#flex", inspector);
  const container = getRuleViewProperty(view, "#flex", "display").valueSpan;
  const flexboxToggle = container.querySelector(
    ".js-toggle-flexbox-highlighter"
  );

  info("Toggling ON the flexbox highlighter from the rule-view.");
  const onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  flexboxToggle.click();
  await onHighlighterShown;

  ok(
    getNodeForActiveHighlighter(HIGHLIGHTER_TYPE),
    "Flexbox highlighter is shown."
  );

  info("Reload the page, expect the highlighter to be displayed once again");
  let onStateRestored = inspector.highlighters.once("flexbox-state-restored");

  const onReloaded = inspector.once("reloaded");
  await refreshTab();
  info("Wait for inspector to be reloaded after page reload");
  await onReloaded;

  let { restored } = await onStateRestored;
  ok(restored, "The highlighter state was restored");

  info(
    "Check that the flexbox highlighter can be displayed after reloading the page"
  );
  ok(
    getNodeForActiveHighlighter(HIGHLIGHTER_TYPE),
    "Flexbox highlighter is shown."
  );

  info("Navigate to another URL, and check that the highlighter is hidden");
  const otherUri =
    "data:text/html;charset=utf-8," + encodeURIComponent(OTHER_URI);
  onStateRestored = inspector.highlighters.once("flexbox-state-restored");
  await navigateTo(otherUri);
  ({ restored } = await onStateRestored);
  ok(!restored, "The highlighter state was not restored");
  ok(
    !getNodeForActiveHighlighter(HIGHLIGHTER_TYPE),
    "Flexbox highlighter is hidden."
  );
});
