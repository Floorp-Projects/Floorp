/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that player widgets are displayed right when the animation panel is
// initialized, if the selected node (<body> by default) is animated.

const { ANIMATION_TYPES } = require("devtools/server/actors/animation");

add_task(async function() {
  await addTab(URL_ROOT + "doc_multiple_animation_types.html");

  const {panel} = await openAnimationInspector();
  is(panel.animationsTimelineComponent.animations.length, 3,
    "Three animations are handled by the timeline after init");
  assertAnimationsDisplayed(panel, 3,
    "Three animations are displayed after init");
  is(
    panel.animationsTimelineComponent
         .animationsEl
         .querySelectorAll(`.animation.${ANIMATION_TYPES.SCRIPT_ANIMATION}`)
         .length,
    1,
    "One script-generated animation is displayed");
  is(
    panel.animationsTimelineComponent
         .animationsEl
         .querySelectorAll(`.animation.${ANIMATION_TYPES.CSS_ANIMATION}`)
         .length,
    1,
    "One CSS animation is displayed");
  is(
    panel.animationsTimelineComponent
         .animationsEl
         .querySelectorAll(`.animation.${ANIMATION_TYPES.CSS_TRANSITION}`)
         .length,
    1,
    "One CSS transition is displayed");
});
