/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the telemetry is correct when the flexbox highlighter is activated from
// the markup view.

const TEST_URI = `
  <style type="text/css">
    #flex {
      display: flex;
    }
  </style>
  <div id="flex"></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  startTelemetry();
  const { inspector } = await openLayoutView();
  const { highlighters } = inspector;

  await selectNode("#flex", inspector);
  const flexContainer = await getContainerForSelector("#flex", inspector);
  const flexDisplayBadge = flexContainer.elt.querySelector(
    ".inspector-badge.interactive[data-display]"
  );

  info("Toggling ON the flexbox highlighter from the flex display badge.");
  const onHighlighterShown = highlighters.once("flexbox-highlighter-shown");
  flexDisplayBadge.click();
  await onHighlighterShown;

  info("Toggling OFF the flexbox highlighter from the flex display badge.");
  const onHighlighterHidden = highlighters.once("flexbox-highlighter-hidden");
  flexDisplayBadge.click();
  await onHighlighterHidden;

  checkResults();
});

function checkResults() {
  checkTelemetry("devtools.markup.flexboxhighlighter.opened", "", 1, "scalar");
  checkTelemetry(
    "DEVTOOLS_FLEXBOX_HIGHLIGHTER_TIME_ACTIVE_SECONDS",
    "",
    null,
    "hasentries"
  );
}
