/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the highlighter element picker still works through iframe
// navigations.

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>bug 699308 - test iframe navigation</p>" +
  "<iframe src='data:text/html;charset=utf-8,hello world'></iframe>";

add_task(function* () {
  let { toolbox, testActor } = yield openInspectorForURL(TEST_URI);

  info("Starting element picker.");
  yield startPicker(toolbox);

  info("Waiting for highlighter to activate.");
  let highlighterShowing = toolbox.once("highlighter-ready");
  testActor.synthesizeMouse({
    selector: "body",
    options: {type: "mousemove"},
    x: 1,
    y: 1
  });
  yield highlighterShowing;

  let isVisible = yield testActor.isHighlighting();
  ok(isVisible, "Inspector is highlighting.");

  yield testActor.reloadFrame("iframe");
  info("Frame reloaded. Reloading again.");

  yield testActor.reloadFrame("iframe");
  info("Frame reloaded twice.");

  isVisible = yield testActor.isHighlighting();
  ok(isVisible, "Inspector is highlighting after iframe nav.");

  info("Stopping element picker.");
  yield toolbox.highlighterUtils.stopPicker();
});
