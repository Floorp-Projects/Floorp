/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that whether title in header of animations detail.

const TEST_DATA = [
  {
    targetClass: "cssanimation-normal",
    expectedTitle: "cssanimation — CSS Animation",
  },
  {
    targetClass: "delay-positive",
    expectedTitle: "test-delay-animation — Script Animation",
  },
  {
    targetClass: "easing-step",
    expectedTitle: "Script Animation",
  },
];

add_task(async function () {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept(TEST_DATA.map(t => `.${t.targetClass}`));
  const { animationInspector, panel } = await openAnimationInspector();

  info("Checking title in each header of animation detail");

  for (const { targetClass, expectedTitle } of TEST_DATA) {
    info(`Checking title at ${targetClass}`);
    await clickOnAnimationByTargetSelector(
      animationInspector,
      panel,
      `.${targetClass}`
    );
    const titleEl = panel.querySelector(".animation-detail-title");
    is(
      titleEl.textContent,
      expectedTitle,
      `Title of "${targetClass}" should be "${expectedTitle}"`
    );
  }
});
