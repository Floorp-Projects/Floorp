/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following EffectTimingPath component works.
// * element existance
// * path

const TEST_CASES = [
  {
    targetClassName: "cssanimation-linear",
  },
  {
    targetClassName: "delay-negative",
  },
  {
    targetClassName: "easing-step",
    expectedPath: [
      { x: 0, y: 0 },
      { x: 49900, y: 0 },
      { x: 50000, y: 50 },
      { x: 99999, y: 50 },
      { x: 100000, y: 0 },
    ],
  },
  {
    targetClassName: "keyframes-easing-step",
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

    info(`Checking effect timing path existance for ${ targetClassName }`);
    const effectTimingPathEl =
      animationItemEl.querySelector(".animation-effect-timing-path");

    if (expectedPath) {
      ok(effectTimingPathEl,
         "The effect timing path element should be in animation item element");
      const pathEl = effectTimingPathEl.querySelector(".animation-iteration-path");
      assertPathSegments(pathEl, false, expectedPath);
    } else {
      ok(!effectTimingPathEl,
         "The effect timing path element should not be in animation item element");
    }
  }
});
