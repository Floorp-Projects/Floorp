/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following ComputedTimingPath component works.
// * element existance
// * iterations: path, count
// * delay: path
// * fill: path
// * endDelay: path

const TEST_DATA = [
  {
    targetClass: "cssanimation-normal",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 250000, y: 40.851 },
        { x: 500000, y: 80.24 },
        { x: 750000, y: 96.05 },
        { x: 1000000, y: 100 },
        { x: 1000000, y: 0 },
      ],
    ],
  },
  {
    targetClass: "cssanimation-linear",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 250000, y: 25 },
        { x: 500000, y: 50 },
        { x: 750000, y: 75 },
        { x: 1000000, y: 100 },
        { x: 1000000, y: 0 },
      ],
    ],
  },
  {
    targetClass: "delay-positive",
    expectedDelayPath: [{ x: 0, y: 0 }, { x: 500000, y: 0 }],
    expectedIterationPathList: [
      [
        { x: 500000, y: 0 },
        { x: 750000, y: 25 },
        { x: 1000000, y: 50 },
        { x: 1250000, y: 75 },
        { x: 1500000, y: 100 },
        { x: 1500000, y: 0 },
      ],
    ],
  },
  {
    targetClass: "easing-step",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 499999, y: 0 },
        { x: 500000, y: 50 },
        { x: 999999, y: 50 },
        { x: 1000000, y: 0 },
      ],
    ],
  },
  {
    targetClass: "enddelay-positive",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 250000, y: 25 },
        { x: 500000, y: 50 },
        { x: 750000, y: 75 },
        { x: 1000000, y: 100 },
        { x: 1000000, y: 0 },
      ],
    ],
    expectedEndDelayPath: [{ x: 1000000, y: 0 }, { x: 1500000, y: 0 }],
  },
  {
    targetClass: "enddelay-negative",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 250000, y: 25 },
        { x: 500000, y: 50 },
        { x: 500000, y: 0 },
      ],
    ],
  },
  {
    targetClass: "enddelay-with-fill-forwards",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 250000, y: 25 },
        { x: 500000, y: 50 },
        { x: 750000, y: 75 },
        { x: 1000000, y: 100 },
        { x: 1000000, y: 0 },
      ],
    ],
    expectedEndDelayPath: [
      { x: 1000000, y: 0 },
      { x: 1000000, y: 100 },
      { x: 1500000, y: 100 },
      { x: 1500000, y: 0 },
    ],
    expectedForwardsPath: [{ x: 1500000, y: 0 }, { x: 1500000, y: 100 }],
  },
  {
    targetClass: "enddelay-with-iterations-infinity",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 250000, y: 25 },
        { x: 500000, y: 50 },
        { x: 750000, y: 75 },
        { x: 1000000, y: 100 },
        { x: 1000000, y: 0 },
      ],
      [{ x: 1000000, y: 0 }, { x: 1250000, y: 25 }, { x: 1500000, y: 50 }],
    ],
    isInfinity: true,
  },
  {
    targetClass: "direction-alternate-with-iterations-infinity",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 250000, y: 25 },
        { x: 500000, y: 50 },
        { x: 750000, y: 75 },
        { x: 1000000, y: 100 },
        { x: 1000000, y: 0 },
      ],
      [
        { x: 1000000, y: 0 },
        { x: 1000000, y: 100 },
        { x: 1250000, y: 75 },
        { x: 1500000, y: 50 },
      ],
    ],
    isInfinity: true,
  },
  {
    targetClass: "direction-alternate-reverse-with-iterations-infinity",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 0, y: 100 },
        { x: 250000, y: 75 },
        { x: 500000, y: 50 },
        { x: 750000, y: 25 },
        { x: 1000000, y: 0 },
      ],
      [{ x: 1000000, y: 0 }, { x: 1250000, y: 25 }, { x: 1500000, y: 50 }],
    ],
    isInfinity: true,
  },
  {
    targetClass: "direction-reverse-with-iterations-infinity",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 0, y: 100 },
        { x: 250000, y: 75 },
        { x: 500000, y: 50 },
        { x: 750000, y: 25 },
        { x: 1000000, y: 0 },
      ],
      [
        { x: 1000000, y: 0 },
        { x: 1000000, y: 100 },
        { x: 1250000, y: 75 },
        { x: 1500000, y: 50 },
      ],
    ],
    isInfinity: true,
  },
  {
    targetClass: "fill-backwards",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 250000, y: 25 },
        { x: 500000, y: 50 },
        { x: 750000, y: 75 },
        { x: 1000000, y: 100 },
        { x: 1000000, y: 0 },
      ],
    ],
  },
  {
    targetClass: "fill-backwards-with-delay-iterationstart",
    expectedDelayPath: [
      { x: 0, y: 0 },
      { x: 0, y: 50 },
      { x: 500000, y: 50 },
      { x: 500000, y: 0 },
    ],
    expectedIterationPathList: [
      [
        { x: 500000, y: 50 },
        { x: 750000, y: 75 },
        { x: 1000000, y: 100 },
        { x: 1000000, y: 0 },
      ],
      [
        { x: 1000000, y: 0 },
        { x: 1250000, y: 25 },
        { x: 1500000, y: 50 },
        { x: 1500000, y: 0 },
      ],
    ],
  },
  {
    targetClass: "fill-both",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 250000, y: 25 },
        { x: 500000, y: 50 },
        { x: 750000, y: 75 },
        { x: 1000000, y: 100 },
        { x: 1000000, y: 0 },
      ],
    ],
    expectedForwardsPath: [
      { x: 1000000, y: 0 },
      { x: 1000000, y: 100 },
      { x: 1500000, y: 100 },
      { x: 1500000, y: 0 },
    ],
  },
  {
    targetClass: "fill-both-width-delay-iterationstart",
    expectedDelayPath: [
      { x: 0, y: 0 },
      { x: 0, y: 50 },
      { x: 500000, y: 50 },
      { x: 500000, y: 0 },
    ],
    expectedIterationPathList: [
      [
        { x: 500000, y: 50 },
        { x: 750000, y: 75 },
        { x: 1000000, y: 100 },
        { x: 1000000, y: 0 },
      ],
      [
        { x: 1000000, y: 0 },
        { x: 1250000, y: 25 },
        { x: 1500000, y: 50 },
        { x: 1500000, y: 0 },
      ],
    ],
    expectedForwardsPath: [{ x: 1500000, y: 0 }, { x: 1500000, y: 50 }],
  },
  {
    targetClass: "fill-forwards",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 250000, y: 25 },
        { x: 500000, y: 50 },
        { x: 750000, y: 75 },
        { x: 1000000, y: 100 },
        { x: 1000000, y: 0 },
      ],
    ],
    expectedForwardsPath: [
      { x: 1000000, y: 0 },
      { x: 1000000, y: 100 },
      { x: 1500000, y: 100 },
      { x: 1500000, y: 0 },
    ],
  },
  {
    targetClass: "iterationstart",
    expectedIterationPathList: [
      [
        { x: 0, y: 50 },
        { x: 250000, y: 75 },
        { x: 500000, y: 100 },
        { x: 500000, y: 0 },
      ],
      [
        { x: 500000, y: 0 },
        { x: 750000, y: 25 },
        { x: 1000000, y: 50 },
        { x: 1000000, y: 0 },
      ],
    ],
  },
  {
    targetClass: "no-compositor",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 250000, y: 25 },
        { x: 500000, y: 50 },
        { x: 750000, y: 75 },
        { x: 1000000, y: 100 },
        { x: 1000000, y: 0 },
      ],
    ],
  },
  {
    targetClass: "keyframes-easing-step",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 499999, y: 0 },
        { x: 500000, y: 50 },
        { x: 999999, y: 50 },
        { x: 1000000, y: 0 },
      ],
    ],
  },
  {
    targetClass: "narrow-keyframes",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 100000, y: 10 },
        { x: 110000, y: 10 },
        { x: 115000, y: 10 },
        { x: 129999, y: 10 },
        { x: 130000, y: 13 },
        { x: 135000, y: 13.5 },
      ],
    ],
  },
  {
    targetClass: "duplicate-offsets",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 250000, y: 25 },
        { x: 500000, y: 50 },
        { x: 999999, y: 50 },
      ],
    ],
  },
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept(TEST_DATA.map(t => `.${t.targetClass}`));
  const { panel } = await openAnimationInspector();

  for (const testData of TEST_DATA) {
    const {
      expectedDelayPath,
      expectedEndDelayPath,
      expectedForwardsPath,
      expectedIterationPathList,
      isInfinity,
      targetClass,
    } = testData;

    const animationItemEl = findAnimationItemElementsByTargetSelector(
      panel,
      `.${targetClass}`
    );

    info(`Checking computed timing path existance for ${targetClass}`);
    const computedTimingPathEl = animationItemEl.querySelector(
      ".animation-computed-timing-path"
    );
    ok(
      computedTimingPathEl,
      "The computed timing path element should be in each animation item element"
    );

    info(`Checking delay path for ${targetClass}`);
    const delayPathEl = computedTimingPathEl.querySelector(
      ".animation-delay-path"
    );

    if (expectedDelayPath) {
      ok(delayPathEl, "delay path should be existance");
      assertPathSegments(delayPathEl, true, expectedDelayPath);
    } else {
      ok(!delayPathEl, "delay path should not be existance");
    }

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

      info(`Checking infinity ${targetClass}`);
      if (isInfinity && j >= 1) {
        ok(
          iterationPathEl.classList.contains("infinity"),
          "iteration path should have 'infinity' class"
        );
      } else {
        ok(
          !iterationPathEl.classList.contains("infinity"),
          "iteration path should not have 'infinity' class"
        );
      }
    }

    info(`Checking endDelay path for ${targetClass}`);
    const endDelayPathEl = computedTimingPathEl.querySelector(
      ".animation-enddelay-path"
    );

    if (expectedEndDelayPath) {
      ok(endDelayPathEl, "endDelay path should be existance");
      assertPathSegments(endDelayPathEl, true, expectedEndDelayPath);
    } else {
      ok(!endDelayPathEl, "endDelay path should not be existance");
    }

    info(`Checking forwards fill path for ${targetClass}`);
    const forwardsPathEl = computedTimingPathEl.querySelector(
      ".animation-fill-forwards-path"
    );

    if (expectedForwardsPath) {
      ok(forwardsPathEl, "forwards path should be existance");
      assertPathSegments(forwardsPathEl, true, expectedForwardsPath);
    } else {
      ok(!forwardsPathEl, "forwards path should not be existance");
    }
  }
});
