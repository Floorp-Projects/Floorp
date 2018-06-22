/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

// Test for following DelaySign component works.
// * element existance
// * marginInlineStart position
// * width
// * additinal class

const TEST_DATA = [
  {
    targetClass: "delay-positive",
    expectedResult: {
      marginInlineStart: "25%",
      width: "25%",
    },
  },
  {
    targetClass: "delay-negative",
    expectedResult: {
      additionalClass: "negative",
      marginInlineStart: "0%",
      width: "25%",
    },
  },
  {
    targetClass: "fill-backwards-with-delay-iterationstart",
    expectedResult: {
      additionalClass: "fill",
      marginInlineStart: "25%",
      width: "25%",
    },
  },
  {
    targetClass: "fill-both",
  },
  {
    targetClass: "fill-both-width-delay-iterationstart",
    expectedResult: {
      additionalClass: "fill",
      marginInlineStart: "25%",
      width: "25%",
    },
  },
  {
    targetClass: "keyframes-easing-step",
  },
];

// eslint-disable-next-line no-unused-vars
async function testSummaryGraphDelaySign() {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept(TEST_DATA.map(t => `.${ t.targetClass }`));
  const { panel } = await openAnimationInspector();

  for (const { targetClass, expectedResult } of TEST_DATA) {
    const animationItemEl =
      findAnimationItemElementsByTargetSelector(panel, `.${ targetClass }`);

    info(`Checking delay sign existance for ${ targetClass }`);
    const delaySignEl = animationItemEl.querySelector(".animation-delay-sign");

    if (expectedResult) {
      ok(delaySignEl, "The delay sign element should be in animation item element");

      is(delaySignEl.style.marginInlineStart, expectedResult.marginInlineStart,
        `marginInlineStart position should be ${ expectedResult.marginInlineStart }`);
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
}
