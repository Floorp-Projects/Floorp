/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following AnimationName component works.
// * element existance
// * name text

const TEST_CASES = [
  {
    targetClassName: "cssanimation-normal",
    expectedLabel: "cssanimation",
  },
  {
    targetClassName: "cssanimation-linear",
    expectedLabel: "cssanimation",
  },
  {
    targetClassName: "delay-positive",
    expectedLabel: "test-delay-animation",
  },
  {
    targetClassName: "delay-negative",
    expectedLabel: "test-negative-delay-animation",
  },
  {
    targetClassName: "easing-step",
  },
];

add_task(async function () {
  await addTab(URL_ROOT + "doc_multi_timings.html");

  const { panel } = await openAnimationInspector();

  for (const testCase of TEST_CASES) {
    const {
      expectedLabel,
      targetClassName,
    } = testCase;

    const animationItemEl =
      findAnimationItemElementsByTargetClassName(panel, targetClassName);

    info(`Checking animation name element existance for ${ targetClassName }`);
    const animationNameEl = animationItemEl.querySelector(".animation-name");

    if (expectedLabel) {
      ok(animationNameEl,
         "The animation name element should be in animation item element");
      is(animationNameEl.textContent, expectedLabel,
         `The animation name should be ${ expectedLabel }`);
    } else {
      ok(!animationNameEl,
         "The animation name element should not be in animation item element");
    }
  }
});
