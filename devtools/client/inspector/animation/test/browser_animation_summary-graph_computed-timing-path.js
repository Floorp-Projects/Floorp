/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following ComputedTimingPath component works.
// * element existance
// * iterations: path, count
// * delay: path
// * fill: path
// * endDelay: path

const TEST_CASES = [
  {
    targetClassName: "cssanimation-normal",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 40.851 },
        { x: 50000, y: 80.24},
        { x: 75000, y: 96.05 },
        { x: 100000, y: 100 },
        { x: 100000, y: 0 },
      ]
    ],
  },
  {
    targetClassName: "cssanimation-linear",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 25 },
        { x: 50000, y: 50 },
        { x: 75000, y: 75 },
        { x: 100000, y: 100 },
        { x: 100000, y: 0 },
      ]
    ],
  },
  {
    targetClassName: "delay-positive",
    expectedDelayPath: [
      { x: 0, y: 0 },
      { x: 50000, y: 0 },
    ],
    expectedIterationPathList: [
      [
        { x: 50000, y: 0 },
        { x: 75000, y: 25 },
        { x: 100000, y: 50 },
        { x: 125000, y: 75 },
        { x: 150000, y: 100 },
        { x: 150000, y: 0 },
      ]
    ],
  },
  {
    targetClassName: "delay-negative",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 0, y: 50 },
        { x: 25000, y: 75 },
        { x: 50000, y: 100 },
        { x: 50000, y: 0 },
      ]
    ],
  },
  {
    targetClassName: "easing-step",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 49999, y: 0 },
        { x: 50000, y: 50 },
        { x: 99999, y: 50 },
        { x: 100000, y: 0 },
      ]
    ],
  },
  {
    targetClassName: "enddelay-positive",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 25 },
        { x: 50000, y: 50 },
        { x: 75000, y: 75 },
        { x: 100000, y: 100 },
        { x: 100000, y: 0 },
      ]
    ],
    expectedEndDelayPath: [
      { x: 100000, y: 0 },
      { x: 150000, y: 0 },
    ],
  },
  {
    targetClassName: "enddelay-negative",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 25 },
        { x: 50000, y: 50 },
        { x: 50000, y: 0 },
      ]
    ],
  },
  {
    targetClassName: "enddelay-with-fill-forwards",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 25 },
        { x: 50000, y: 50 },
        { x: 75000, y: 75 },
        { x: 100000, y: 100 },
        { x: 100000, y: 0 },
      ]
    ],
    expectedEndDelayPath: [
      { x: 100000, y: 0 },
      { x: 100000, y: 100 },
      { x: 150000, y: 100 },
      { x: 150000, y: 0 },
    ],
    expectedForwardsPath: [
      { x: 150000, y: 0 },
      { x: 150000, y: 100 },
      { x: 200000, y: 100 },
      { x: 200000, y: 0 },
    ],
  },
  {
    targetClassName: "enddelay-with-iterations-infinity",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 25 },
        { x: 50000, y: 50 },
        { x: 75000, y: 75 },
        { x: 100000, y: 100 },
        { x: 100000, y: 0 },
      ],
      [
        { x: 100000, y: 0 },
        { x: 125000, y: 25 },
        { x: 150000, y: 50 },
        { x: 175000, y: 75 },
        { x: 200000, y: 100 },
        { x: 200000, y: 0 },
      ]
    ],
    isInfinity: true,
  },
  {
    targetClassName: "direction-alternate-with-iterations-infinity",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 25 },
        { x: 50000, y: 50 },
        { x: 75000, y: 75 },
        { x: 100000, y: 100 },
        { x: 100000, y: 0 },
      ],
      [
        { x: 100000, y: 0 },
        { x: 100000, y: 100 },
        { x: 125000, y: 75 },
        { x: 150000, y: 50 },
        { x: 175000, y: 25 },
        { x: 200000, y: 0 },
      ]
    ],
    isInfinity: true,
  },
  {
    targetClassName: "direction-alternate-reverse-with-iterations-infinity",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 0, y: 100 },
        { x: 25000, y: 75 },
        { x: 50000, y: 50 },
        { x: 75000, y: 25 },
        { x: 100000, y: 0 },
      ],
      [
        { x: 100000, y: 0 },
        { x: 125000, y: 25 },
        { x: 150000, y: 50 },
        { x: 175000, y: 75 },
        { x: 200000, y: 100 },
        { x: 200000, y: 0 },
      ]
    ],
    isInfinity: true,
  },
  {
    targetClassName: "direction-reverse-with-iterations-infinity",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 0, y: 100 },
        { x: 25000, y: 75 },
        { x: 50000, y: 50 },
        { x: 75000, y: 25 },
        { x: 100000, y: 0 },
      ],
      [
        { x: 100000, y: 0 },
        { x: 100000, y: 100 },
        { x: 125000, y: 75 },
        { x: 150000, y: 50 },
        { x: 175000, y: 25 },
        { x: 200000, y: 0 },
      ]
    ],
    isInfinity: true,
  },
  {
    targetClassName: "fill-backwards",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 25 },
        { x: 50000, y: 50 },
        { x: 75000, y: 75 },
        { x: 100000, y: 100 },
        { x: 100000, y: 0 },
      ]
    ],
  },
  {
    targetClassName: "fill-backwards-with-delay-iterationstart",
    expectedDelayPath: [
      { x: 0, y: 0 },
      { x: 0, y: 50 },
      { x: 50000, y: 50 },
      { x: 50000, y: 0 },
    ],
    expectedIterationPathList: [
      [
        { x: 50000, y: 50 },
        { x: 75000, y: 75 },
        { x: 100000, y: 100 },
        { x: 100000, y: 0 },
      ],
      [
        { x: 100000, y: 0 },
        { x: 125000, y: 25 },
        { x: 150000, y: 50 },
        { x: 150000, y: 0 },
      ]
    ],
  },
  {
    targetClassName: "fill-both",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 25 },
        { x: 50000, y: 50 },
        { x: 75000, y: 75 },
        { x: 100000, y: 100 },
        { x: 100000, y: 0 },
      ]
    ],
    expectedForwardsPath: [
      { x: 100000, y: 0 },
      { x: 100000, y: 100 },
      { x: 200000, y: 100 },
      { x: 200000, y: 0 },
    ],
  },
  {
    targetClassName: "fill-both-width-delay-iterationstart",
    expectedDelayPath: [
      { x: 0, y: 0 },
      { x: 0, y: 50 },
      { x: 50000, y: 50 },
      { x: 50000, y: 0 },
    ],
    expectedIterationPathList: [
      [
        { x: 50000, y: 50 },
        { x: 75000, y: 75 },
        { x: 100000, y: 100 },
        { x: 100000, y: 0 },
      ],
      [
        { x: 100000, y: 0 },
        { x: 125000, y: 25 },
        { x: 150000, y: 50 },
        { x: 150000, y: 0 },
      ]
    ],
    expectedForwardsPath: [
      { x: 150000, y: 0 },
      { x: 150000, y: 50 },
    ],
  },
  {
    targetClassName: "fill-forwards",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 25 },
        { x: 50000, y: 50 },
        { x: 75000, y: 75 },
        { x: 100000, y: 100 },
        { x: 100000, y: 0 },
      ]
    ],
    expectedForwardsPath: [
      { x: 100000, y: 0 },
      { x: 100000, y: 100 },
      { x: 200000, y: 100 },
      { x: 200000, y: 0 },
    ],
  },
  {
    targetClassName: "iterationstart",
    expectedIterationPathList: [
      [
        { x: 0, y: 50 },
        { x: 25000, y: 75 },
        { x: 50000, y: 100 },
        { x: 50000, y: 0 },
      ],
      [
        { x: 50000, y: 0 },
        { x: 75000, y: 25 },
        { x: 100000, y: 50 },
        { x: 100000, y: 0 },
      ]
    ],
  },
  {
    targetClassName: "no-compositor",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 25 },
        { x: 50000, y: 50 },
        { x: 75000, y: 75 },
        { x: 100000, y: 100 },
        { x: 100000, y: 0 },
      ]
    ],
  },
  {
    targetClassName: "keyframes-easing-step",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 49999, y: 0 },
        { x: 50000, y: 50 },
        { x: 99999, y: 50 },
        { x: 100000, y: 0 },
      ]
    ],
  },
  {
    targetClassName: "narrow-keyframes",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 10000, y: 10 },
        { x: 11000, y: 10 },
        { x: 11500, y: 10 },
        { x: 12999, y: 10 },
        { x: 13000, y: 13 },
        { x: 13500, y: 13.5 },
      ]
    ],
  },
  {
    targetClassName: "duplicate-offsets",
    expectedIterationPathList: [
      [
        { x: 0, y: 0 },
        { x: 25000, y: 25 },
        { x: 50000, y: 50 },
        { x: 99999, y: 50 },
      ]
    ],
  },
];

