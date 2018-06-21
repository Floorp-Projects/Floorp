/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test following animated property list test.
// 1. Existence for animated property list.
// 2. Number of animated property item.

const TEST_DATA = [
  {
    targetClass: "animated",
    expectedNumber: 1,
  },
  {
    targetClass: "compositor-notall",
    expectedNumber: 3,
  },
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept(TEST_DATA.map(t => `.${ t.targetClass }`));
  const { animationInspector, panel } = await openAnimationInspector();

  info("Checking animated property list and items existence at initial");
  ok(!panel.querySelector(".animated-property-list"),
     "The animated-property-list should not be in the DOM at initial");

  for (const { targetClass, expectedNumber } of TEST_DATA) {
    info(`Checking animated-property-list and items existence at ${ targetClass }`);
    await clickOnAnimationByTargetSelector(animationInspector,
                                           panel, `.${ targetClass }`);
    ok(panel.querySelector(".animated-property-list"),
      `The animated-property-list should be in the DOM at ${ targetClass }`);
    const itemEls =
      panel.querySelectorAll(".animated-property-list .animated-property-item");
    is(itemEls.length, expectedNumber,
       `The number of animated-property-list should be ${ expectedNumber } ` +
       `at ${ targetClass }`);
  }
});
