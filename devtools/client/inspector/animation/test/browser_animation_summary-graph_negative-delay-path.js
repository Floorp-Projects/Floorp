/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following NegativeDelayPath component works.
// * element existance
// * path

const TEST_CASES = [
  {
    targetClassName: "delay-positive",
  },
  {
    targetClassName: "delay-negative",
    expectedPath: [
      { x: -50000, y: 0 },
      { x: -25000, y: 25 },
      { x: 0, y: 50 },
      { x: 0, y: 0 },
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

    info(`Checking negative delay path existence for ${ targetClassName }`);
    const negativeDelayPathEl =
      animationItemEl.querySelector(".animation-negative-delay-path");

    if (expectedPath) {
      ok(negativeDelayPathEl,
         "The negative delay path element should be in animation item element");
      const pathEl = negativeDelayPathEl.querySelector("path");
      assertPathSegments(pathEl, true, expectedPath);
    } else {
      ok(!negativeDelayPathEl,
         "The negative delay path element should not be in animation item element");
    }
  }
});
