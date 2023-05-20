/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for ComputedValuePath of animations that consist by one animated property
// on complexed keyframes.

requestLongerTimeout(2);

const TEST_DATA = [
  {
    targetClass: "steps-effect",
    properties: [
      {
        name: "opacity",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 250, y: 25 },
          { x: 500, y: 50 },
          { x: 750, y: 75 },
          { x: 1000, y: 100 },
        ],
      },
    ],
  },
  {
    targetClass: "steps-jump-none-keyframe",
    properties: [
      {
        name: "opacity",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 199, y: 0 },
          { x: 200, y: 25 },
          { x: 399, y: 25 },
          { x: 400, y: 50 },
          { x: 599, y: 50 },
          { x: 600, y: 75 },
          { x: 799, y: 75 },
          { x: 800, y: 100 },
          { x: 1000, y: 100 },
        ],
      },
    ],
  },
  {
    targetClass: "narrow-offsets",
    properties: [
      {
        name: "opacity",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 100, y: 100 },
          { x: 110, y: 100 },
          { x: 114.9, y: 100 },
          { x: 115, y: 50 },
          { x: 129.9, y: 50 },
          { x: 130, y: 0 },
          { x: 1000, y: 100 },
        ],
      },
    ],
  },
  {
    targetClass: "duplicate-offsets",
    properties: [
      {
        name: "opacity",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 100 },
          { x: 250, y: 100 },
          { x: 499, y: 100 },
          { x: 500, y: 100 },
          { x: 500, y: 0 },
          { x: 750, y: 50 },
          { x: 1000, y: 100 },
        ],
      },
    ],
  },
];

add_task(async function () {
  await testKeyframesGraphComputedValuePath(TEST_DATA);
});
