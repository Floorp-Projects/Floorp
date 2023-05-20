/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling ON/OFF the flexbox highlighter from the flexbox inspector panel.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.FLEXBOX;
  const { getActiveHighlighter } = getHighlighterTestHelpers(inspector);

  const onFlexHighlighterToggleRendered = waitForDOM(
    doc,
    "#flexbox-checkbox-toggle"
  );
  await selectNode("#container", inspector);
  const [flexHighlighterToggle] = await onFlexHighlighterToggleRendered;

  info("Checking the initial state of the Flexbox Inspector.");
  ok(flexHighlighterToggle, "The flexbox highlighter toggle is rendered.");
  ok(
    !flexHighlighterToggle.checked,
    "The flexbox highlighter toggle is unchecked."
  );
  ok(
    !getActiveHighlighter(HIGHLIGHTER_TYPE),
    "No flexbox highlighter is shown."
  );

  await toggleHighlighterON(flexHighlighterToggle, inspector);

  info("Checking the flexbox highlighter is created.");
  ok(getActiveHighlighter(HIGHLIGHTER_TYPE), "Flexbox highlighter is shown.");
  ok(
    flexHighlighterToggle.checked,
    "The flexbox highlighter toggle is checked."
  );

  await toggleHighlighterOFF(flexHighlighterToggle, inspector);

  info("Checking the flexbox highlighter is not shown.");
  ok(
    !getActiveHighlighter(HIGHLIGHTER_TYPE),
    "No flexbox highlighter is shown."
  );
  ok(
    !flexHighlighterToggle.checked,
    "The flexbox highlighter toggle is unchecked."
  );
});
