/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Also checks that the button goes to the right state when the scrubber has
// reached the end of the timeline: continues to be in playing mode for infinite
// animations, goes to paused mode otherwise.
// And test that clicking the button once the scrubber has reached the end of
// the timeline does the right thing.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");

  let {panel, inspector} = await openAnimationInspector();
  let btn = panel.playTimelineButtonEl;

  // For a finite animation, once the scrubber reaches the end of the timeline, the pause
  // button should go back to paused mode.
  info("Select a finite animation and wait for the animation to complete");
  await selectNodeAndWaitForAnimations(".negative-delay", inspector);

  await reloadTab(inspector);

  if (!btn.classList.contains("paused")) {
    await waitForButtonPaused(btn);
  }

  ok(btn.classList.contains("paused"),
     "The button is in paused state once finite animations are done");
  await assertScrubberMoving(panel, false);

  info("Click again on the button to play the animation from the start again");
  await clickTimelinePlayPauseButton(panel);

  ok(!btn.classList.contains("paused"),
     "Clicking the button once finite animations are done should restart them");
  await assertScrubberMoving(panel, true);
});

function waitForButtonPaused(btn) {
  return new Promise(resolve => {
    let observer = new btn.ownerDocument.defaultView.MutationObserver(mutations => {
      for (let mutation of mutations) {
        if (mutation.type === "attributes" &&
            mutation.attributeName === "class" &&
            !mutation.oldValue.includes("paused") &&
            btn.classList.contains("paused")) {
          observer.disconnect();
          resolve();
        }
      }
    });
    observer.observe(btn, { attributes: true, attributeOldValue: true });
  });
}
