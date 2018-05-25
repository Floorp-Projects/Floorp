/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether keyframes progress bar moves correctly after resuming the animation.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated"]);
  const { animationInspector, panel } = await openAnimationInspector();

  const scrubberPositions = [0, 0.25, 0.5, 0.75];
  const expectedPositions = [0, 0.25, 0.5, 0.75];

  info("Check whether the keyframes progress bar position was correct");
  await assertPosition(panel, scrubberPositions, expectedPositions, animationInspector);

  info("Check whether the keyframes progress bar position was correct " +
       "after a bit time passed and resuming");
  await wait(500);
  await clickOnPauseResumeButton(animationInspector, panel);
  await assertPosition(panel, scrubberPositions, expectedPositions, animationInspector);
});

async function assertPosition(panel, scrubberPositions,
                              expectedPositions, animationInspector) {
  const areaEl = panel.querySelector(".keyframes-progress-bar-area");
  const barEl = areaEl.querySelector(".keyframes-progress-bar");
  const controllerBounds = areaEl.getBoundingClientRect();

  for (let i = 0; i < scrubberPositions.length; i++) {
    info(`Scrubber position is ${ scrubberPositions[i] }`);
    await clickOnCurrentTimeScrubberController(animationInspector,
                                               panel, scrubberPositions[i]);
    const barBounds = barEl.getBoundingClientRect();
    const barX = barBounds.x + barBounds.width / 2 - controllerBounds.x;
    const expected = controllerBounds.width * expectedPositions[i];
    ok(expected - 1 < barX && barX < expected + 1,
       `Position should apploximately be ${ expected } (x of bar is ${ barX })`);
  }
}
