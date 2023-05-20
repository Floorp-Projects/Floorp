/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that flexbox highlighter is hidden on page navigation.

const TEST_URI = `
  <style type='text/css'>
    #flex {
      display: flex;
    }
  </style>
  <div id="flex"></div>
`;

const TEST_URI_2 = "data:text/html,<html><body>test</body></html>";

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.FLEXBOX;
  const { getNodeForActiveHighlighter, waitForHighlighterTypeShown } =
    getHighlighterTestHelpers(inspector);

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

  await navigateTo(TEST_URI_2);
  ok(
    !getNodeForActiveHighlighter(HIGHLIGHTER_TYPE),
    "Flexbox highlighter is hidden."
  );
});
