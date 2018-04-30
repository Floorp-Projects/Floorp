/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following AnimationTarget component works.
// * element existance
// * number of elements
// * content of element
// * title of inspect icon

const TEST_DATA = [
  { expectedTextContent: "div.ball.animated" },
  { expectedTextContent: "div.ball.long" },
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".long"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Checking the animation target elements existance");
  const animationItemEls = panel.querySelectorAll(".animation-list .animation-item");
  is(animationItemEls.length, animationInspector.state.animations.length,
     "Number of animation target element should be same to number of animations " +
     "that displays");

  for (let i = 0; i < animationItemEls.length; i++) {
    const animationItemEl = animationItemEls[i];
    const animationTargetEl = animationItemEl.querySelector(".animation-target");
    ok(animationTargetEl,
      "The animation target element should be in each animation item element");

    info("Checking the content of animation target");
    const testData = TEST_DATA[i];
    is(animationTargetEl.textContent, testData.expectedTextContent,
       "The target element's content is correct");
    ok(animationTargetEl.querySelector(".objectBox"), "objectBox is in the page exists");
    ok(animationTargetEl.querySelector(".open-inspector").title,
       INSPECTOR_L10N.getStr("inspector.nodePreview.highlightNodeLabel"));
  }
});
