/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following AnimationName component works.
// * element existance
// * name text

const TEST_DATA = [
  {
    targetClass: "cssanimation-normal",
    expectedLabel: "cssanimation",
  },
  {
    targetClass: "cssanimation-linear",
    expectedLabel: "cssanimation",
  },
  {
    targetClass: "delay-positive",
    expectedLabel: "test-delay-animation",
  },
  {
    targetClass: "delay-negative",
    expectedLabel: "test-negative-delay-animation",
  },
  {
    targetClass: "easing-step",
  },
];

add_task(async function () {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept(TEST_DATA.map(t => `.${t.targetClass}`));
  const { panel } = await openAnimationInspector();

  for (const { targetClass, expectedLabel } of TEST_DATA) {
    const animationItemEl = await findAnimationItemByTargetSelector(
      panel,
      `.${targetClass}`
    );

    info(`Checking animation name element existance for ${targetClass}`);
    const animationNameEl = animationItemEl.querySelector(".animation-name");

    if (expectedLabel) {
      ok(
        animationNameEl,
        "The animation name element should be in animation item element"
      );
      is(
        animationNameEl.textContent,
        expectedLabel,
        `The animation name should be ${expectedLabel}`
      );
    } else {
      ok(
        !animationNameEl,
        "The animation name element should not be in animation item element"
      );
    }
  }
});
