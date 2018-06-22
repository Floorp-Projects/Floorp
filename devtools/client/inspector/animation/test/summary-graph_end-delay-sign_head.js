/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

// Test for following EndDelaySign component works.
// * element existance
// * marginInlineStart position
// * width
// * additinal class

const TEST_DATA = [
  {
    targetClass: "enddelay-positive",
    expectedResult: {
      marginInlineStart: "75%",
      width: "25%",
    },
  },
  {
    targetClass: "enddelay-negative",
    expectedResult: {
      additionalClass: "negative",
      marginInlineStart: "50%",
      width: "25%",
    },
  },
  {
    targetClass: "enddelay-with-fill-forwards",
    expectedResult: {
      additionalClass: "fill",
      marginInlineStart: "75%",
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

// eslint-disable-next-line no-unused-vars
async function testSummaryGraphEndDelaySign() {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept(TEST_DATA.map(t => `.${ t.targetClass }`));
  const { panel } = await openAnimationInspector();

  for (const { targetClass, expectedResult } of TEST_DATA) {
    const animationItemEl =
      findAnimationItemElementsByTargetSelector(panel, `.${ targetClass }`);

    info(`Checking endDelay sign existance for ${ targetClass }`);
    const endDelaySignEl = animationItemEl.querySelector(".animation-end-delay-sign");

    if (expectedResult) {
      ok(endDelaySignEl, "The endDelay sign element should be in animation item element");

      is(endDelaySignEl.style.marginInlineStart, expectedResult.marginInlineStart,
        `marginInlineStart position should be ${ expectedResult.marginInlineStart }`);
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
}
