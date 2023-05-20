/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following ComputedTimingPath component works.
// * element existance
// * iterations: path, count
// * delay: path
// * fill: path
// * endDelay: path

/* import-globals-from summary-graph_computed-timing-path_head.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "summary-graph_computed-timing-path_head.js",
  this
);

const TEST_DATA = [
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
    expectedForwardsPath: [
      { x: 1500000, y: 0 },
      { x: 1500000, y: 50 },
    ],
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

add_task(async function () {
  await testComputedTimingPath(TEST_DATA);
});
