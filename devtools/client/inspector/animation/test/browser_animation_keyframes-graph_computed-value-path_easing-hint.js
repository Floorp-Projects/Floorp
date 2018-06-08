/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following easing hint in ComputedValuePath.
// * element existence
// * path segments
// * hint text

const TEST_DATA = [
  {
    targetClass: "no-easing",
    properties: [
      {
        name: "opacity",
        expectedHints: [
          {
            hint: "linear",
            path: [
              { x: 0, y: 100 },
              { x: 500, y: 50 },
              { x: 1000, y: 0 },
            ],
          },
        ],
      },
    ],
  },
  {
    targetClass: "effect-easing",
    properties: [
      {
        name: "opacity",
        expectedHints: [
          {
            hint: "linear",
            path: [
              { x: 0, y: 100 },
              { x: 500, y: 50 },
              { x: 1000, y: 0 },
            ],
          },
        ],
      },
    ],
  },
  {
    targetClass: "keyframe-easing",
    properties: [
      {
        name: "opacity",
        expectedHints: [
          {
            hint: "steps(2)",
            path: [
              { x: 0, y: 100 },
              { x: 499, y: 100 },
              { x: 500, y: 50 },
              { x: 999, y: 50 },
              { x: 1000, y: 0 },
            ],
          },
        ],
      },
    ],
  },
  {
    targetClass: "both-easing",
    properties: [
      {
        name: "margin-left",
        expectedHints: [
          {
            hint: "steps(1)",
            path: [
              { x: 0, y: 0 },
              { x: 999, y: 0 },
              { x: 1000, y: 100 },
            ],
          },
        ],
      },
      {
        name: "opacity",
        expectedHints: [
          {
            hint: "steps(2)",
            path: [
              { x: 0, y: 100 },
              { x: 499, y: 100 },
              { x: 500, y: 50 },
              { x: 999, y: 50 },
              { x: 1000, y: 0 },
            ],
          },
        ],
      },
    ],
  },
  {
    targetClass: "narrow-keyframes",
    properties: [
      {
        name: "opacity",
        expectedHints: [
          {
            hint: "linear",
            path: [
              { x: 0, y: 0 },
              { x: 100, y: 100 },
            ],
          },
          {
            hint: "steps(1)",
            path: [
              { x: 129, y: 100 },
              { x: 130, y: 0 },
            ],
          },
          {
            hint: "linear",
            path: [
              { x: 130, y: 0 },
              { x: 1000, y: 100 },
            ],
          },
        ],
      },
    ],
  },
  {
    targetClass: "duplicate-keyframes",
    properties: [
      {
        name: "opacity",
        expectedHints: [
          {
            hint: "linear",
            path: [
              { x: 0, y: 0 },
              { x: 500, y: 100 },
            ],
          },
          {
            hint: "",
            path: [
              { x: 500, y: 100 },
              { x: 500, y: 0 },
            ],
          },
          {
            hint: "steps(1)",
            path: [
              { x: 500, y: 0 },
              { x: 999, y: 0 },
              { x: 1000, y: 100 },
            ],
          },
        ],
      },
    ],
  },
  {
    targetClass: "color-keyframes",
    properties: [
      {
        name: "color",
        expectedHints: [
          {
            hint: "ease-in",
            rect: {
              x: 0,
              height: 100,
              width: 400,
            },
          },
          {
            hint: "ease-out",
            rect: {
              x: 400,
              height: 100,
              width: 600,
            },
          },
        ],
      },
    ],
  },
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_multi_easings.html");
  await removeAnimatedElementsExcept(TEST_DATA.map(t => `.${ t.targetClass }`));
  const { animationInspector, panel } = await openAnimationInspector();

  for (const { properties, targetClass } of TEST_DATA) {
    info(`Checking keyframes graph for ${ targetClass }`);
    await clickOnAnimationByTargetSelector(animationInspector,
                                           panel, `.${ targetClass }`);

    for (const { name, expectedHints } of properties) {
      const testTarget = `${ name } in ${ targetClass }`;
      info(`Checking easing hint for ${ testTarget }`);
      info(`Checking easing hint existence for ${ testTarget }`);
      const hintEls = panel.querySelectorAll(`.${ name } .hint`);
      is(hintEls.length, expectedHints.length,
        `Count of easing hint elements of ${ testTarget } ` +
        `should be ${ expectedHints.length }`);

      for (let i = 0; i < expectedHints.length; i++) {
        const hintTarget = `hint[${ i }] of ${ testTarget }`;

        info(`Checking ${ hintTarget }`);
        const hintEl = hintEls[i];
        const expectedHint = expectedHints[i];

        info(`Checking <title> in ${ hintTarget }`);
        const titleEl = hintEl.querySelector("title");
        ok(titleEl,
          `<title> element in ${ hintTarget } should be existence`);
        is(titleEl.textContent, expectedHint.hint,
          `Content of <title> in ${ hintTarget } should be ${ expectedHint.hint }`);

        let interactionEl = null;
        let displayedEl = null;
        if (expectedHint.path) {
          info(`Checking <path> in ${ hintTarget }`);
          interactionEl = hintEl.querySelector("path");
          displayedEl = interactionEl;
          ok(interactionEl, `The <path> element  in ${ hintTarget } should be existence`);
          assertPathSegments(interactionEl, false, expectedHint.path);
        } else {
          info(`Checking <rect> in ${ hintTarget }`);
          interactionEl = hintEl.querySelector("rect");
          displayedEl = hintEl.querySelector("line");
          ok(interactionEl, `The <rect> element  in ${ hintTarget } should be existence`);
          is(interactionEl.getAttribute("x"), expectedHint.rect.x,
            `x of <rect> in ${ hintTarget } should be ${ expectedHint.rect.x }`);
          is(interactionEl.getAttribute("width"), expectedHint.rect.width,
            `width of <rect> in ${ hintTarget } should be ${ expectedHint.rect.width }`);
        }

        info(`Checking interaction for ${ hintTarget }`);
        interactionEl.scrollIntoView(false);
        const win = hintEl.ownerGlobal;
        // Mouse over the pathEl.
        ok(isStrokeChangedByMouseOver(interactionEl, displayedEl, win),
          `stroke-opacity of hintEl for ${ hintTarget } should be 1 ` +
          "while mouse is over the element");
        // Mouse out from pathEl.
        EventUtils.synthesizeMouse(panel.querySelector(".animation-toolbar"),
                                   0, 0, { type: "mouseover" }, win);
        is(win.getComputedStyle(displayedEl).strokeOpacity, 0,
          `stroke-opacity of hintEl for ${ hintTarget } should be 0 ` +
          "while mouse is out from the element");
      }
    }
  }
});

function isStrokeChangedByMouseOver(mouseoverEl, displayedEl, win) {
  const boundingBox = mouseoverEl.getBoundingClientRect();
  const x = boundingBox.width / 2;

  for (let y = 0; y < boundingBox.height; y++) {
    EventUtils.synthesizeMouse(mouseoverEl, x, y, { type: "mouseover" }, win);

    if (win.getComputedStyle(displayedEl).strokeOpacity == 1) {
      return true;
    }
  }

  return false;
}
