/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that animation delay is visualized in the timeline when the animation
// is delayed.
// Also check that negative delays do not overflow the UI, and are shown like
// positive delays.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Selecting a delayed animated node");
  yield selectNodeAndWaitForAnimations(".delayed", inspector);
  let timelineEl = panel.animationsTimelineComponent.rootWrapperEl;
  checkDelayAndName(timelineEl, true);
  let animationEl = timelineEl.querySelector(".animation");
  let state = panel.animationsTimelineComponent.timeBlocks[0].animation.state;
  checkPath(animationEl, state);

  info("Selecting a no-delay animated node");
  yield selectNodeAndWaitForAnimations(".animated", inspector);
  checkDelayAndName(timelineEl, false);
  animationEl = timelineEl.querySelector(".animation");
  state = panel.animationsTimelineComponent.timeBlocks[0].animation.state;
  checkPath(animationEl, state);

  info("Selecting a negative-delay animated node");
  yield selectNodeAndWaitForAnimations(".negative-delay", inspector);
  checkDelayAndName(timelineEl, true);
  animationEl = timelineEl.querySelector(".animation");
  state = panel.animationsTimelineComponent.timeBlocks[0].animation.state;
  checkPath(animationEl, state);
});

function checkDelayAndName(timelineEl, hasDelay) {
  let delay = timelineEl.querySelector(".delay");

  is(!!delay, hasDelay, "The timeline " +
                        (hasDelay ? "contains" : "does not contain") +
                        " a delay element, as expected");

  if (hasDelay) {
    let name = timelineEl.querySelector(".name");
    let targetNode = timelineEl.querySelector(".target");

    // Check that the delay element does not cause the timeline to overflow.
    let delayLeft = Math.round(delay.getBoundingClientRect().x);
    let sidebarWidth = Math.round(targetNode.getBoundingClientRect().width);
    ok(delayLeft >= sidebarWidth,
       "The delay element isn't displayed over the sidebar");

    // Check that the delay is not displayed on top of the name.
    let delayRight = Math.round(delay.getBoundingClientRect().right);
    let nameLeft = Math.round(name.getBoundingClientRect().left);
    ok(delayRight <= nameLeft,
       "The delay element does not span over the name element");
  }
}

function checkPath(animationEl, state) {
  // Check existance of delay path.
  const delayPathEl = animationEl.querySelector(".delay-path");
  if (!state.iterationCount && state.delay < 0) {
    // Infinity
    ok(!delayPathEl, "The delay path for Infinity should not exist");
    return;
  }
  if (state.delay === 0) {
    ok(!delayPathEl, "The delay path for zero delay should not exist");
    return;
  }
  ok(delayPathEl, "The delay path should exist");

  // Check delay path coordinates.
  const pathSegList = delayPathEl.pathSegList;
  const startingPathSeg = pathSegList.getItem(0);
  const endingPathSeg = pathSegList.getItem(pathSegList.numberOfItems - 2);
  if (state.delay < 0) {
    ok(delayPathEl.classList.contains("negative"),
       "The delay path should have 'negative' class");
    const startingX = state.delay;
    const endingX = 0;
    is(startingPathSeg.x, startingX,
       `The x of starting point should be ${ startingX }`);
    is(endingPathSeg.x, endingX,
       `The x of ending point should be ${ endingX }`);
  } else {
    ok(!delayPathEl.classList.contains("negative"),
       "The delay path should not have 'negative' class");
    const startingX = 0;
    const endingX = state.delay;
    is(startingPathSeg.x, startingX,
       `The x of starting point should be ${ startingX }`);
    is(endingPathSeg.x, endingX,
       `The x of ending point should be ${ endingX }`);
  }
}
