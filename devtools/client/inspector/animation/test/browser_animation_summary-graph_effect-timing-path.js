/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following EffectTimingPath component works.
// * element existance
// * path

const TEST_DATA = [
  {
    targetClass: "cssanimation-linear",
  },
  {
    targetClass: "delay-negative",
  },
  {
    targetClass: "easing-step",
    expectedPath: [
      { x: 0, y: 0 },
      { x: 499999, y: 0 },
      { x: 500000, y: 50 },
      { x: 999999, y: 50 },
      { x: 1000000, y: 0 },
    ],
  },
  {
    targetClass: "keyframes-easing-step",
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

    info(`Checking effect timing path existance for ${targetClass}`);
    const effectTimingPathEl = animationItemEl.querySelector(
      ".animation-effect-timing-path"
    );

    if (expectedPath) {
      ok(
        effectTimingPathEl,
        "The effect timing path element should be in animation item element"
      );
      const pathEl = effectTimingPathEl.querySelector(
        ".animation-iteration-path"
      );
      assertPathSegments(pathEl, false, expectedPath);
    } else {
      ok(
        !effectTimingPathEl,
        "The effect timing path element should not be in animation item element"
      );
    }
  }
});
