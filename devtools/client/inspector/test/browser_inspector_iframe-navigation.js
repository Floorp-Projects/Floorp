/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the highlighter element picker still works through iframe
// navigations.

const TEST_URI =
  "data:text/html;charset=utf-8," +
  "<p>bug 699308 - test iframe navigation</p>" +
  "<iframe src='data:text/html;charset=utf-8,hello world'></iframe>";

add_task(async function() {
  const { toolbox, testActor } = await openInspectorForURL(TEST_URI);

  info("Starting element picker.");
  await startPicker(toolbox);

  info("Waiting for highlighter to activate.");
  const highlighterShowing = toolbox.once("highlighter-ready");
  testActor.synthesizeMouse({
    selector: "body",
    options: { type: "mousemove" },
    x: 1,
    y: 1,
  });
  await highlighterShowing;

  let isVisible = await testActor.isHighlighting();
  ok(isVisible, "Inspector is highlighting.");

  await testActor.reloadFrame("iframe");
  info("Frame reloaded. Reloading again.");

  await testActor.reloadFrame("iframe");
  info("Frame reloaded twice.");

  isVisible = await testActor.isHighlighting();
  ok(isVisible, "Inspector is highlighting after iframe nav.");

  info("Stopping element picker.");
  await toolbox.inspectorFront.nodePicker.stop();
});
