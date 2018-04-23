/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the iteration start is displayed correctly in time blocks.

add_task(async function() {
  await addTab(URL_ROOT + "doc_script_animation.html");
  let {panel} = await openAnimationInspector();
  let timelineComponent = panel.animationsTimelineComponent;
  let timeBlockComponents = getAnimationTimeBlocks(panel);
  let detailsComponent = timelineComponent.details;

  for (let i = 0; i < timeBlockComponents.length; i++) {
    info(`Expand time block ${i} so its keyframes are visible`);
    await clickOnAnimation(panel, i);

    info(`Check the state of time block ${i}`);
    let {containerEl, animation: {state}} = timeBlockComponents[i];

    checkAnimationTooltip(containerEl, state);
    checkProgressAtStartingTime(containerEl, state);

    // Get the first set of keyframes (there's only one animated property
    // anyway), and the first frame element from there, we're only interested in
    // its offset.
    let keyframeComponent = detailsComponent.keyframeComponents[0];
    let frameEl = keyframeComponent.keyframesEl.querySelector(".frame");
    checkKeyframeOffset(containerEl, frameEl, state);
  }
});

function checkAnimationTooltip(el, {iterationStart, duration}) {
  info("Check an animation's iterationStart data in its tooltip");
  let title = el.querySelector(".name").getAttribute("title");

  let iterationStartTime = iterationStart * duration / 1000;
  let iterationStartTimeString = iterationStartTime.toLocaleString(undefined, {
    maximumFractionDigits: 2,
    minimumFractionDigits: 2
  }).replace(".", "\\.");
  let iterationStartString = iterationStart.toString().replace(".", "\\.");

  let regex = new RegExp("Iteration start: " + iterationStartString +
                         " \\(" + iterationStartTimeString + "s\\)");
  ok(title.match(regex), "The tooltip shows the expected iteration start");
}

function checkProgressAtStartingTime(el, { delay, iterationStart }) {
  info("Check the progress of starting time");
  const groupEls = el.querySelectorAll("svg g");
  groupEls.forEach(groupEl => {
    const pathEl = groupEl.querySelector(".iteration-path");
    const pathSegList = pathEl.pathSegList;
    const pathSeg = pathSegList.getItem(1);
    const progress = pathSeg.y;
    is(progress, iterationStart % 1,
       `The progress at starting point should be ${ iterationStart % 1 }`);

    if (delay) {
      const delayPathEl = groupEl.querySelector(".delay-path");
      const delayPathSegList = delayPathEl.pathSegList;
      const delayStartingPathSeg = delayPathSegList.getItem(1);
      const delayEndingPathSeg =
        delayPathSegList.getItem(delayPathSegList.numberOfItems - 2);
      const startingX = 0;
      const endingX = delay;
      is(delayStartingPathSeg.x, startingX,
         `The x of starting point should be ${ startingX }`);
      is(delayStartingPathSeg.y, progress,
         "The y of starting point should be same to starting point of iteration-path "
         + progress);
      is(delayEndingPathSeg.x, endingX,
         `The x of ending point should be ${ endingX }`);
      is(delayStartingPathSeg.y, progress,
         "The y of ending point should be same to starting point of iteration-path "
         + progress);
    }
  });
}

function checkKeyframeOffset(timeBlockEl, frameEl, {iterationStart}) {
  info("Check that the first keyframe is offset correctly");

  let start = getKeyframeOffset(frameEl);
  is(start, 0, "The frame offset for iteration start");
}

function getKeyframeOffset(el) {
  return parseFloat(/(\d+)%/.exec(el.style.left)[1]);
}
