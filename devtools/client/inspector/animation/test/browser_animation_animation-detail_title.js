/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that whether title in header of animations detail.

const TEST_CASES = [
  {
    target: ".cssanimation-normal",
    expectedTitle: "cssanimation - CSS Animation",
  },
  {
    target: ".delay-positive",
    expectedTitle: "test-delay-animation - Script Animation",
  },
  {
    target: ".easing-step",
    expectedTitle: "Script Animation",
  },
];

add_task(async function () {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  const { inspector, panel } = await openAnimationInspector();

  info("Checking title in each header of animation detail");

  for (const testCase of TEST_CASES) {
    info(`Checking title at ${ testCase.target }`);
    const animatedNode = await getNodeFront(testCase.target, inspector);
    await selectNodeAndWaitForAnimations(animatedNode, inspector);
    const titleEl = panel.querySelector(".animation-detail-title");
    is(titleEl.textContent, testCase.expectedTitle,
       `Title of "${ testCase.target }" should be "${ testCase.expectedTitle }"`);
  }
});
