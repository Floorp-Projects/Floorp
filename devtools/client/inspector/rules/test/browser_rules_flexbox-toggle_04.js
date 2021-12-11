/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the flexbox highlighter in the rule view from a
// 'display: flex!important' declaration.

const TEST_URI = `
  <style type='text/css'>
    #flex {
      display: flex !important;
    }
  </style>
  <div id="flex"></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.FLEXBOX;
  const {
    getActiveHighlighter,
    getNodeForActiveHighlighter,
    waitForHighlighterTypeShown,
    waitForHighlighterTypeHidden,
  } = getHighlighterTestHelpers(inspector);

  await selectNode("#flex", inspector);
  const container = getRuleViewProperty(view, "#flex", "display").valueSpan;
  const flexboxToggle = container.querySelector(
    ".js-toggle-flexbox-highlighter"
  );

  info("Checking the initial state of the flexbox toggle in the rule-view.");
  ok(flexboxToggle, "Flexbox highlighter toggle is visible.");
  ok(
    !flexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle button is not active."
  );
  ok(
    !getActiveHighlighter(HIGHLIGHTER_TYPE),
    "No flexbox highlighter exists in the rule-view."
  );
  ok(
    !getNodeForActiveHighlighter(HIGHLIGHTER_TYPE),
    "No flexbox highlighter is shown."
  );

  info("Toggling ON the flexbox highlighter from the rule-view.");
  const onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  flexboxToggle.click();
  await onHighlighterShown;

  info(
    "Checking the flexbox highlighter is created and toggle button is active in " +
      "the rule-view."
  );
  ok(
    flexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle is active."
  );
  ok(
    getActiveHighlighter(HIGHLIGHTER_TYPE),
    "Flexbox highlighter created in the rule-view."
  );
  ok(
    getNodeForActiveHighlighter(HIGHLIGHTER_TYPE),
    "Flexbox highlighter is shown."
  );

  info("Toggling OFF the flexbox highlighter from the rule-view.");
  const onHighlighterHidden = waitForHighlighterTypeHidden(HIGHLIGHTER_TYPE);
  flexboxToggle.click();
  await onHighlighterHidden;

  info(
    "Checking the flexbox highlighter is not shown and toggle button is not active " +
      "in the rule-view."
  );
  ok(
    !flexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle button is not active."
  );
  ok(
    !getActiveHighlighter(HIGHLIGHTER_TYPE),
    "No flexbox highlighter is shown."
  );
});
