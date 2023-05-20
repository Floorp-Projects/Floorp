/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the telemetry is correct when the flexbox highlighter is activated from
// the layout view.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function () {
  await addTab(TEST_URI);
  startTelemetry();
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;
  const onFlexHighlighterToggleRendered = waitForDOM(
    doc,
    "#flexbox-checkbox-toggle"
  );
  await selectNode("#container", inspector);
  const [flexHighlighterToggle] = await onFlexHighlighterToggleRendered;

  await toggleHighlighterON(flexHighlighterToggle, inspector);
  await toggleHighlighterOFF(flexHighlighterToggle, inspector);

  checkResults();
});

function checkResults() {
  checkTelemetry("devtools.layout.flexboxhighlighter.opened", "", 1, "scalar");
  checkTelemetry(
    "DEVTOOLS_FLEXBOX_HIGHLIGHTER_TIME_ACTIVE_SECONDS",
    "",
    null,
    "hasentries"
  );
}
