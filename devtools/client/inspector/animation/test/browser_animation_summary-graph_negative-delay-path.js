/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following NegativeDelayPath component works.
// * element existance
// * path

const TEST_DATA = [
  {
    targetClass: "delay-positive",
  },
  {
    targetClass: "delay-negative",
    expectedPath: [
      { x: -500000, y: 0 },
      { x: -250000, y: 25 },
      { x: 0, y: 50 },
      { x: 0, y: 0 },
    ],
  },
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_multi_timings.html");

  const { panel } = await openAnimationInspector();

  for (const { targetClass, expectedPath } of TEST_DATA) {
    const animationItemEl =
      findAnimationItemElementsByTargetClassName(panel, targetClass);

    info(`Checking negative delay path existence for ${ targetClass }`);
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
