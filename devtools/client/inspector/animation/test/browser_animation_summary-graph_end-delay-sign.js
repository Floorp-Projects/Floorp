/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following EndDelaySign component works.
// * element existance
// * left position
// * width
// * additinal class

const TEST_DATA = [
  {
    targetClass: "enddelay-positive",
    expectedResult: {
      left: "75%",
      width: "25%",
    },
  },
  {
    targetClass: "enddelay-negative",
    expectedResult: {
      additionalClass: "negative",
      left: "50%",
      width: "25%",
    },
  },
  {
    targetClass: "enddelay-with-fill-forwards",
    expectedResult: {
      additionalClass: "fill",
      left: "75%",
      width: "25%",
    },
  },
  {
    targetClass: "enddelay-with-iterations-infinity",
  },
  {
    targetClass: "delay-negative",
  },
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept(TEST_DATA.map(t => `.${ t.targetClass }`));
  const { panel } = await openAnimationInspector();

  for (const { targetClass, expectedResult } of TEST_DATA) {
    const animationItemEl =
      findAnimationItemElementsByTargetClassName(panel, targetClass);

    info(`Checking endDelay sign existance for ${ targetClass }`);
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
