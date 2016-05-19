/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that the highlighter is correctly positioned when switching context
// to an iframe that has an offset from the parent viewport (eg. 100px margin)

const TEST_URI = "data:text/html;charset=utf-8," +
  "<div id=\"outer\"></div>" +
  "<iframe style='margin:100px' src='data:text/html," +
  "<div id=\"inner\">Look I am here!</div>'>";

add_task(function* () {
  info("Enable command-button-frames preference setting");
  Services.prefs.setBoolPref("devtools.command-button-frames.enabled", true);
  let {inspector, toolbox, testActor} = yield openInspectorForURL(TEST_URI);

  info("Switch to the iframe context.");
  yield switchToFrameContext(1, toolbox, inspector);

  info("Check navigation was successful.");
  let hasOuterNode = yield testActor.hasNode("#outer");
  ok(!hasOuterNode, "Check testActor has no access to outer element");
  let hasTestNode = yield testActor.hasNode("#inner");
  ok(hasTestNode, "Check testActor has access to inner element");

  info("Check highlighting is correct after switching iframe context");
  yield selectAndHighlightNode("#inner", inspector);
  let isHighlightCorrect = yield testActor.assertHighlightedNode("#inner");
  ok(isHighlightCorrect, "The selected node is properly highlighted.");

  info("Cleanup command-button-frames preferences.");
  Services.prefs.clearUserPref("devtools.command-button-frames.enabled");
});

/**
 * Helper designed to switch context to another frame at the provided index.
 * Returns a promise that will resolve when the navigation is complete.
 * @return {Promise}
 */
function* switchToFrameContext(frameIndex, toolbox, inspector) {
  // Verify that the frame list button is visible and populated
  let frameListBtn = toolbox.doc.getElementById("command-button-frames");
  let frameBtns = frameListBtn.firstChild.querySelectorAll("[data-window-id]");

  info("Select the iframe in the frame list.");
  let newRoot = inspector.once("new-root");
  frameBtns[frameIndex].click();
  yield newRoot;
  yield inspector.once("inspector-updated");

  info("Navigation to the iframe is done.");
}
