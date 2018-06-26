/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following summary graph with the animation which has infinity duration.
// * Tooltips
// * Graph path
// * Delay sign

const TEST_DATA = [
  {
    targetClass: "infinity",
    expectedIterationPath: [
      { x: 0, y: 0 },
      { x: 200000, y: 0 },
    ],
    expectedTooltip: {
      duration: "\u221E",
    },
  },
  {
    targetClass: "infinity-delay-iteration-start",
    expectedDelayPath: [
      { x: 0, y: 0 },
      { x: 100000, y: 0 },
    ],
    expectedDelaySign: {
      marginInlineStart: "0%",
      width: "50%",
    },
    expectedIterationPath: [
      { x: 100000, y: 50 },
      { x: 200000, y: 50 },
    ],
    expectedTooltip: {
      delay: "100s",
      duration: "\u221E",
      iterationStart: "0.5 (\u221E)",
    },
  },
  {
    targetClass: "limited",
    expectedIterationPath: [
      { x: 0, y: 0 },
      { x: 100000, y: 100 },
    ],
    expectedTooltip: {
      duration: "100s",
    },
  },
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_infinity_duration.html");
  const { panel } = await openAnimationInspector();

  for (const testData of TEST_DATA) {
    const {
      targetClass,
      expectedDelayPath,
      expectedDelaySign,
      expectedIterationPath,
      expectedTooltip,
    } = testData;

    const animationItemEl =
      findAnimationItemElementsByTargetSelector(panel, `.${ targetClass }`);
    const summaryGraphEl = animationItemEl.querySelector(".animation-summary-graph");

    info(`Check tooltip for the animation of .${ targetClass }`);
    assertTooltip(summaryGraphEl, expectedTooltip);

    if (expectedDelayPath) {
      info(`Check delay path for the animation of .${ targetClass }`);
      assertDelayPath(summaryGraphEl, expectedDelayPath);
    }

    if (expectedDelaySign) {
      info(`Check delay sign for the animation of .${ targetClass }`);
      assertDelaySign(summaryGraphEl, expectedDelaySign);
    }

    info(`Check iteration path for the animation of .${ targetClass }`);
    assertIterationPath(summaryGraphEl, expectedIterationPath);
  }
});

function assertDelayPath(summaryGraphEl, expectedPath) {
  assertPath(summaryGraphEl,
             ".animation-computed-timing-path .animation-delay-path",
             expectedPath);
}

function assertDelaySign(summaryGraphEl, expectedSign) {
  const signEl = summaryGraphEl.querySelector(".animation-delay-sign");

  is(signEl.style.marginInlineStart, expectedSign.marginInlineStart,
     `marginInlineStart position should be ${ expectedSign.marginInlineStart }`);
  is(signEl.style.width, expectedSign.width,
     `Width should be ${ expectedSign.width }`);
}

function assertIterationPath(summaryGraphEl, expectedPath) {
  assertPath(summaryGraphEl,
             ".animation-computed-timing-path .animation-iteration-path",
             expectedPath);
}

function assertPath(summaryGraphEl, pathSelector, expectedPath) {
  const pathEl = summaryGraphEl.querySelector(pathSelector);
  assertPathSegments(pathEl, true, expectedPath);
}

function assertTooltip(summaryGraphEl, expectedTooltip) {
  const tooltip = summaryGraphEl.getAttribute("title");
  const {
    delay,
    duration,
    iterationStart,
  } = expectedTooltip;

  if (delay) {
    const expected = `Delay: ${ delay }`;
    ok(tooltip.includes(expected), `Tooltip should include '${ expected }'`);
  }

  if (duration) {
    const expected = `Duration: ${ duration }`;
    ok(tooltip.includes(expected), `Tooltip should include '${ expected }'`);
  }

  if (iterationStart) {
    const expected = `Iteration start: ${ iterationStart }`;
    ok(tooltip.includes(expected), `Tooltip should include '${ expected }'`);
  }
}
