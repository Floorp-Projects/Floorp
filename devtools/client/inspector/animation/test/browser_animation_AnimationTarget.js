/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test existance and content of animation target.

add_task(async function () {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  const { animationInspector, inspector, panel } = await openAnimationInspector();

  info("Checking the animation target elements existance");
  const animationItemEls = panel.querySelectorAll(".animation-list .animation-item");
  is(animationItemEls.length, animationInspector.animations.length,
     "Number of animation target element should be same to number of animations "
     + "that displays");

  for (const animationItemEl of animationItemEls) {
    const animationTargetEl = animationItemEl.querySelector(".animation-target");
    ok(animationTargetEl,
       "The animation target element should be in each animation item element");
  }

  info("Checking the content of animation target");
  await selectNodeAndWaitForAnimations(".animated", inspector);
  const animationTargetEl =
    panel.querySelector(".animation-list .animation-item .animation-target");
  is(animationTargetEl.textContent, "div.ball.animated",
     "The target element's content is correct");
  ok(animationTargetEl.querySelector(".objectBox"),
     "objectBox is in the page exists");
});
