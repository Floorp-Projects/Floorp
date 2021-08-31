/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

/**
 * Test for computed timing path on summary graph using given test data.
 * @param {Array} testData
 */
// eslint-disable-next-line no-unused-vars
async function testComputedTimingPath(testData) {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept(testData.map(t => `.${t.targetClass}`));
  const { panel } = await openAnimationInspector();

  for (const {
    expectedDelayPath,
    expectedEndDelayPath,
    expectedForwardsPath,
    expectedIterationPathList,
    isInfinity,
    targetClass,
  } of testData) {
    const animationItemEl = await findAnimationItemByTargetSelector(
      panel,
      `.${targetClass}`
    );

    info(`Checking computed timing path existance for ${targetClass}`);
    const computedTimingPathEl = animationItemEl.querySelector(
      ".animation-computed-timing-path"
    );
    ok(
      computedTimingPathEl,
      "The computed timing path element should be in each animation item element"
    );

    info(`Checking delay path for ${targetClass}`);
    const delayPathEl = computedTimingPathEl.querySelector(
      ".animation-delay-path"
    );

    if (expectedDelayPath) {
      ok(delayPathEl, "delay path should be existance");
      assertPathSegments(delayPathEl, true, expectedDelayPath);
    } else {
      ok(!delayPathEl, "delay path should not be existance");
    }

    info(`Checking iteration path list for ${targetClass}`);
    const iterationPathEls = computedTimingPathEl.querySelectorAll(
      ".animation-iteration-path"
    );
    is(
      iterationPathEls.length,
      expectedIterationPathList.length,
      `Number of iteration path should be ${expectedIterationPathList.length}`
    );

    for (const [j, iterationPathEl] of iterationPathEls.entries()) {
      assertPathSegments(iterationPathEl, true, expectedIterationPathList[j]);

      info(`Checking infinity ${targetClass}`);
      if (isInfinity && j >= 1) {
        ok(
          iterationPathEl.classList.contains("infinity"),
          "iteration path should have 'infinity' class"
        );
      } else {
        ok(
          !iterationPathEl.classList.contains("infinity"),
          "iteration path should not have 'infinity' class"
        );
      }
    }

    info(`Checking endDelay path for ${targetClass}`);
    const endDelayPathEl = computedTimingPathEl.querySelector(
      ".animation-enddelay-path"
    );

    if (expectedEndDelayPath) {
      ok(endDelayPathEl, "endDelay path should be existance");
      assertPathSegments(endDelayPathEl, true, expectedEndDelayPath);
    } else {
      ok(!endDelayPathEl, "endDelay path should not be existance");
    }

    info(`Checking forwards fill path for ${targetClass}`);
    const forwardsPathEl = computedTimingPathEl.querySelector(
      ".animation-fill-forwards-path"
    );

    if (expectedForwardsPath) {
      ok(forwardsPathEl, "forwards path should be existance");
      assertPathSegments(forwardsPathEl, true, expectedForwardsPath);
    } else {
      ok(!forwardsPathEl, "forwards path should not be existance");
    }
  }
}
