/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that the highlighter is correctly positioned when switching context
// to an iframe that has an offset from the parent viewport (eg. 100px margin)

const TEST_URI =
  "data:text/html;charset=utf-8," +
  '<div id="outer"></div>' +
  "<iframe style='margin:100px' src='data:text/html," +
  '<div id="inner">Look I am here!</div>\'>';

add_task(async function() {
  info("Enable command-button-frames preference setting");
  Services.prefs.setBoolPref("devtools.command-button-frames.enabled", true);
  const {
    inspector,
    toolbox,
    highlighterTestFront,
  } = await openInspectorForURL(TEST_URI);

  await assertMarkupViewAsTree(
    `
    body
      div id="outer"
      iframe!ignore-children`,
    "body",
    inspector
  );

  info("Switch to the iframe context.");
  await switchToFrameContext(1, toolbox, inspector);

  info("Check the markup view is rendered correctly after switching frames");
  await assertMarkupViewAsTree(
    `
    body
      div id="inner"`,
    "body",
    inspector
  );

  info("Check highlighting is correct after switching iframe context");
  await selectAndHighlightNode("#inner", inspector);
  const isHighlightCorrect = await highlighterTestFront.assertHighlightedNode(
    "#inner"
  );
  ok(isHighlightCorrect, "The selected node is properly highlighted.");

  info("Cleanup command-button-frames preferences.");
  Services.prefs.clearUserPref("devtools.command-button-frames.enabled");
});

/**
 * Helper designed to switch context to another frame at the provided index.
 * Returns a promise that will resolve when the navigation is complete.
 * @return {Promise}
 */
async function switchToFrameContext(frameIndex, toolbox, inspector) {
  // Open frame menu and wait till it's available on the screen.
  const btn = toolbox.doc.getElementById("command-button-frames");
  const panel = toolbox.doc.getElementById("command-button-frames-panel");
  btn.click();
  ok(panel, "popup panel has created.");
  await waitUntil(() => panel.classList.contains("tooltip-visible"));

  info("Select the iframe in the frame list.");
  const menuList = toolbox.doc.getElementById("toolbox-frame-menu");
  const firstButton = menuList.querySelectorAll(".command")[frameIndex];
  const newRoot = inspector.once("new-root");

  firstButton.click();

  await newRoot;
  await inspector.once("inspector-updated");

  info("Navigation to the iframe is done.");
}
