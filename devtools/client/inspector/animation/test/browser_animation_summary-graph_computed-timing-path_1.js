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
    expectedDelayPath: [
      { x: 0, y: 0 },
      { x: 500000, y: 0 },
    ],
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
    expectedEndDelayPath: [
      { x: 1000000, y: 0 },
      { x: 1500000, y: 0 },
    ],
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
    expectedForwardsPath: [
      { x: 1500000, y: 0 },
      { x: 1500000, y: 100 },
    ],
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
      [
        { x: 1000000, y: 0 },
        { x: 1250000, y: 25 },
        { x: 1500000, y: 50 },
      ],
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
      [
        { x: 1000000, y: 0 },
        { x: 1250000, y: 25 },
        { x: 1500000, y: 50 },
      ],
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
];

add_task(async function () {
  await testComputedTimingPath(TEST_DATA);
});
