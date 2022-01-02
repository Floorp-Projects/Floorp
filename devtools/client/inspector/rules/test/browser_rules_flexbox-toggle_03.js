/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the flexbox highlighter in the rule view with multiple flexboxes in the
// page.

const TEST_URI = `
  <style type='text/css'>
    .flex {
      display: flex;
    }
  </style>
  <div id="flex1" class="flex"></div>
  <div id="flex2" class="flex"></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.FLEXBOX;
  const {
    getActiveHighlighter,
    getNodeForActiveHighlighter,
    waitForHighlighterTypeShown,
  } = getHighlighterTestHelpers(inspector);

  info("Selecting the first flexbox container.");
  await selectNode("#flex1", inspector);
  let container = getRuleViewProperty(view, ".flex", "display").valueSpan;
  let flexboxToggle = container.querySelector(".js-toggle-flexbox-highlighter");

  info(
    "Checking the state of the flexbox toggle for the first flexbox container in " +
      "the rule-view."
  );
  ok(flexboxToggle, "flexbox highlighter toggle is visible.");
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

  info(
    "Toggling ON the flexbox highlighter for the first flexbox container from the " +
      "rule-view."
  );
  let onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
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

  info("Selecting the second flexbox container.");
  await selectNode("#flex2", inspector);
  const firstFlexboxHighterShown = getNodeForActiveHighlighter(
    HIGHLIGHTER_TYPE
  );
  container = getRuleViewProperty(view, ".flex", "display").valueSpan;
  flexboxToggle = container.querySelector(".js-toggle-flexbox-highlighter");

  info(
    "Checking the state of the CSS flexbox toggle for the second flexbox container " +
      "in the rule-view."
  );
  ok(flexboxToggle, "Flexbox highlighter toggle is visible.");
  ok(
    !flexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle button is not active."
  );
  ok(
    getNodeForActiveHighlighter(HIGHLIGHTER_TYPE),
    "Flexbox highlighter is still shown."
  );

  info(
    "Toggling ON the flexbox highlighter for the second flexbox container from the " +
      "rule-view."
  );
  onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  flexboxToggle.click();
  await onHighlighterShown;

  info(
    "Checking the flexbox highlighter is created for the second flexbox container " +
      "and toggle button is active in the rule-view."
  );
  ok(
    flexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle is active."
  );
  ok(
    getNodeForActiveHighlighter(HIGHLIGHTER_TYPE) != firstFlexboxHighterShown,
    "Flexbox highlighter for the second flexbox container is shown."
  );

  info("Selecting the first flexbox container.");
  await selectNode("#flex1", inspector);
  container = getRuleViewProperty(view, ".flex", "display").valueSpan;
  flexboxToggle = container.querySelector(".js-toggle-flexbox-highlighter");

  info(
    "Checking the state of the flexbox toggle for the first flexbox container in " +
      "the rule-view."
  );
  ok(flexboxToggle, "Flexbox highlighter toggle is visible.");
  ok(
    !flexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle button is not active."
  );
});
