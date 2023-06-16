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
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 0, y: 50 },
        { x: 250000, y: 75 },
        { x: 500000, y: 100 },
        { x: 500000, y: 0 },
      ],
    ],
    expectedNegativePath: [
      { x: -500000, y: 0 },
      { x: -250000, y: 25 },
      { x: 0, y: 50 },
      { x: 0, y: 0 },
    ],
  },
  {
    targetClass: "delay-negative-25",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 0, y: 25 },
        { x: 750000, y: 100 },
        { x: 750000, y: 0 },
      ],
    ],
    expectedNegativePath: [
      { x: -250000, y: 0 },
      { x: 0, y: 25 },
      { x: 0, y: 0 },
    ],
  },
  {
    targetClass: "delay-negative-75",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 0, y: 75 },
        { x: 250000, y: 100 },
        { x: 250000, y: 0 },
      ],
    ],
    expectedNegativePath: [
      { x: -750000, y: 0 },
      { x: 0, y: 75 },
      { x: 0, y: 0 },
    ],
  },
];

add_task(async function () {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept(TEST_DATA.map(t => `.${t.targetClass}`));
  const { panel } = await openAnimationInspector();

  for (const {
    targetClass,
    expectedIterationPathList,
    expectedNegativePath,
  } of TEST_DATA) {
    const animationItemEl = await findAnimationItemByTargetSelector(
      panel,
      `.${targetClass}`
    );

    info(`Checking negative delay path existence for ${targetClass}`);
    const negativeDelayPathEl = animationItemEl.querySelector(
      ".animation-negative-delay-path"
    );

    if (expectedNegativePath) {
      ok(
        negativeDelayPathEl,
        "The negative delay path element should be in animation item element"
      );
      const pathEl = negativeDelayPathEl.querySelector("path");
      assertPathSegments(pathEl, true, expectedNegativePath);
    } else {
      ok(
        !negativeDelayPathEl,
        "The negative delay path element should not be in animation item element"
      );
    }

    if (!expectedIterationPathList) {
      // We don't need to test for iteration path.
      continue;
    }

    info(`Checking computed timing path existance for ${targetClass}`);
    const computedTimingPathEl = animationItemEl.querySelector(
      ".animation-computed-timing-path"
    );
    ok(
      computedTimingPathEl,
      "The computed timing path element should be in each animation item element"
    );

    info(`Checking iteration path list for ${targetClass}`);
    const iterationPathEls = computedTimingPathEl.querySelectorAll(
      ".animation-iteration-path"
    );
    is(
      iterationPathEls.length,
      expectedIterationPathList.length,
      `Number of iteration path should be ${expectedIterationPathList.length}`
    );

    for (const [j, iterationPathEl] of iterationPathEls.entries()) {
      assertPathSegments(iterationPathEl, true, expectedIterationPathList[j]);
    }
  }
});
