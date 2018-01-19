/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following NegativeEndDelayPath component works.
// * element existance
// * path

const TEST_CASES = [
  {
    targetClassName: "enddelay-positive",
  },
  {
    targetClassName: "enddelay-negative",
    expectedPath: [
      { x: 50000, y: 0 },
      { x: 50000, y: 50 },
      { x: 75000, y: 75 },
      { x: 100000, y: 100 },
      { x: 100000, y: 0 },
    ],
  },
];

add_task(async function () {
  await addTab(URL_ROOT + "doc_multi_timings.html");

  const { panel } = await openAnimationInspector();

  for (const testCase of TEST_CASES) {
    const {
      expectedPath,
      targetClassName,
    } = testCase;

    const animationItemEl =
      findAnimationItemElementsByTargetClassName(panel, targetClassName);

    info(`Checking negative endDelay path existance for ${ targetClassName }`);
    const negativeEndDelayPathEl =
      animationItemEl.querySelector(".animation-negative-end-delay-path");

    if (expectedPath) {
      ok(negativeEndDelayPathEl,
         "The negative endDelay path element should be in animation item element");
      const pathEl = negativeEndDelayPathEl.querySelector("path");
      assertPathSegments(pathEl, true, expectedPath);
    } else {
      ok(!negativeEndDelayPathEl,
         "The negative endDelay path element should not be in animation item element");
    }
  }
});
