/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether AnimationInspector and base pane exists.

add_task(async function() {
  const { animationInspector, panel } = await openAnimationInspector();
  ok(animationInspector, "AnimationInspector should exist");
  ok(panel, "Main animation-inspector panel should exist");

  await closeAnimationInspector();
});
