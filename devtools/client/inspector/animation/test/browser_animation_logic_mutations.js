/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following mutations:
// * add animation
// * remove animation
// * modify animation

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([
    ".compositor-all",
    ".compositor-notall",
    ".no-compositor",
    ".still",
  ]);
  const {
    animationInspector,
    inspector,
    panel,
  } = await openAnimationInspector();

  info("Checking the mutation for add an animation");
  const originalAnimationCount = animationInspector.state.animations.length;
  await setClassAttribute(animationInspector, ".still", "ball no-compositor");
  is(
    animationInspector.state.animations.length,
    originalAnimationCount + 1,
    "Count of animation should be plus one to original count"
  );

  info(
    "Checking added animation existence even the animation name is duplicated"
  );
  is(
    getAnimationNameCount(panel, "no-compositor"),
    2,
    "Count of animation should be plus one to original count"
  );

  info("Checking the mutation for remove an animation");
  await setClassAttribute(
    animationInspector,
    ".compositor-notall",
    "ball still"
  );
  is(
    animationInspector.state.animations.length,
    originalAnimationCount,
    "Count of animation should be same to original count since we remove an animation"
  );

  info("Checking the mutation for modify an animation");
  await selectNodeAndWaitForAnimations(".compositor-all", inspector);
  await setStyle(
    animationInspector,
    ".compositor-all",
    "animationDuration",
    "100s"
  );
  await setStyle(
    animationInspector,
    ".compositor-all",
    "animationIterationCount",
    1
  );
  const summaryGraphPathEl = getSummaryGraphPathElement(
    panel,
    "compositor-all"
  );
  is(
    summaryGraphPathEl.viewBox.baseVal.width,
    100000,
    "Width of summary graph path should be 100000 " +
      "after modifing the duration and iteration count"
  );
  await setStyle(
    animationInspector,
    ".compositor-all",
    "animationDelay",
    "100s"
  );
  is(
    summaryGraphPathEl.viewBox.baseVal.width,
    200000,
    "Width of summary graph path should be 200000 after modifing the delay"
  );
  ok(
    summaryGraphPathEl.parentElement.querySelector(".animation-delay-sign"),
    "Delay sign element shoud exist"
  );
});

function getAnimationNameCount(panel, animationName) {
  return [...panel.querySelectorAll(".animation-name")].reduce(
    (count, element) =>
      element.textContent === animationName ? count + 1 : count,
    0
  );
}

function getSummaryGraphPathElement(panel, animationName) {
  for (const animationNameEl of panel.querySelectorAll(".animation-name")) {
    if (animationNameEl.textContent === animationName) {
      return animationNameEl
        .closest(".animation-summary-graph")
        .querySelector(".animation-summary-graph-path");
    }
  }

  return null;
}
