/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following AnimationTarget component works.
// * element existance
// * number of elements
// * content of element
// * select an animated node by clicking on inspect node
// * title of inspect icon

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".long"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Checking the animation target elements existance");
  const animationItemEls = panel.querySelectorAll(".animation-list .animation-item");
  is(animationItemEls.length, animationInspector.state.animations.length,
     "Number of animation target element should be same to number of animations " +
     "that displays");

  for (const animationItemEl of animationItemEls) {
    const animationTargetEl = animationItemEl.querySelector(".animation-target");
    ok(animationTargetEl,
      "The animation target element should be in each animation item element");
  }

  info("Checking the selecting an animated node by clicking the target node");
  await clickOnTargetNode(animationInspector, panel, 0);
  is(panel.querySelectorAll(".animation-target").length, 1,
    "The length of animations should be 1");

  info("Checking the content of animation target");
  const animationTargetEl =
    panel.querySelector(".animation-list .animation-item .animation-target");
  is(animationTargetEl.textContent, "div.ball.animated",
    "The target element's content is correct");
  ok(animationTargetEl.querySelector(".objectBox"), "objectBox is in the page exists");
  ok(animationTargetEl.querySelector(".open-inspector").title,
     INSPECTOR_L10N.getStr("inspector.nodePreview.highlightNodeLabel"));
});
