/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

// Test for following keyframe marker.
// * element existence
// * title
// * and marginInlineStart style

const KEYFRAMES_TEST_DATA = [
  {
    targetClass: "multi-types",
    properties: [
      {
        name: "background-color",
        expectedValues: [
          {
            title: "rgb(255, 0, 0)",
            marginInlineStart: "0%",
          },
          {
            title: "rgb(0, 255, 0)",
            marginInlineStart: "100%",
          }
        ],
      },
      {
        name: "background-repeat",
        expectedValues: [
          {
            title: "space round",
            marginInlineStart: "0%",
          },
          {
            title: "round space",
            marginInlineStart: "100%",
          }
        ],
      },
      {
        name: "font-size",
        expectedValues: [
          {
            title: "10px",
            marginInlineStart: "0%",
          },
          {
            title: "20px",
            marginInlineStart: "100%",
          }
        ],
      },
      {
        name: "margin-left",
        expectedValues: [
          {
            title: "0px",
            marginInlineStart: "0%",
          },
          {
            title: "100px",
            marginInlineStart: "100%",
          }
        ],
      },
      {
        name: "opacity",
        expectedValues: [
          {
            title: "0",
            marginInlineStart: "0%",
          },
          {
            title: "1",
            marginInlineStart: "100%",
          }
        ],
      },
      {
        name: "text-align",
        expectedValues: [
          {
            title: "right",
            marginInlineStart: "0%",
          },
          {
            title: "center",
            marginInlineStart: "100%",
          }
        ],
      },
      {
        name: "transform",
        expectedValues: [
          {
            title: "translate(0px)",
            marginInlineStart: "0%",
          },
          {
            title: "translate(100px)",
            marginInlineStart: "100%",
          }
        ],
      },
    ],
  },
  {
    targetClass: "narrow-offsets",
    properties: [
      {
        name: "opacity",
        expectedValues: [
          {
            title: "0",
            marginInlineStart: "0%",
          },
          {
            title: "1",
            marginInlineStart: "10%",
          },
          {
            title: "0",
            marginInlineStart: "13%",
          },
          {
            title: "1",
            marginInlineStart: "100%",
          },
        ],
      },
    ],
  }
];

/**
 * Do test for keyframes-graph_keyframe-marker-ltf/rtl.
 *
 * @param {Array} testData
 */
// eslint-disable-next-line no-unused-vars
async function testKeyframesGraphKeyframesMarker() {
  await addTab(URL_ROOT + "doc_multi_keyframes.html");
  await removeAnimatedElementsExcept(KEYFRAMES_TEST_DATA.map(t => `.${ t.targetClass }`));
  const { animationInspector, panel } = await openAnimationInspector();

  for (const { properties, targetClass } of KEYFRAMES_TEST_DATA) {
    info(`Checking keyframe marker for ${ targetClass }`);
    await clickOnAnimationByTargetSelector(animationInspector,
                                           panel, `.${ targetClass }`);

    for (const { name, expectedValues } of properties) {
      const testTarget = `${ name } in ${ targetClass }`;
      info(`Checking keyframe marker for ${ testTarget }`);
      info(`Checking keyframe marker existence for ${ testTarget }`);
      const markerEls = panel.querySelectorAll(`.${ name } .keyframe-marker-item`);
      is(markerEls.length, expectedValues.length,
        `Count of keyframe marker elements of ${ testTarget } ` +
        `should be ${ expectedValues.length }`);

      for (let i = 0; i < expectedValues.length; i++) {
        const hintTarget = `.keyframe-marker-item[${ i }] of ${ testTarget }`;

        info(`Checking ${ hintTarget }`);
        const markerEl = markerEls[i];
        const expectedValue = expectedValues[i];

        info(`Checking title in ${ hintTarget }`);
        is(markerEl.getAttribute("title"), expectedValue.title,
         `title in ${ hintTarget } should be ${ expectedValue.title }`);

        info(`Checking marginInlineStart style in ${ hintTarget }`);
        is(markerEl.style.marginInlineStart, expectedValue.marginInlineStart,
          `marginInlineStart in ${ hintTarget } should be ` +
          `${ expectedValue.marginInlineStart }`);
      }
    }
  }
}

