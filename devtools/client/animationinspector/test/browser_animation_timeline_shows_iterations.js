/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the timeline is displays as many iteration elements as there are
// iterations in an animation.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Selecting the test node");
  yield selectNode(".delayed", inspector);

  info("Getting the animation element from the panel");
  let timelineEl = panel.animationsTimelineComponent.rootWrapperEl;
  let animation = timelineEl.querySelector(".time-block");
  let iterations = animation.querySelector(".iterations");

  // Iterations are rendered with a repeating linear-gradient, so we need to
  // calculate how many iterations are represented by looking at the background
  // size.
  let iterationCount = getIterationCountFromBackground(iterations);

  is(iterationCount, 10,
     "The animation timeline contains the right number of iterations");
  ok(!iterations.classList.contains("infinite"),
     "The iteration element doesn't have the infinite class");

  info("Selecting another test node with an infinite animation");
  yield selectNode(".animated", inspector);

  info("Getting the animation element from the panel again");
  animation = timelineEl.querySelector(".time-block");
  iterations = animation.querySelector(".iterations");

  iterationCount = getIterationCountFromBackground(iterations);

  is(iterationCount, 1,
     "The animation timeline contains just one iteration");
  ok(iterations.classList.contains("infinite"),
     "The iteration element has the infinite class");
});

function getIterationCountFromBackground(el) {
  let backgroundSize = parseFloat(el.style.backgroundSize.split(" ")[0]);
  let width = el.offsetWidth;
  return Math.round(width / backgroundSize);
}