add_task(async function () {
  await addTab(URL_ROOT + "doc_multi_timings.html");

  const { panel } = await openAnimationInspector();

  for (const testCase of TEST_CASES) {
    const {
      expectedDelayPath,
      expectedEndDelayPath,
      expectedForwardsPath,
      expectedIterationPathList,
      isInfinity,
      targetClassName,
    } = testCase;

    const animationItemEl =
      findAnimationItemElementsByTargetClassName(panel, targetClassName);

    info(`Checking computed timing path existance for ${ targetClassName }`);
    const computedTimingPathEl =
      animationItemEl.querySelector(".animation-computed-timing-path");
    ok(computedTimingPathEl,
       "The computed timing path element should be in each animation item element");

    info(`Checking delay path for ${ targetClassName }`);
    const delayPathEl = computedTimingPathEl.querySelector(".animation-delay-path");

    if (expectedDelayPath) {
      ok(delayPathEl, "delay path should be existance");
      assertPathSegments(delayPathEl, true, expectedDelayPath);
    } else {
      ok(!delayPathEl, "delay path should not be existance");
    }

    info(`Checking iteration path list for ${ targetClassName }`);
    const iterationPathEls =
      computedTimingPathEl.querySelectorAll(".animation-iteration-path");
    is(iterationPathEls.length, expectedIterationPathList.length,
       `Number of iteration path should be ${ expectedIterationPathList.length }`);

    for (const [j, iterationPathEl] of iterationPathEls.entries()) {
      assertPathSegments(iterationPathEl, true, expectedIterationPathList[j]);

      info(`Checking infinity ${ targetClassName }`);
      if (isInfinity && j >= 1) {
        ok(iterationPathEl.classList.contains("infinity"),
           "iteration path should have 'infinity' class");
      } else {
        ok(!iterationPathEl.classList.contains("infinity"),
           "iteration path should not have 'infinity' class");
      }
    }

    info(`Checking endDelay path for ${ targetClassName }`);
    const endDelayPathEl = computedTimingPathEl.querySelector(".animation-enddelay-path");

    if (expectedEndDelayPath) {
      ok(endDelayPathEl, "endDelay path should be existance");
      assertPathSegments(endDelayPathEl, true, expectedEndDelayPath);
    } else {
      ok(!endDelayPathEl, "endDelay path should not be existance");
    }

    info(`Checking forwards fill path for ${ targetClassName }`);
    const forwardsPathEl =
      computedTimingPathEl.querySelector(".animation-fill-forwards-path");

    if (expectedForwardsPath) {
      ok(forwardsPathEl, "forwards path should be existance");
      assertPathSegments(forwardsPathEl, true, expectedForwardsPath);
    } else {
      ok(!forwardsPathEl, "forwards path should not be existance");
    }
  }
});
