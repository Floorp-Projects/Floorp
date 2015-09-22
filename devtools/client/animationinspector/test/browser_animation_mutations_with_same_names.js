/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that when animations are added later (through animation mutations) and
// if these animations have the same names, then all of them are still being
// displayed (which should be true as long as these animations apply to
// different nodes).

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_negative_animation.html");
  let {controller, panel} = yield openAnimationInspector();

  info("Wait until all animations have been added " +
       "(they're added with setTimeout)");
  while (controller.animationPlayers.length < 3) {
    yield controller.once(controller.PLAYERS_UPDATED_EVENT);
  }
  yield waitForAllAnimationTargets(panel);

  is(panel.animationsTimelineComponent.animations.length, 3,
     "The timeline shows 3 animations too");

  // Reduce the known nodeFronts to a set to make them unique.
  let nodeFronts = new Set(panel.animationsTimelineComponent
                                .targetNodes.map(n => n.nodeFront));
  is(nodeFronts.size, 3,
     "The animations are applied to 3 different node fronts");
});
