/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test short duration (e.g. 1ms) animation.

add_task(async function() {
  await addTab(URL_ROOT + "doc_short_duration_animation.html");
  const { panel, inspector } = await openAnimationInspector();

  info("Check the listed time blocks");
  const timeBlocks = getAnimationTimeBlocks(panel);
  for (let i = 0; i < timeBlocks.length; i++) {
    info(`Check the time block ${i}`);
    const {containerEl, animation: {state}} = timeBlocks[i];
    checkSummaryGraph(containerEl, state);
  }

  info("Check the time block one by one");
  info("Check #onetime");
  await selectNodeAndWaitForAnimations("#onetime", inspector);
  let timeBlock = getAnimationTimeBlocks(panel)[0];
  let containerEl = timeBlock.containerEl;
  let state = timeBlock.animation.state;
  checkSummaryGraph(containerEl, state, true);

  info("Check #infinite");
  await selectNodeAndWaitForAnimations("#infinite", inspector);
  timeBlock = getAnimationTimeBlocks(panel)[0];
  containerEl = timeBlock.containerEl;
  state = timeBlock.animation.state;
  checkSummaryGraph(containerEl, state, true);
});

function checkSummaryGraph(el, state, isDetail) {
  info("Check the coordinates of summary graph");
  const groupEls = el.querySelectorAll("svg g");
  groupEls.forEach(groupEl => {
    const pathEls = groupEl.querySelectorAll(".iteration-path");
    let expectedIterationCount = 0;
    if (isDetail) {
      expectedIterationCount = state.iterationCount ? state.iterationCount : 1;
    } else {
      expectedIterationCount = state.iterationCount ? state.iterationCount : 2;
    }
    is(pathEls.length, expectedIterationCount,
       `The count of path shoud be ${ expectedIterationCount }`);
    pathEls.forEach((pathEl, index) => {
      const startX = index * state.duration;
      const endX = startX + state.duration;

      const pathSegList = pathEl.pathSegList;
      const firstPathSeg = pathSegList.getItem(0);
      is(firstPathSeg.x, startX,
         `The x of first segment should be ${ startX }`);
      is(firstPathSeg.y, 0, "The y of first segment should be 0");

      // The easing of test animation is 'linear'.
      // Therefore, the y of second path segment will be 0.
      const secondPathSeg = pathSegList.getItem(1);
      is(secondPathSeg.x, startX,
         `The x of second segment should be ${ startX }`);
      is(secondPathSeg.y, 0, "The y of second segment should be 0");

      const thirdLastPathSeg = pathSegList.getItem(pathSegList.numberOfItems - 4);
      approximate(thirdLastPathSeg.x, endX - 0.001, 0.005,
                  "The x of third last segment should be approximately "
                  + (endX - 0.001));
      approximate(thirdLastPathSeg.y, 0.999, 0.005,
                  " The y of third last segment should be approximately "
                  + thirdLastPathSeg.x);

      // The test animation is not 'forwards' fill-mode.
      // Therefore, the y of second last path segment will be 0.
      const secondLastPathSeg =
        pathSegList.getItem(pathSegList.numberOfItems - 3);
      is(secondLastPathSeg.x, endX,
         `The x of second last segment should be ${ endX }`);
      // We use computed style of 'opacity' to create summary graph.
      // So, if currentTime is same to the duration, although progress is null
      // opacity is 0.
      const expectedY = 0;
      is(secondLastPathSeg.y, expectedY,
         `The y of second last segment should be ${ expectedY }`);

      const lastPathSeg = pathSegList.getItem(pathSegList.numberOfItems - 2);
      is(lastPathSeg.x, endX, `The x of last segment should be ${ endX }`);
      is(lastPathSeg.y, 0, "The y of last segment should be 0");

      const closePathSeg = pathSegList.getItem(pathSegList.numberOfItems - 1);
      is(closePathSeg.pathSegType, closePathSeg.PATHSEG_CLOSEPATH,
         `The actual last segment should be close path`);
    });
  });
}

function approximate(value, expected, permissibleRange, message) {
  const min = expected - permissibleRange;
  const max = expected + permissibleRange;
  ok(min <= value && value <= max, message);
}
