/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the flex display badge toggles on the flexbox highlighter.

const TEST_URI = `
  <style type="text/css">
    #flex {
      display: flex;
    }
  </style>
  <div id="flex"></div>
`;

const HIGHLIGHTER_TYPE = "FlexboxHighlighter";

add_task(async function() {
  await pushPref("devtools.inspector.flexboxHighlighter.enabled", true);
  await pushPref("devtools.flexboxinspector.enabled", true);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector } = await openLayoutView();
  const { highlighters, store } = inspector;

  info("Check the flex display badge is shown and not active.");
  await selectNode("#flex", inspector);
  const flexContainer = await getContainerForSelector("#flex", inspector);
  const flexDisplayBadge = flexContainer.elt.querySelector(".markup-badge[data-display]");
  ok(!flexDisplayBadge.classList.contains("active"), "flex display badge is not active.");
  ok(flexDisplayBadge.classList.contains("interactive"),
    "flex display badge is interactive.");

  info("Check the initial state of the flex highlighter.");
  ok(!highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No flexbox highlighter exists in the highlighters overlay.");
  ok(!highlighters.flexboxHighlighterShown, "No flexbox highlighter is shown.");

  info("Toggling ON the flexbox highlighter from the flex display badge.");
  const onHighlighterShown = highlighters.once("flexbox-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state => state.flexbox.highlighted);
  flexDisplayBadge.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info("Check the flexbox highlighter is created and flex display badge state.");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "Flexbox highlighter is created in the highlighters overlay.");
  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");
  ok(flexDisplayBadge.classList.contains("active"), "flex display badge is active.");
  ok(flexDisplayBadge.classList.contains("interactive"),
    "flex display badge is interactive.");

  info("Toggling OFF the flexbox highlighter from the flex display badge.");
  const onHighlighterHidden = highlighters.once("flexbox-highlighter-hidden");
  onCheckboxChange = waitUntilState(store, state => !state.flexbox.highlighted);
  flexDisplayBadge.click();
  await onHighlighterHidden;
  await onCheckboxChange;

  ok(!flexDisplayBadge.classList.contains("active"), "flex display badge is not active.");
  ok(flexDisplayBadge.classList.contains("interactive"),
    "flex display badge is interactive.");
});
