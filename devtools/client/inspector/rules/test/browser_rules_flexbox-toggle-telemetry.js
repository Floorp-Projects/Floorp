/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the telemetry is correct when the flexbox highlighter is activated from
// the rules view.

const TEST_URI = `
  <style type='text/css'>
    #flex {
      display: flex;
    }
  </style>
  <div id="flex"></div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  startTelemetry();
  const { inspector, view } = await openRuleView();
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.FLEXBOX;
  const { waitForHighlighterTypeShown, waitForHighlighterTypeHidden } =
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

  info("Toggling OFF the flexbox highlighter from the rule-view.");
  const onHighlighterHidden = waitForHighlighterTypeHidden(HIGHLIGHTER_TYPE);
  flexboxToggle.click();
  await onHighlighterHidden;

  checkResults();
});

function checkResults() {
  checkTelemetry("devtools.rules.flexboxhighlighter.opened", "", 1, "scalar");
  checkTelemetry(
    "DEVTOOLS_FLEXBOX_HIGHLIGHTER_TIME_ACTIVE_SECONDS",
    "",
    null,
    "hasentries"
  );
}
