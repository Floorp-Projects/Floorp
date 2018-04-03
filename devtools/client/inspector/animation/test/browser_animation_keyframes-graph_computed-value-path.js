/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following ComputedValuePath component:
// * element existence
// * path segments
// * fill color by animation type
// * stop color if the animation type is color

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
        ]
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
        ]
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
  {
    targetClass: "middle-keyframe",
    properties: [
      {
        name: "background-color",
        computedValuePathClass: "color-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 0, y: 100 },
          { x: 500, y: 100 },
          { x: 1000, y: 100 },
        ],
        expectedStopColors: [
          { offset: 0, color: "rgb(255, 0, 0)" },
          { offset: 0.5, color: "rgb(0, 0, 255)" },
          { offset: 1, color: "rgb(0, 255, 0)" },
        ]
      },
      {
        name: "background-repeat",
        computedValuePathClass: "discrete-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 249.999, y: 0 },
          { x: 250, y: 100 },
          { x: 749.999, y: 100 },
          { x: 750, y: 0 },
          { x: 1000, y: 0 },
        ],
      },
      {
        name: "font-size",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 250, y: 50 },
          { x: 500, y: 100 },
          { x: 750, y: 50 },
          { x: 1000, y: 0 },
        ],
      },
      {
        name: "margin-left",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 250, y: 50 },
          { x: 500, y: 100 },
          { x: 750, y: 50 },
          { x: 1000, y: 0 },
        ],
      },
      {
        name: "opacity",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 250, y: 50 },
          { x: 500, y: 100 },
          { x: 750, y: 50 },
          { x: 1000, y: 0 },
        ],
      },
      {
        name: "text-align",
        computedValuePathClass: "discrete-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 249.999, y: 0 },
          { x: 250, y: 100 },
          { x: 749.999, y: 100 },
          { x: 750, y: 0 },
          { x: 1000, y: 0 },
        ],
      },
      {
        name: "transform",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 250, y: 50 },
          { x: 500, y: 100 },
          { x: 750, y: 50 },
          { x: 1000, y: 0 },
        ],
      },
    ],
  },
  {
    targetClass: "steps-keyframe",
    properties: [
      {
        name: "background-color",
        computedValuePathClass: "color-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 0, y: 100 },
          { x: 500, y: 100 },
          { x: 1000, y: 100 },
        ],
        expectedStopColors: [
          { offset: 0, color: "rgb(255, 0, 0)" },
          { offset: 0.499, color: "rgb(255, 0, 0)" },
          { offset: 0.5, color: "rgb(128, 128, 0)" },
          { offset: 0.999, color: "rgb(128, 128, 0)" },
          { offset: 1, color: "rgb(0, 255, 0)" },
        ]
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
          { x: 500, y: 0 },
          { x: 500, y: 50 },
          { x: 1000, y: 50 },
          { x: 1000, y: 100 },
        ],
      },
      {
        name: "margin-left",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 499.999, y: 0 },
          { x: 500, y: 50 },
          { x: 999.999, y: 50 },
          { x: 1000, y: 100 },
        ],
      },
      {
        name: "opacity",
        computedValuePathClass: "distance-path",
        expectedPathSegments: [
          { x: 0, y: 0 },
          { x: 499.999, y: 0 },
          { x: 500, y: 50 },
          { x: 999.999, y: 50 },
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
          { x: 500, y: 0 },
          { x: 500, y: 50 },
          { x: 1000, y: 50 },
          { x: 1000, y: 100 },
        ],
      },
    ],
  },
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
    targetClass: "frames-keyframe",
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

add_task(async function() {
  await addTab(URL_ROOT + "doc_multi_keyframes.html");
  const { animationInspector, panel } = await openAnimationInspector();

  for (const { properties, targetClass } of TEST_DATA) {
    info(`Checking keyframes graph for ${ targetClass }`);
    await clickOnAnimationByTargetSelector(animationInspector,
                                           panel, `.${ targetClass }`);

    for (const property of properties) {
      const {
        name,
        computedValuePathClass,
        expectedPathSegments,
        expectedStopColors,
      } = property;

      const testTarget = `${ name } in ${ targetClass }`;
      info(`Checking keyframes graph for ${ testTarget }`);
      info(`Checking keyframes graph path existence for ${ testTarget }`);
      const keyframesGraphPathEl = panel.querySelector(`.${ name }`);
      ok(keyframesGraphPathEl,
        `The keyframes graph path element of ${ testTarget } should be existence`);

      info(`Checking computed value path existence for ${ testTarget }`);
      const computedValuePathEl =
        keyframesGraphPathEl.querySelector(`.${ computedValuePathClass }`);
      ok(computedValuePathEl,
        `The computed value path element of ${ testTarget } should be existence`);

      info(`Checking path segments for ${ testTarget }`);
      const pathEl = computedValuePathEl.querySelector("path");
      ok(pathEl, `The <path> element of ${ testTarget } should be existence`);
      assertPathSegments(pathEl, true, expectedPathSegments);

      if (!expectedStopColors) {
        continue;
      }

      info(`Checking linearGradient for ${ testTarget }`);
      const linearGradientEl = computedValuePathEl.querySelector("linearGradient");
      ok(linearGradientEl,
        `The <linearGradientEl> element of ${ testTarget } should be existence`);

      for (const expectedStopColor of expectedStopColors) {
        const { offset, color } = expectedStopColor;
        assertLinearGradient(linearGradientEl, offset, color);
      }
    }
  }
});
