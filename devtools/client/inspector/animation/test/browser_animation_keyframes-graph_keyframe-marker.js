/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following keyframe marker.
// * element existence
// * title
// * and left style

const TEST_DATA = [
  {
    targetClass: "multi-types",
    properties: [
      {
        name: "background-color",
        expectedValues: [
          {
            title: "rgb(255, 0, 0)",
            left: "0%",
          },
          {
            title: "rgb(0, 255, 0)",
            left: "100%",
          }
        ],
      },
      {
        name: "background-repeat",
        expectedValues: [
          {
            title: "space round",
            left: "0%",
          },
          {
            title: "round space",
            left: "100%",
          }
        ],
      },
      {
        name: "font-size",
        expectedValues: [
          {
            title: "10px",
            left: "0%",
          },
          {
            title: "20px",
            left: "100%",
          }
        ],
      },
      {
        name: "margin-left",
        expectedValues: [
          {
            title: "0px",
            left: "0%",
          },
          {
            title: "100px",
            left: "100%",
          }
        ],
      },
      {
        name: "opacity",
        expectedValues: [
          {
            title: "0",
            left: "0%",
          },
          {
            title: "1",
            left: "100%",
          }
        ],
      },
      {
        name: "text-align",
        expectedValues: [
          {
            title: "right",
            left: "0%",
          },
          {
            title: "center",
            left: "100%",
          }
        ],
      },
      {
        name: "transform",
        expectedValues: [
          {
            title: "translate(0px)",
            left: "0%",
          },
          {
            title: "translate(100px)",
            left: "100%",
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
            left: "0%",
          },
          {
            title: "1",
            left: "10%",
          },
          {
            title: "0",
            left: "13%",
          },
          {
            title: "1",
            left: "100%",
          },
        ],
      },
    ],
  }
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_multi_keyframes.html");
  await removeAnimatedElementsExcept(TEST_DATA.map(t => `.${ t.targetClass }`));
  const { animationInspector, panel } = await openAnimationInspector();

  for (const { properties, targetClass } of TEST_DATA) {
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

        info(`Checking left style in ${ hintTarget }`);
        is(markerEl.style.left, expectedValue.left,
         `left in ${ hintTarget } should be ${ expectedValue.left }`);
      }
    }
  }
});
