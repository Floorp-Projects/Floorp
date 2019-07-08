/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a message is displayed when no flex container is selected.

const TEST_URI = `
  <div></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;
  const { highlighters } = inspector;

  info("Checking the initial state of the Flexbox Inspector.");
  ok(
    doc.querySelector(
      ".flex-accordion .devtools-sidepanel-no-result",
      "A message is displayede when no flex container is selected."
    )
  );
  ok(
    !highlighters.flexboxHighlighterShown,
    "No flexbox highlighter exists in the highlighters overlay."
  );
});
