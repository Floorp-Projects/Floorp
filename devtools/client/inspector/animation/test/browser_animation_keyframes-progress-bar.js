/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following KeyframesProgressBar:
// * element existence
// * progress bar position in multi effect timings
// * progress bar position after changing playback rate
// * progress bar position when select another animation

const POSITION_TESTCASES = [
  {
    targetClassName: "cssanimation-linear",
    scrubberPositions: [0, 0.25, 0.5, 0.75, 1],
    expectedPositions: [0, 0.25, 0.5, 0.75, 0],
  },
  {
    targetClassName: "easing-step",
    scrubberPositions: [0, 0.49, 0.5, 0.99],
    expectedPositions: [0, 0, 0.5, 0.5],
  },
  {
    targetClassName: "delay-positive",
    scrubberPositions: [0, 0.33, 0.5],
    expectedPositions: [0, 0, 0.25],
  },
  {
    targetClassName: "delay-negative",
    scrubberPositions: [0, 0.49, 0.5, 0.75],
    expectedPositions: [0, 0, 0.5, 0.75],
  },
  {
    targetClassName: "enddelay-positive",
    scrubberPositions: [0, 0.66, 0.67, 0.99],
    expectedPositions: [0, 0.99, 0, 0],
  },
  {
    targetClassName: "enddelay-negative",
    scrubberPositions: [0, 0.49, 0.5, 0.99],
    expectedPositions: [0, 0.49, 0, 0],
  },
  {
    targetClassName: "direction-reverse-with-iterations-infinity",
    scrubberPositions: [0, 0.25, 0.5, 0.75, 1],
    expectedPositions: [1, 0.75, 0.5, 0.25, 1],
  },
  {
    targetClassName: "fill-both-width-delay-iterationstart",
    scrubberPositions: [0, 0.33, 0.66, 0.833, 1],
    expectedPositions: [0.5, 0.5, 0.99, 0.25, 0.5],
  },
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  const { animationInspector, inspector, panel } = await openAnimationInspector();

  info("Checking progress bar position in multi effect timings");
  await clickOnPauseResumeButton(animationInspector, panel);

  for (const testcase of POSITION_TESTCASES) {
    info(`Checking progress bar position for ${ testcase.targetClassName }`);
    await selectNodeAndWaitForAnimations(`.${ testcase.targetClassName }`, inspector);

    info("Checking progress bar existence");
    const areaEl = panel.querySelector(".keyframes-progress-bar-area");
    ok(areaEl, "progress bar area should exist");
    const barEl = areaEl.querySelector(".keyframes-progress-bar");
    ok(barEl, "progress bar should exist");

    const scrubberPositions = testcase.scrubberPositions;
    const expectedPositions = testcase.expectedPositions;

    for (let i = 0; i < scrubberPositions.length; i++) {
      info(`Scrubber position is ${ scrubberPositions[i] }`);
      await clickOnCurrentTimeScrubberController(animationInspector,
                                                 panel, scrubberPositions[i]);
      assertPosition(barEl, areaEl, expectedPositions[i], animationInspector);
    }
  }
});

function assertPosition(barEl, areaEl, expectedRate, animationInspector) {
  const controllerBounds = areaEl.getBoundingClientRect();
  const barBounds = barEl.getBoundingClientRect();
  const barX = barBounds.x + barBounds.width / 2 - controllerBounds.x;
  const expected = controllerBounds.width * expectedRate;
  ok(expected - 1 < barX && barX < expected + 1,
    `Position should apploximately be ${ expected } (x of bar is ${ barX })`);
}
