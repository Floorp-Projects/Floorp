/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following NegativeEndDelayPath component works.
// * element existance
// * path

const TEST_DATA = [
  {
    targetClass: "enddelay-positive",
  },
  {
    targetClass: "enddelay-negative",
    expectedPath: [
      { x: 500000, y: 0 },
      { x: 500000, y: 50 },
      { x: 750000, y: 75 },
      { x: 1000000, y: 100 },
      { x: 1000000, y: 0 },
    ],
  },
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept(TEST_DATA.map(t => `.${t.targetClass}`));
  const { panel } = await openAnimationInspector();

  for (const { targetClass, expectedPath } of TEST_DATA) {
    const animationItemEl = findAnimationItemElementsByTargetSelector(
      panel,
      `.${targetClass}`
    );

    info(`Checking negative endDelay path existance for ${targetClass}`);
    const negativeEndDelayPathEl = animationItemEl.querySelector(
      ".animation-negative-end-delay-path"
    );

    if (expectedPath) {
      ok(
        negativeEndDelayPathEl,
        "The negative endDelay path element should be in animation item element"
      );
      const pathEl = negativeEndDelayPathEl.querySelector("path");
      assertPathSegments(pathEl, true, expectedPath);
    } else {
      ok(
        !negativeEndDelayPathEl,
        "The negative endDelay path element should not be in animation item element"
      );
    }
  }
});
