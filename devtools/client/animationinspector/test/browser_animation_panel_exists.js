/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the animation panel sidebar exists

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8,welcome to the animation panel");
  let {panel, controller} = yield openAnimationInspector();

  ok(controller,
     "The animation controller exists");
  ok(controller.animationsFront,
     "The animation controller has been initialized");
  ok(panel,
     "The animation panel exists");
  ok(panel.playersEl,
     "The animation panel has been initialized");
  ok(panel.animationsTimelineComponent,
     "The animation panel has been initialized");
});
