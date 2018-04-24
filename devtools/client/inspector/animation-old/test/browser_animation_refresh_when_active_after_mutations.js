/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that refresh animation UI while the panel is hidden.

const EXPECTED_GRAPH_PATH_SEGMENTS = [{ x: 0, y: 0 },
                                      { x: 49999, y: 0.0 },
                                      { x: 50000, y: 0.5 },
                                      { x: 99999, y: 0.5 },
                                      { x: 100000, y: 0 }];

add_task(async function() {
  info("Open animation inspector once so that activate animation mutations listener");
  await addTab("data:text/html;charset=utf8,<div id='target'>test</div>");
  const { controller, inspector, panel } = await openAnimationInspector();

  info("Select other tool to hide animation inspector");
  await inspector.sidebar.select("ruleview");

  // Count players-updated event in controller.
  let updatedEventCount = 0;
  controller.on("players-updated", () => {
    updatedEventCount += 1;
  });

  info("Make animation by eval in content");
  await evalInDebuggee(`document.querySelector('#target').animate(
                        { transform: 'translate(100px)' },
                        { duration: 100000, easing: 'steps(2)' });`);
  info("Wait for animation mutations event");
  await controller.animationsFront.once("mutations");
  info("Check players-updated events count");
  is(updatedEventCount, 0, "players-updated event shoud not be fired");

  info("Re-select animation inspector and check the UI");
  await inspector.sidebar.select("animationinspector");
  await waitForAnimationTimelineRendering(panel);

  const timeBlocks = getAnimationTimeBlocks(panel);
  is(timeBlocks.length, 1, "One animation should display");
  const timeBlock = timeBlocks[0];
  const state = timeBlock.animation.state;
  const effectEl = timeBlock.containerEl.querySelector(".effect-easing");
  ok(effectEl, "<g> element for effect easing should exist");
  const pathEl = effectEl.querySelector("path");
  ok(pathEl, "<path> element for effect easing should exist");
  assertPathSegments(pathEl, state.duration, false, EXPECTED_GRAPH_PATH_SEGMENTS);
});
