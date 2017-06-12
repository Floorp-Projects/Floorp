/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that when animations are added later (through animation mutations) and
// if these animations have the same names, then all of them are still being
// displayed (which should be true as long as these animations apply to
// different nodes).

add_task(function* () {
  yield addTab(URL_ROOT + "doc_negative_animation.html");
  const {controller, panel} = yield openAnimationInspector();
  const timeline = panel.animationsTimelineComponent;

  const areTracksReady = () => timeline.animations.every(a => timeline.tracksMap.has(a));

  // We need to wait for all tracks to be ready, cause this is an async part of the init
  // of the panel.
  while (controller.animationPlayers.length < 3 || !areTracksReady()) {
    yield waitForAnimationTimelineRendering(panel);
  }
  // Same for animation targets, they're retrieved asynchronously.
  yield waitForAllAnimationTargets(panel);

  is(panel.animationsTimelineComponent.animations.length, 3,
     "The timeline shows 3 animations too");

  // Reduce the known nodeFronts to a set to make them unique.
  let nodeFronts = new Set(panel.animationsTimelineComponent
                                .targetNodes.map(n => n.previewer.nodeFront));
  is(nodeFronts.size, 3, "The animations are applied to 3 different node fronts");
});
