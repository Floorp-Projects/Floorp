/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following EndDelaySign component works.
// * element existance
// * left position
// * width
// * additinal class

const TEST_CASES = [
  {
    targetClassName: "enddelay-positive",
    expectedResult: {
      left: "75%",
      width: "25%",
    },
  },
  {
    targetClassName: "enddelay-negative",
    expectedResult: {
      additionalClass: "negative",
      left: "50%",
      width: "25%",
    },
  },
  {
    targetClassName: "enddelay-with-fill-forwards",
    expectedResult: {
      additionalClass: "fill",
      left: "75%",
      width: "25%",
    },
  },
  {
    targetClassName: "enddelay-with-iterations-infinity",
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

    info(`Checking endDelay sign existance for ${ targetClassName }`);
    const endDelaySignEl = animationItemEl.querySelector(".animation-end-delay-sign");

    if (expectedResult) {
      ok(endDelaySignEl, "The endDelay sign element should be in animation item element");

      is(endDelaySignEl.style.left, expectedResult.left,
         `Left position should be ${ expectedResult.left }`);
      is(endDelaySignEl.style.width, expectedResult.width,
         `Width should be ${ expectedResult.width }`);

      if (expectedResult.additionalClass) {
        ok(endDelaySignEl.classList.contains(expectedResult.additionalClass),
           `endDelay sign element should have ${ expectedResult.additionalClass } class`);
      } else {
        ok(!endDelaySignEl.classList.contains(expectedResult.additionalClass),
           "endDelay sign element should not have " +
           `${ expectedResult.additionalClass } class`);
      }
    } else {
      ok(!endDelaySignEl,
         "The endDelay sign element should not be in animation item element");
    }
  }
});
