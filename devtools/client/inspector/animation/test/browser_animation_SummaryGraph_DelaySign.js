/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following DelaySign component works.
// * element existance
// * left position
// * width
// * additinal class

const TEST_CASES = [
  {
    targetClassName: "delay-positive",
    expectedResult: {
      left: "25%",
      width: "25%",
    },
  },
  {
    targetClassName: "delay-negative",
    expectedResult: {
      additionalClass: "negative",
      left: "0%",
      width: "25%",
    },
  },
  {
    targetClassName: "fill-backwards-with-delay-iterationstart",
    expectedResult: {
      additionalClass: "fill",
      left: "25%",
      width: "25%",
    },
  },
  {
    targetClassName: "fill-both",
  },
  {
    targetClassName: "fill-both-width-delay-iterationstart",
    expectedResult: {
      additionalClass: "fill",
      left: "25%",
      width: "25%",
    },
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
      expectedResult,
      targetClassName,
    } = testCase;

    const animationItemEl =
      findAnimationItemElementsByTargetClassName(panel, targetClassName);

    info(`Checking delay sign existance for ${ targetClassName }`);
    const delaySignEl = animationItemEl.querySelector(".animation-delay-sign");

    if (expectedResult) {
      ok(delaySignEl, "The delay sign element should be in animation item element");

      is(delaySignEl.style.left, expectedResult.left,
         `Left position should be ${ expectedResult.left }`);
      is(delaySignEl.style.width, expectedResult.width,
         `Width should be ${ expectedResult.width }`);

      if (expectedResult.additionalClass) {
        ok(delaySignEl.classList.contains(expectedResult.additionalClass),
           `delay sign element should have ${ expectedResult.additionalClass } class`);
      } else {
        ok(!delaySignEl.classList.contains(expectedResult.additionalClass),
           "delay sign element should not have " +
           `${ expectedResult.additionalClass } class`);
      }
    } else {
      ok(!delaySignEl, "The delay sign element should not be in animation item element");
    }
  }
});
