/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for ComputedValuePath of animations that consist by multi types of animated
// properties.

requestLongerTimeout(2);

const TEST_DATA = [
  {
    targetClass: "multi-types",
    properties: [
      {
        name: "background-color",
        computedValuePathClass: "color-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 0, y: 100 },
          { x: 1000, y: 100 },
        ],
        expectedStopColors: [
          { offset: 0, color: "rgb(255, 0, 0)" },
          { offset: 1, color: "rgb(0, 255, 0)" },
        ],
      },
      {
        name: "background-repeat",
        computedValuePathClass: "discrete-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 499.999, y: 0 },
          { x: 500, y: 100 },
          { x: 1000, y: 100 },
        ],
      },
      {
        name: "font-size",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 500, y: 50 },
          { x: 1000, y: 100 },
        ],
      },
      {
        name: "margin-left",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 500, y: 50 },
          { x: 1000, y: 100 },
        ],
      },
      {
        name: "opacity",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 500, y: 50 },
          { x: 1000, y: 100 },
        ],
      },
      {
        name: "text-align",
        computedValuePathClass: "discrete-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 499.999, y: 0 },
          { x: 500, y: 100 },
          { x: 1000, y: 100 },
        ],
      },
      {
        name: "transform",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 500, y: 50 },
          { x: 1000, y: 100 },
        ],
      },
    ],
  },
  {
    targetClass: "multi-types-reverse",
    properties: [
      {
        name: "background-color",
        computedValuePathClass: "color-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 0, y: 100 },
          { x: 1000, y: 100 },
        ],
        expectedStopColors: [
          { offset: 0, color: "rgb(0, 255, 0)" },
          { offset: 1, color: "rgb(255, 0, 0)" },
        ],
      },
      {
        name: "background-repeat",
        computedValuePathClass: "discrete-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 499.999, y: 0 },
          { x: 500, y: 100 },
          { x: 1000, y: 100 },
        ],
      },
      {
        name: "font-size",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 100 },
          { x: 500, y: 50 },
          { x: 1000, y: 0 },
        ],
      },
      {
        name: "margin-left",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 100 },
          { x: 500, y: 50 },
          { x: 1000, y: 0 },
        ],
      },
      {
        name: "opacity",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 100 },
          { x: 500, y: 50 },
          { x: 1000, y: 0 },
        ],
      },
      {
        name: "text-align",
        computedValuePathClass: "discrete-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 499.999, y: 0 },
          { x: 500, y: 100 },
          { x: 1000, y: 100 },
        ],
      },
      {
        name: "transform",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 100 },
          { x: 500, y: 50 },
          { x: 1000, y: 0 },
        ],
      },
    ],
  },
];

add_task(async function () {
  await testKeyframesGraphComputedValuePath(TEST_DATA);
});
