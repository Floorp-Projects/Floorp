/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the iteration start is displayed correctly in time blocks.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_script_animation.html");
  let {panel} = yield openAnimationInspector();
  let timelineComponent = panel.animationsTimelineComponent;
  let timeBlockComponents = timelineComponent.timeBlocks;
  let detailsComponents = timelineComponent.details;

  for (let i = 0; i < timeBlockComponents.length; i++) {
    info(`Expand time block ${i} so its keyframes are visible`);
    yield clickOnAnimation(panel, i);

    info(`Check the state of time block ${i}`);
    let {containerEl, animation: {state}} = timeBlockComponents[i];

    checkAnimationTooltip(containerEl, state);
    checkIterationBackground(containerEl, state);

    // Get the first set of keyframes (there's only one animated property
    // anyway), and the first frame element from there, we're only interested in
    // its offset.
    let keyframeComponent = detailsComponents[i].keyframeComponents[0];
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

function checkIterationBackground(el, {iterationCount, iterationStart}) {
  info("Check the background-image used to display iterations is offset " +
       "correctly to represent the iterationStart");

  let iterationsEl = el.querySelector(".iterations");
  let start = getIterationStartFromBackground(iterationsEl, iterationCount);
  is(start, iterationStart % 1,
     "The right background-position for iteration start");
}

function getIterationStartFromBackground(el, iterationCount) {
  if (iterationCount == 1) {
    let size = parseFloat(/([.\d]+)%/.exec(el.style.backgroundSize)[1]);
    return 1 - size / 100;
  }

  let size = parseFloat(/([.\d]+)%/.exec(el.style.backgroundSize)[1]);
  let position = parseFloat(/([-\d]+)%/.exec(el.style.backgroundPosition)[1]);
  let iterationStartW = -position / size * (100 - size);
  let rounded = Math.round(iterationStartW * 100);
  return rounded / 10000;
}

function checkKeyframeOffset(timeBlockEl, frameEl, {iterationStart}) {
  info("Check that the first keyframe is offset correctly");

  let start = getIterationStartFromLeft(frameEl);
  is(start, iterationStart % 1, "The frame offset for iteration start");
}

function getIterationStartFromLeft(el) {
  let left = 100 - parseFloat(/(\d+)%/.exec(el.style.left)[1]);
  return left / 100;
}
