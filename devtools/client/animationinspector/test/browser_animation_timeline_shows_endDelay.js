/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that animation endDelay is visualized in the timeline when the
// animation is delayed.
// Also check that negative endDelays do not overflow the UI, and are shown
// like positive endDelays.

add_task(async function() {
  await addTab(URL_ROOT + "doc_end_delay.html");
  let {inspector, panel} = await openAnimationInspector();

  let selectors = ["#target1", "#target2", "#target3", "#target4"];
  for (let i = 0; i < selectors.length; i++) {
    let selector = selectors[i];
    await selectNode(selector, inspector);
    await waitForAnimationSelecting(panel);
    let timelineEl = panel.animationsTimelineComponent.rootWrapperEl;
    let animationEl = timelineEl.querySelector(".animation");
    checkEndDelayAndName(animationEl);
    const state = getAnimationTimeBlocks(panel)[0].animation.state;
    checkPath(animationEl, state);
  }
});

function checkEndDelayAndName(animationEl) {
  let endDelay = animationEl.querySelector(".end-delay");
  let name = animationEl.querySelector(".name");
  let targetNode = animationEl.querySelector(".target");

  // Check that the endDelay element does not cause the timeline to overflow.
  let endDelayLeft = Math.round(endDelay.getBoundingClientRect().x);
  let sidebarWidth = Math.round(targetNode.getBoundingClientRect().width);
  ok(endDelayLeft >= sidebarWidth,
     "The endDelay element isn't displayed over the sidebar");

  // Check that the endDelay is not displayed on top of the name.
  let endDelayRight = Math.round(endDelay.getBoundingClientRect().right);
  let nameLeft = Math.round(name.getBoundingClientRect().left);
  ok(endDelayRight >= nameLeft,
     "The endDelay element does not span over the name element");
}

function checkPath(animationEl, state) {
  const groupEls = animationEl.querySelectorAll("svg g");
  groupEls.forEach(groupEl => {
    // Check existance of enddelay path.
    const endDelayPathEl = groupEl.querySelector(".enddelay-path");
    ok(endDelayPathEl, "The endDelay path should exist");

    // Check enddelay path coordinates.
    const pathSegList = endDelayPathEl.pathSegList;
    const startingPathSeg = pathSegList.getItem(0);
    const endingPathSeg = pathSegList.getItem(pathSegList.numberOfItems - 2);
    if (state.endDelay < 0) {
      ok(endDelayPathEl.classList.contains("negative"),
         "The endDelay path should have 'negative' class");
      const endingX = state.delay + state.iterationCount * state.duration;
      const startingX = endingX + state.endDelay;
      is(startingPathSeg.x, startingX,
         `The x of starting point should be ${ startingX }`);
      is(endingPathSeg.x, endingX,
         `The x of ending point should be ${ endingX }`);
    } else {
      ok(!endDelayPathEl.classList.contains("negative"),
         "The endDelay path should not have 'negative' class");
      const startingX =
        state.delay + state.iterationCount * state.duration;
      const endingX = startingX + state.endDelay;
      is(startingPathSeg.x, startingX,
         `The x of starting point should be ${ startingX }`);
      is(endingPathSeg.x, endingX,
         `The x of ending point should be ${ endingX }`);
    }
  });
}
