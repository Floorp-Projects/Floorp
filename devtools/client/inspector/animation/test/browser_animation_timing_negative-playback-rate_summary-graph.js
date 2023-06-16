/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following summary graph with the animation which is negative playback rate.
// * Tooltips
// * Graph path
// * Delay sign
// * End delay sign

const TEST_DATA = [
  {
    targetSelector: ".normal",
    expectedPathList: [
      {
        selector: ".animation-iteration-path",
        path: [
          { x: 0, y: 100 },
          { x: 50000, y: 50 },
          { x: 100000, y: 0 },
        ],
      },
    ],
    expectedTooltip: "Playback rate: -1",
    expectedViewboxWidth: 200000,
  },
  {
    targetSelector: ".normal-playbackrate-2",
    expectedPathList: [
      {
        selector: ".animation-iteration-path",
        path: [
          { x: 0, y: 100 },
          { x: 50000, y: 50 },
          { x: 100000, y: 0 },
        ],
      },
    ],
    expectedTooltip: "Playback rate: -2",
    expectedViewboxWidth: 400000,
  },
  {
    targetSelector: ".positive-delay",
    expectedSignList: [
      {
        selector: ".animation-end-delay-sign",
        sign: {
          marginInlineStart: "75%",
          width: "25%",
        },
      },
    ],
    expectedPathList: [
      {
        selector: ".animation-iteration-path",
        path: [
          { x: 0, y: 100 },
          { x: 50000, y: 50 },
          { x: 100000, y: 0 },
        ],
      },
    ],
    expectedTooltip: "Playback rate: -1",
  },
  {
    targetSelector: ".negative-delay",
    expectedSignList: [
      {
        selector: ".animation-end-delay-sign",
        sign: {
          marginInlineStart: "50%",
          width: "25%",
        },
      },
    ],
    expectedPathList: [
      {
        selector: ".animation-iteration-path",
        path: [
          { x: 0, y: 100 },
          { x: 50000, y: 50 },
          { x: 50000, y: 0 },
        ],
      },
      {
        selector: ".animation-negative-delay-path path",
        path: [
          { x: 50000, y: 50 },
          { x: 100000, y: 0 },
        ],
      },
    ],
    expectedTooltip: "Playback rate: -1",
  },
  {
    targetSelector: ".positive-end-delay",
    expectedSignList: [
      {
        selector: ".animation-delay-sign",
        sign: {
          isFilled: true,
          marginInlineStart: "25%",
          width: "25%",
        },
      },
    ],
    expectedPathList: [
      {
        selector: ".animation-iteration-path",
        path: [
          { x: 50000, y: 100 },
          { x: 100000, y: 50 },
          { x: 150000, y: 0 },
        ],
      },
    ],
    expectedTooltip: "Playback rate: -1",
  },
  {
    targetSelector: ".negative-end-delay",
    expectedSignList: [
      {
        selector: ".animation-delay-sign",
        sign: {
          isFilled: true,
          marginInlineStart: "0%",
          width: "25%",
        },
      },
    ],
    expectedPathList: [
      {
        selector: ".animation-iteration-path",
        path: [
          { x: 0, y: 50 },
          { x: 50000, y: 0 },
        ],
      },
      {
        selector: ".animation-negative-end-delay-path path",
        path: [
          { x: -50000, y: 100 },
          { x: 0, y: 0 },
        ],
      },
    ],
    expectedTooltip: "Playback rate: -1",
  },
];

add_task(async function () {
  await addTab(URL_ROOT + "doc_negative_playback_rate.html");
  const { panel } = await openAnimationInspector();

  for (const testData of TEST_DATA) {
    const {
      targetSelector,
      expectedPathList,
      expectedSignList,
      expectedTooltip,
      expectedViewboxWidth,
    } = testData;

    const animationItemEl = await findAnimationItemByTargetSelector(
      panel,
      targetSelector
    );
    const summaryGraphEl = animationItemEl.querySelector(
      ".animation-summary-graph"
    );

    info(`Check tooltip for the animation of ${targetSelector}`);
    assertTooltip(summaryGraphEl, expectedTooltip);

    if (expectedPathList) {
      for (const { selector, path } of expectedPathList) {
        info(`Check path for ${selector}`);
        assertPath(summaryGraphEl, selector, path);
      }
    }

    if (expectedSignList) {
      for (const { selector, sign } of expectedSignList) {
        info(`Check sign for ${selector}`);
        assertSign(summaryGraphEl, selector, sign);
      }
    }

    if (expectedViewboxWidth) {
      info("Check width of viewbox of SVG");
      const svgEl = summaryGraphEl.querySelector(
        ".animation-summary-graph-path"
      );
      is(
        svgEl.viewBox.baseVal.width,
        expectedViewboxWidth,
        `width of viewbox should be ${expectedViewboxWidth}`
      );
    }
  }
});

function assertPath(summaryGraphEl, pathSelector, expectedPath) {
  const pathEl = summaryGraphEl.querySelector(pathSelector);
  assertPathSegments(pathEl, true, expectedPath);
}

function assertSign(summaryGraphEl, selector, expectedSign) {
  const signEl = summaryGraphEl.querySelector(selector);

  is(
    signEl.style.marginInlineStart,
    expectedSign.marginInlineStart,
    `marginInlineStart position should be ${expectedSign.marginInlineStart}`
  );
  is(
    signEl.style.width,
    expectedSign.width,
    `Width should be ${expectedSign.width}`
  );
  is(
    signEl.classList.contains("fill"),
    expectedSign.isFilled || false,
    "signEl should be correct"
  );
}

function assertTooltip(summaryGraphEl, expectedTooltip) {
  const tooltip = summaryGraphEl.getAttribute("title");
  ok(
    tooltip.includes(expectedTooltip),
    `Tooltip should include '${expectedTooltip}'`
  );
}
