/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that animation delay is visualized in the timeline when the animation
// is delayed.
// Also check that negative delays do not overflow the UI, and are shown like
// positive delays.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  const {inspector, panel} = await openAnimationInspector();

  info("Selecting a delayed animated node");
  await selectNodeAndWaitForAnimations(".delayed", inspector);
  const timelineEl = panel.animationsTimelineComponent.rootWrapperEl;
  checkDelayAndName(timelineEl, true);
  let animationEl = timelineEl.querySelector(".animation");
  let state = getAnimationTimeBlocks(panel)[0].animation.state;
  checkPath(animationEl, state);

  info("Selecting a no-delay animated node");
  await selectNodeAndWaitForAnimations(".animated", inspector);
  checkDelayAndName(timelineEl, false);
  animationEl = timelineEl.querySelector(".animation");
  state = getAnimationTimeBlocks(panel)[0].animation.state;
  checkPath(animationEl, state);

  info("Selecting a negative-delay animated node");
  await selectNodeAndWaitForAnimations(".negative-delay", inspector);
  checkDelayAndName(timelineEl, true);
  animationEl = timelineEl.querySelector(".animation");
  state = getAnimationTimeBlocks(panel)[0].animation.state;
  checkPath(animationEl, state);
});

function checkDelayAndName(timelineEl, hasDelay) {
  const delay = timelineEl.querySelector(".delay");

  is(!!delay, hasDelay, "The timeline " +
                        (hasDelay ? "contains" : "does not contain") +
                        " a delay element, as expected");

  if (hasDelay) {
    const targetNode = timelineEl.querySelector(".target");

    // Check that the delay element does not cause the timeline to overflow.
    const delayLeft = Math.round(delay.getBoundingClientRect().x);
    const sidebarWidth = Math.round(targetNode.getBoundingClientRect().width);
    ok(delayLeft >= sidebarWidth,
       "The delay element isn't displayed over the sidebar");
  }
}

function checkPath(animationEl, state) {
  const groupEls = animationEl.querySelectorAll("svg g");
  groupEls.forEach(groupEl => {
    // Check existance of delay path.
    const delayPathEl = groupEl.querySelector(".delay-path");
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
      const expectedY = 0;
      const startingX = state.delay;
      const endingX = 0;
      is(startingPathSeg.x, startingX,
         `The x of starting point should be ${ startingX }`);
      is(startingPathSeg.y, expectedY,
         `The y of starting point should be ${ expectedY }`);
      is(endingPathSeg.x, endingX,
         `The x of ending point should be ${ endingX }`);
      is(endingPathSeg.y, expectedY,
         `The y of ending point should be ${ expectedY }`);
    } else {
      ok(!delayPathEl.classList.contains("negative"),
         "The delay path should not have 'negative' class");
      const expectedY = 0;
      const startingX = 0;
      const endingX = state.delay;
      is(startingPathSeg.x, startingX,
         `The x of starting point should be ${ startingX }`);
      is(startingPathSeg.y, expectedY,
         `The y of starting point should be ${ expectedY }`);
      is(endingPathSeg.x, endingX,
         `The x of ending point should be ${ endingX }`);
      is(endingPathSeg.y, expectedY,
         `The y of ending point should be ${ expectedY }`);
    }
  });
}
