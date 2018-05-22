/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether proper currentTime was set for each animations.

const WAIT_TIME = 3000;

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".still"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Add an animation to make a situation which has different creation time");
  await wait(WAIT_TIME);
  await setClassAttribute(animationInspector, ".still", "ball compositor-all");

  info("Move the scrubber");
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 0.5);

  info("Check existed animations have different currentTime");
  const animations = animationInspector.state.animations;
  ok(animations[0].state.currentTime + WAIT_TIME > animations[1].state.currentTime,
    `The currentTime of added animation shold be ${ WAIT_TIME }ms less than ` +
    "at least that currentTime of first animation");
});
